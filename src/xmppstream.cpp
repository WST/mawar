
#include <xmppstream.h>
#include <xmppserver.h>
#include <virtualhost.h>
#include <db.h>
#include <tagbuilder.h>
#include <nanosoft/base64.h>

// for debug only
#include <string>
#include <iostream>
#include <stdio.h>

using namespace std;
using namespace nanosoft;

/**
* Чисто тегов выделенно перед обработкой станзы
*/
unsigned XMPPStream::tags_created_before_stanza = 0;

/**
* Число тегов уничтожено перед обработкой станзы
*/
unsigned XMPPStream::tags_destroyed_before_stanza = 0;

/**
* Число тегов выделеных для обработки станзы
*/
unsigned XMPPStream::tags_created_for_stanza = 0;

/**
* Число тегов удаленных при обработки станзы
*/
unsigned XMPPStream::tags_destroyed_for_stanza = 0;

/**
* Максимальное число тегов выделенных для обработки 1 станзы
*/
unsigned XMPPStream::tags_max_created_for_stanza = 0;

/**
* Число утеряных тегов при обработке станзы
*/
unsigned XMPPStream::tags_leak_for_stanza = 0;

/**
* Всего утеряных тегов
*/
unsigned XMPPStream::tags_leak = 0;

/**
* Число обработанных станз
*/
unsigned XMPPStream::stanza_count = 0;

/**
* Конструктор потока
*/
XMPPStream::XMPPStream(XMPPServer *srv, int sock):
	AsyncXMLStream(sock), XMLWriter(1024),
	server(srv), depth(0), want_write(0), disable_write(0)
{
}

/**
* Деструктор потока
*/
XMPPStream::~XMPPStream()
{
	server->daemon->cleanup(fd);
}

/**
* Вернуть маску ожидаемых событий
*/
uint32_t XMPPStream::getEventsMask()
{
	uint32_t mask = EPOLLIN | EPOLLRDHUP | EPOLLONESHOT | EPOLLHUP | EPOLLERR | EPOLLPRI;
	if ( want_write && ! disable_write ) mask |= EPOLLOUT;
	return mask;
}

/**
* Запись XML
*/
void XMPPStream::onWriteXML(const char *data, size_t len)
{
	if ( server->daemon->put(fd, data, len) )
	{
		want_write = true;
		server->daemon->modifyObject(this);
	}
	else onError("write buffer fault");
}

/**
* Событие готовности к записи
*
* Вызывается, когда в поток готов принять
* данные для записи без блокировки
*/
void XMPPStream::onWrite()
{
	//cerr << "not implemented XMPPStream::onWrite()" << endl;
	want_write = ! server->daemon->push(fd);
}

/**
* Обработчик открытия тега
*/
void XMPPStream::onStartElement(const std::string &name, const attributtes_t &attributes)
{
	depth ++;
	switch ( depth )
	{
	case 1:
		onStartStream(name, attributes);
		break;
	case 2: // начало станзы
		builder.startElement(name, attributes, depth);
	break;
	default: // добавить тег в станзу
		builder.startElement(name, attributes, depth);
	}
}

/**
* Обработчик символьных данных
*/
void XMPPStream::onCharacterData(const std::string &cdata)
{
	builder.characterData(cdata);
}

/**
* Обработчик закрытия тега
*/
void XMPPStream::onEndElement(const std::string &name)
{
	switch (depth)
	{
	case 1:
		onEndStream();
		break;
	case 2: {
		builder.endElement(name);
		Stanza stanza = builder.fetchResult();
		
#ifdef DUMP_IO
		fprintf(stdout, "DUMP STANZA[%d]: \033[22;31m%s\033[0m\n", fd, stanza->asString().c_str());
#endif
		
		onBeforeStanza();
		onStanza(stanza);
		onAfterStanza();
		delete stanza; // Внимание — станза удаляется здесь
		break;
	}
	default:
		builder.endElement(name);
	}
	depth --;
}

/**
* Обработчик ошибок парсера
*/
void XMPPStream::onParseError(const char *message)
{
	printf("[XMPPStream: %d] parse error: %s\n", fd, message);
	// TODO something...
	server->daemon->removeObject(this);
}

/**
* Вызывается после получения, но до обработки станзы
*/
void XMPPStream::onBeforeStanza()
{
	tags_created_before_stanza = ATXmlTag::getTagsCreated();
	tags_destroyed_before_stanza = ATXmlTag::getTagsDestroyed();
	
	printf("onBeforeStanza: tag_count: %d, max_count: %d\n", ATXmlTag::getTagsCount(), ATXmlTag::getTagsMaxCount());
}

/**
* Вызывается после завершение обработки станзы
*/
void XMPPStream::onAfterStanza()
{
	tags_created_for_stanza = ATXmlTag::getTagsCreated() - tags_created_before_stanza;
	tags_destroyed_for_stanza = ATXmlTag::getTagsDestroyed() - tags_destroyed_before_stanza;
	
	if ( tags_created_for_stanza > tags_max_created_for_stanza )
	{
		tags_max_created_for_stanza = tags_created_for_stanza;
	}
	
	tags_leak_for_stanza = tags_created_for_stanza - tags_destroyed_for_stanza;
	tags_leak += tags_leak_for_stanza;
	stanza_count++;
	
	printf("onAfterStanza: tags_created: %d, tags_destroyed: %d, tags_leak: %d\n",
		tags_created_for_stanza,
		tags_destroyed_for_stanza,
		tags_leak_for_stanza
	);
}

/**
* Отправить станзу в поток (thread-safe)
* @param stanza станза
* @return TRUE - станза отправлена (или буферизована для отправки), FALSE что-то не получилось
*/
bool XMPPStream::sendStanza(Stanza stanza)
{
	string data = stanza->asString();
	//fprintf(stdout, "[XMPPStream: %d] sendStanza(\033[22;34m%s\033[0m)\n", fd, data.c_str());
	if ( server->daemon->put(fd, data.c_str(), data.length()) )
	{
		want_write = true;
		server->daemon->modifyObject(this);
	}
	else onError("[XMPPStream: %d] sendStanza() fault");
}

/**
* Отправить станзу как есть без обработки (сжатия, шифрования и т.п.)
*/
void XMPPStream::sendStanzaRaw(Stanza stanza)
{
	string data = stanza->asString();
	//fprintf(stdout, "[XMPPStream: %d] sendStanza(\033[22;34m%s\033[0m)\n", fd, data.c_str());
	if ( server->daemon->putRaw(fd, data.c_str(), data.length()) )
	{
		want_write = true;
		server->daemon->modifyObject(this);
	}
	else onError("[XMPPStream: %d] sendStanza() fault");
}
