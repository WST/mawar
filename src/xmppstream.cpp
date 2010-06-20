
#include <xmppstream.h>
#include <xmppserver.h>
#include <stanzabuffer.h>
#include <virtualhost.h>
#include <db.h>
#include <tagbuilder.h>
#include <nanosoft/base64.h>
#include <stanzabuffer.h>

// for debug only
#include <string>
#include <iostream>
#include <stdio.h>

using namespace std;
using namespace nanosoft;

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
	server->buffer->cleanup(fd);
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
	std::string s(data, len);
	fprintf(stdout, "[XMPPStream: %d] onWriteXML deprecated: \033[22;34m%s\033[0m\n", fd, s.c_str());
	if ( server->buffer->put(fd, data, len) )
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
	want_write = ! server->buffer->push(fd);
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
		Stanza s = builder.fetchResult();
		//fprintf(stdout, "[XMPPStream: %d] onStanza(\033[22;31m%s\033[0m)\n", fd, s->asString().c_str());
		onStanza(s);
		delete s; // Внимание — станза удаляется здесь
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
	fprintf(stderr, "#%d: [XMPPStream: %d] parse error: %s\n", getWorkerId(), fd, message);
	// TODO something...
	server->daemon->removeObject(this);
}

void XMPPStream::sendTag(ATXmlTag * tag) {
	startElement(tag->name());
	attributes_t attributes = tag->getAttributes();
	for(attributes_t::iterator it = attributes.begin(); it != attributes.end(); it++) {
		setAttribute(it->first, it->second);
	}
	nodes_list_t nodes = tag->getChildNodes();
	for(nodes_list_t::iterator it = nodes.begin(); it != nodes.end(); it++) {
		if((*it)->type == TTag) {
			sendTag((*it)->tag);
		} else {
			characterData((*it)->cdata);
		}
	}
	endElement(tag->name());
	// send(tag->asString()); — так будет куда проще…
	// TODO: shade, что-то меня не очень такая реализация радует
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
	if ( server->buffer->put(fd, data.c_str(), data.length()) )
	{
		want_write = true;
		server->daemon->modifyObject(this);
	}
	else onError("[XMPPStream: %d] sendStanza() fault");
}

