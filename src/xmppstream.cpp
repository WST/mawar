
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
}

/**
* Запись XML
*/
void XMPPStream::onWriteXML(const char *data, size_t len)
{
	int r = write(data, len);
	if ( r != len ) onError("write fault");
	printf("#%d written: \033[22;34m%s\033[0m\n", getWorkerId(), string(data, len).c_str());
}

/**
* Событие готовности к записи
*
* Вызывается, когда в поток готов принять
* данные для записи без блокировки
*/
void XMPPStream::onWrite()
{
	cerr << "not implemented XMPPStream::onWrite()" << endl;
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
	server->daemon->removeObject(this);
	delete this;
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
bool XMPPStream::sendStanza(Stanza stanza) {
	mutex.lock();
		sendTag(stanza);
		flush();
	mutex.unlock();
	return true;
}
