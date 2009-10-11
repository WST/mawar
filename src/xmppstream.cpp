
#include <xmppstream.h>
#include <xmppserver.h>

// for debug only
#include <string>
#include <iostream>

using namespace std;

/**
* Конструктор потока
*/
XMPPStream::XMPPStream(XMPPServer *srv, int sock): AsyncXMLStream(sock), server(srv), XMLWriter(1024)
{
	deep = 0;
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
	//cout << string(data, len) << endl;
	int r = write(data, len);
	if ( r != len ) onError("write fault");
	cout << "written: " << string(data, len) << endl;
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
	deep ++;
	switch ( deep )
	{
	case 1:
		onStartStream(name, attributes);
		break;
	default:
		cout << "onStartElement(" << name << ")" << endl;
	}
}

/**
* Обработчик символьных данных
*/
void XMPPStream::onCharacterData(const std::string &cdata)
{
	cout << "cdata: " << cdata << endl;
}

/**
* Обработчик закрытия тега
*/
void XMPPStream::onEndElement(const std::string &name)
{
	switch (deep)
	{
	case 1:
		onEndStream();
		break;
	default:
		cout << "onEndElement(" << name << ")" << endl;
	}
	deep --;
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
	setAttribute("id", "123456");
	setAttribute("from", "localhost");
	setAttribute("version", "1.0");
	setAttribute("xml:lang", "en");
	
	startElement("stream:features");
		startElement("mechanisms");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
			addElement("mechanism", "PLAIN");
		endElement("mechanisms");
	endElement("stream:features");
/*
<starttls xmlns="urn:ietf:params:xml:ns:xmpp-tls"/>
<compression xmlns="http://jabber.org/features/compress">
<method>zlib</method>
</compression>
<mechanisms xmlns="urn:ietf:params:xml:ns:xmpp-sasl">
<mechanism>DIGEST-MD5</mechanism>
<mechanism>PLAIN</mechanism>
</mechanisms>
<register xmlns="http://jabber.org/features/iq-register"/>
</stream:features>
*/
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
