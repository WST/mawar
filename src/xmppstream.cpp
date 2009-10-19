
#include <xmppstream.h>
#include <xmppserver.h>
#include <virtualhost.h>
#include <tagbuilder.h>
#include <nanosoft/base64.h>

// for debug only
#include <string>
#include <iostream>

using namespace std;
using namespace nanosoft;

/**
* Конструктор потока
*/
XMPPStream::XMPPStream(XMPPServer *srv, int sock):
	AsyncXMLStream(sock), XMLWriter(1024),
	server(srv), vhost(0),
	state(init), depth(0)
{
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
	cerr << "[XMPPStream]: peer shutdown connection" << endl;
	if ( state != terminating ) {
		AsyncXMLStream::onShutdown();
		//server->onOffline(this);
		server->getHostByName(client_jid.hostname())->onOffline(this);
		XMLWriter::flush();
	}
	server->daemon->removeObject(this);
	shutdown(READ | WRITE);
	delete this;
}

/**
* Завершить сессию
*/
void XMPPStream::terminate()
{
	cerr << "[XMPPStream]: terminating connection..." << endl;
	
	switch ( state )
	{
	case terminating:
		return;
	case authorized:
		//server->onOffline(this);
		break;
	}
	
	state = terminating;
	
	endElement("stream:stream");
	flush();
	
	shutdown(WRITE);
}

/**
* Сигнал завершения работы
*
* Объект должен закрыть файловый дескриптор
* и освободить все занимаемые ресурсы
*/
void XMPPStream::onTerminate()
{
	terminate();
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
		builder->startElement(name, attributes, depth);
	break;
	default: // добавить тег в станзу
		builder->startElement(name, attributes, depth);
	}
}

/**
* Обработчик символьных данных
*/
void XMPPStream::onCharacterData(const std::string &cdata)
{
	builder->characterData(cdata);
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
		builder->endElement(name);
		Stanza s = builder->fetchResult();
		onStanza(s);
		delete s; // Внимание — станза удаляется здесь
		break;
	}
	default:
		builder->endElement(name);
	}
	depth --;
}

/**
* Обработчик станз
*/
void XMPPStream::onStanza(Stanza stanza)
{
	cout << "stanza: " << stanza->name() << endl;
	if (stanza->name() == "iq") onIqStanza(stanza);
	else if (stanza->name() == "auth") onAuthStanza(stanza);
	else if (stanza->name() == "response" ) onResponseStanza(stanza);
	else if (stanza->name() == "message") onMessageStanza(stanza);
	else if (stanza->name() == "presence") onPresenceStanza(stanza);
	else ; // ...
}

/**
* Обработчик авторизации
*/
void XMPPStream::onAuthStanza(Stanza stanza)
{
	sasl = server->start("xmpp", client_jid.hostname(), stanza->getAttribute("mechanism"));
	onSASLStep(string());
}

/**
* Обработка этапа авторизации SASL
*/
void XMPPStream::onSASLStep(const std::string &input)
{
	string output;
	switch ( server->step(sasl, input, output) )
	{
	case SASLServer::ok:
		client_jid.setUsername(server->getUsername(sasl));
		server->GSASLServer::close(sasl);
		startElement("success");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
		endElement("success");
		flush();
		resetWriter();
		state = authorized;
		depth = 1; // после выхода из onAuthStanza/onStanza() будет стандартный depth--
		resetParser();
		break;
	case SASLServer::next:
		startElement("challenge");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
			characterData(base64_encode(output));
		endElement("challenge");
		flush();
		break;
	case SASLServer::error:
		server->GSASLServer::close(sasl);
		startElement("failure");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
			addElement("temporary-auth-failure", "");
		endElement("failure");
		flush();
		break;
	}
}

/**
* Обработчик авторизации: ответ клиента
*/
void XMPPStream::onResponseStanza(Stanza stanza)
{
	onSASLStep(base64_decode(stanza));
}

/**
* Обработчик iq-станзы
*/
void XMPPStream::onIqStanza(Stanza stanza)
{
	if(stanza->hasChild("bind") && (stanza.type() == "set" || stanza.type() == "get")) {
		client_jid.setResource(stanza.type() == "set" ? string(stanza["bind"]["resource"]) : "foo");
		startElement("iq");
			setAttribute("type", "result");
			setAttribute("id", stanza->getAttribute("id"));
			startElement("bind");
				setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-bind");
				startElement("jid");
					characterData(jid().full());
				endElement("jid");
			endElement("bind");
		endElement("iq");
		flush();
		return;
	}
	if(stanza->hasChild("session") && stanza.type() == "set") {
		startElement("iq");
			setAttribute("id", stanza->getAttribute("id"));
			setAttribute("type", "result");
			startElement("session");
				setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-session");
			endElement("session");
		endElement("iq");
		flush();
		server->getHostByName(client_jid.hostname())->onOnline(this);
		return;
	}
	
	VirtualHost *s = server->getHostByName(stanza.to().hostname());
	if(s != 0) { // iq-запрос к виртуальному узлу
		s->handleIq(stanza);
	} else {
		// iq-запрос «наружу»
	}
	
}

ClientPresence XMPPStream::presence() {
	return client_presence;
}

void XMPPStream::onMessageStanza(Stanza stanza) {
	stanza->setAttribute("from", jid().full());
	
	VirtualHost *s = server->getHostByName(stanza.to().hostname());
	if(s != 0) { // Сообщение к виртуальному узлу
		s->handleMessage(stanza);
	} else {
		// Сообщение «наружу» (S2S)
	}
}

void XMPPStream::onPresenceStanza(Stanza stanza) {
	stanza->setAttribute("from", client_jid.full());
	
	client_presence.priority = atoi(stanza->getChildValue("priority", "0").c_str()); // TODO
	client_presence.status_text = stanza->getChildValue("status", "");
	client_presence.setShow(stanza->getChildValue("show", "Available"));
	
	VirtualHost *s = server->getHostByName(stanza.to().hostname());
	if(s != 0) { // Сообщение к виртуальному узлу
		s->handlePresence(stanza);
	} else {
		// Присутствие «наружу» (S2S)
	}
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
	
	if ( state == init )
	{
		vhost = server->getHostByName(attributes.find("to")->second);
		if ( vhost == 0 ) {
			Stanza error = Stanza::streamError("host-unknown", "Неизвестный хост", "ru");
			sendStanza(error);
			delete error;
			terminate();
			return;
		}
	}
	
	startElement("stream:features");
	if ( state == init )
	{
		client_jid.setHostname(vhost->hostname());
		startElement("mechanisms");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
			SASLServer::mechanisms_t list = server->getMechanisms();
			for(SASLServer::mechanisms_t::const_iterator pos = list.begin(); pos != list.end(); ++pos)
			{
				addElement("mechanism", *pos);
			}
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
	cerr << "session closed" << endl;
	endElement("stream:stream");
}

/**
* JID потока
*/
JID XMPPStream::jid()
{
	return client_jid;
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
}

bool XMPPStream::sendStanza(Stanza stanza) {
	if ( state == terminating ) {
		cerr << "XMPPStream::sendStanza(): connection terminating..." << endl;
		return false;
	}
	sendTag(stanza);
	flush();
	return true;
}
