
#include <xmppstream.h>
#include <xmppserver.h>
#include <virtualhost.h>
#include <db.h>
#include <tagbuilder.h>
#include <nanosoft/base64.h>
#include <stdlib.h>

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
	server(srv), depth(0)
{
}

/**
* Деструктор потока
*/
XMPPStream::~XMPPStream()
{
	server->daemon->cleanup(getFd());
}

/**
* Вернуть маску ожидаемых событий
*/
uint32_t XMPPStream::getEventsMask()
{
	uint32_t mask = EPOLLIN | EPOLLRDHUP | EPOLLONESHOT | EPOLLHUP | EPOLLERR | EPOLLPRI;
	return mask;
}

/**
* Запись XML
*/
void XMPPStream::onWriteXML(const char *data, size_t len)
{
	if ( ! put(data, len) )
	{
		onError("write buffer fault");
	}
}

/**
* Открытие потока
*/
void XMPPStream::startStream(const std::string &name, const attributtes_t &attributes)
{
	attributes_t::const_iterator it = attributes.find("version");
	ver_major = 0;
	ver_minor = 9;
	if ( it != attributes.end() )
	{
		string version = it->second;
		int pos = version.find(".");
		if ( pos != -1 )
		{
			ver_major = atoi(version.substr(0, pos).c_str());
			ver_minor = atoi(version.substr(pos + 1).c_str());
		}
	}
	
	onStartStream(name, attributes);
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
		startStream(name, attributes);
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
		fprintf(stdout, "DUMP STANZA[%d]: \033[22;31m%s\033[0m\n", getFd(), stanza->asString().c_str());
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
	printf("[XMPPStream: %d] parse error: %s\n", getFd(), message);
	// TODO something...
	server->daemon->removeObject(this);
}

/**
* Вызывается после получения, но до обработки станзы
*/
void XMPPStream::onBeforeStanza()
{
	tags_created_before_stanza = XmlTag::getTagsCreated();
	tags_destroyed_before_stanza = XmlTag::getTagsDestroyed();
	
	//printf("onBeforeStanza: tag_count: %d, max_count: %d\n", XmlTag::getTagsCount(), XmlTag::getTagsMaxCount());
}

/**
* Вызывается после завершение обработки станзы
*/
void XMPPStream::onAfterStanza()
{
	tags_created_for_stanza = XmlTag::getTagsCreated() - tags_created_before_stanza;
	tags_destroyed_for_stanza = XmlTag::getTagsDestroyed() - tags_destroyed_before_stanza;
	
	if ( tags_created_for_stanza > tags_max_created_for_stanza )
	{
		tags_max_created_for_stanza = tags_created_for_stanza;
	}
	
	tags_leak_for_stanza = tags_created_for_stanza - tags_destroyed_for_stanza;
	tags_leak += tags_leak_for_stanza;
	stanza_count++;
	/*
	printf("onAfterStanza: tags_created: %d, tags_destroyed: %d, tags_leak: %d\n",
		tags_created_for_stanza,
		tags_destroyed_for_stanza,
		tags_leak_for_stanza
	);
	*/
}

/**
* Отправить станзу в поток (thread-safe)
* @param stanza станза
* @return TRUE - станза отправлена (или буферизована для отправки), FALSE что-то не получилось
* TODO: asString() тут очень накладен и не нужен
*/
bool XMPPStream::sendStanza(Stanza stanza)
{
	string data = stanza->asString();
	//fprintf(stdout, "[XMPPStream: %d] sendStanza(\033[22;34m%s\033[0m)\n", fd, data.c_str());
	if ( ! put(data.c_str(), data.length()) )
	{
		onError("[XMPPStream: %d] sendStanza() fault");
		return false;
	}
	return true;
}
