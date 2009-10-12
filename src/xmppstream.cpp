
#include <xmppstream.h>
#include <xmppserver.h>
#include <tagbuilder.h>
#include <nanosoft/base64.h>

// for debug only
#include <string>
#include <iostream>

using namespace std;

/**
* Конструктор потока
*/
XMPPStream::XMPPStream(XMPPServer *srv, int sock): AsyncXMLStream(sock), server(srv), XMLWriter(1024)
{
	depth = 0;
	builder = new ATTagBuilder();
}

/**
* Деструктор потока
*/
XMPPStream::~XMPPStream()
{
	delete builder;
}

/**
* Запись XML
*/
void XMPPStream::onWriteXML(const char *data, size_t len)
{
	int r = write(data, len);
	if ( r != len ) onError("write fault");
	cout << "written: \033[22;34m" << string(data, len) << "\033[0m\n";
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
* Событие закрытие соединения
*
* Вызывается если peer закрыл соединение
*/
void XMPPStream::onShutdown()
{
	AsyncXMLStream::onShutdown();
	XMLWriter::flush();
	cerr << "[TestStream]: peer shutdown connection" << endl;
	if ( shutdown(fd, SHUT_RDWR) != 0 )
	{
		stderror();
	}
	server->daemon->removeObject(this);
	delete this;
}

/**
* Обработчик открытия тега
*/
void XMPPStream::onStartElement(const std::string &name, const attributtes_t &attributes)
{
	depth ++;
	cout << "onStartElement: " << name << " depth: " << depth << endl;
	switch ( depth )
	{
	case 1:
		onStartStream(name, attributes);
		break;
	case 2: // начало станзы
		builder->startElement(name, attributes, depth);
	break;
	default: // добавить тег в станзу
		builder->startElement(name, attributes, depth);
		cout << "onStartElement(" << name << ")" << endl;
	}
}

/**
* Обработчик символьных данных
*/
void XMPPStream::onCharacterData(const std::string &cdata)
{
	builder->characterData(cdata);
	cout << "cdata: " << cdata << endl;
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
	case 2:
		builder->endElement(name);
		onStanza(builder->fetchResult());
		break;
	default:
		builder->endElement(name);
		cout << "onEndElement(" << name << ")" << endl;
	}
	depth --;
}

/**
* Обработчик станз
*/
void XMPPStream::onStanza(ATXmlTag *tag)
{
	cout << "stanza: " << tag->name() << endl;
	if ( tag->name() == "iq" ) onIqStanza(tag);
	else if ( tag->name() == "auth" ) onAuthStanza(tag);
	else ; // ...
}

/**
* Обработчик авторизации
*/
void XMPPStream::onAuthStanza(ATXmlTag *tag)
{
	string password = nanosoft::base64_decode(tag->getCharacterData());
	cout << "auth password: " << password << endl;
	
	startElement("success");
		setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
	endElement("success");
	flush();
	
	resetWriter();
	state = authorized;
	depth = 1; // после выхода из onAuthStanza/onStanza() будет стандартный depth--
	resetParser();
}

/**
* Обработчик iq-станзы
*/
void XMPPStream::onIqStanza(ATXmlTag *iq)
{
	cout << "iq: " << iq->asString() << endl;
	startElement("iq");
		setAttribute("type", "result");
		setAttribute("id", iq->getAttribute("id"));
		startElement("bind");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-bind");
			startElement("jid");
				characterData("alex@localhost/bar");
			endElement("jid");
		endElement("bind");
	endElement("iq");
	flush();
}


/**
* Событие: начало потока
*/
void XMPPStream::onStartStream(const std::string &name, const attributes_t &attributes)
{
	cout << "new stream" << endl;
	initXML();
	startElement("stream:stream");
	setAttribute("xmlns", "jabber:client");
	setAttribute("xmlns:stream", "http://etherx.jabber.org/streams");
	setAttribute("id", "123456"); // Требования к id — непредсказуемость и уникальность
	setAttribute("from", "localhost");
	setAttribute("version", "1.0");
	setAttribute("xml:lang", "en");
	
	startElement("stream:features");
	if ( state == init )
	{
		startElement("mechanisms");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
			addElement("mechanism", "PLAIN");
		endElement("mechanisms");
		startElement("register");
			setAttribute("xmlns", "http://jabber.org/features/iq-register");
		endElement("register");
	}
	else
	{
		startElement("bind");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-bind");
		endElement("bind");
		/*
		startElement("session");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-session");
		endElement("session");
		*/
	}
	endElement("stream:features");
	flush();
}

/**
* Событие: конец потока
*/
void XMPPStream::onEndStream()
{
	cout << "session closed" << endl;
	endElement("stream:stream");
}
