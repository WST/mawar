#include <xmppclient.h>
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
XMPPClient::XMPPClient(XMPPServer *srv, int sock):
	XMPPStream(srv, sock), vhost(0),
	state(init), initialPresenceSent(false)
{
}

/**
* Деструктор потока
*/
XMPPClient::~XMPPClient()
{
}

/**
* Событие закрытие соединения
*
* Вызывается если peer закрыл соединение
*/
void XMPPClient::onShutdown()
{
	cerr << "[XMPPStream]: peer shutdown connection" << endl;
	if ( state != terminating ) {
		AsyncXMLStream::onShutdown();
		//server->onOffline(this);
		vhost->onOffline(this);
		XMLWriter::flush();
	}
	server->daemon->removeObject(this);
	shutdown(READ | WRITE);
	delete this;
	cerr << "[XMPPStream]: onShutdown leave" << endl;
}

/**
* Завершить сессию
*
* thread-safe
*/
void XMPPClient::terminate()
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
	
	mutex.lock();
		if ( state != terminating ) {
			state = terminating;
			endElement("stream:stream");
			flush();
			shutdown(WRITE);
		}
	mutex.unlock();
}

/**
* Сигнал завершения работы
*
* Объект должен закрыть файловый дескриптор
* и освободить все занимаемые ресурсы
*/
void XMPPClient::onTerminate()
{
	terminate();
}

/**
* Обработчик станз
*/
void XMPPClient::onStanza(Stanza stanza)
{
	fprintf(stderr, "#%d stanza: %s\n", getWorkerId(), stanza->name().c_str());
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
void XMPPClient::onAuthStanza(Stanza stanza)
{
	sasl = vhost->start("xmpp", vhost->hostname(), stanza->getAttribute("mechanism"));
	onSASLStep(string());
}

/**
* Обработка этапа авторизации SASL
*/
void XMPPClient::onSASLStep(const std::string &input)
{
	string output;
	switch ( vhost->step(sasl, input, output) )
	{
	case SASLServer::ok:
		client_jid.setUsername(vhost->getUsername(sasl));
		vhost->GSASLServer::close(sasl);
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
		vhost->GSASLServer::close(sasl);
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
void XMPPClient::onResponseStanza(Stanza stanza)
{
	onSASLStep(base64_decode(stanza));
}

/**
* Обработчик iq-станзы
*/
void XMPPClient::onIqStanza(Stanza stanza) {
	stanza.setFrom(client_jid);
	
	if(stanza->hasChild("bind") && (stanza.type() == "set" || stanza.type() == "get")) {
		client_jid.setResource(stanza.type() == "set" ? string(stanza["bind"]["resource"]) : "foo");
		Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("type", "result");
			iq->setAttribute("id", stanza->getAttribute("id"));
			TagHelper bind = iq["bind"];
				bind->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-bind");
				bind["jid"] = jid().full();
		sendStanza(iq);
		delete iq;
		vhost->onOnline(this);
		return;
	}
	
	if(stanza->hasChild("session") && stanza.type() == "set") {
		Stanza iq = new ATXmlTag("iq");
			if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
			iq->setAttribute("type", "result");
			iq["session"]->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-session");
		
		sendStanza(iq);
		return;
	}
	
	if(!stanza->hasAttribute("to")) {
		vhost->handleIq(stanza);
		return;
	}
	
	server->routeStanza(stanza.to().hostname(), stanza);
}

ClientPresence XMPPClient::presence() {
	return client_presence;
}

void XMPPClient::onMessageStanza(Stanza stanza) {
	stanza.setFrom(client_jid);
	server->routeStanza(stanza.to().hostname(), stanza);
}

void XMPPClient::onPresenceStanza(Stanza stanza) {
	stanza->setAttribute("from", client_jid.full());
	
	if ( stanza->hasAttribute("to") )
	{
		// доставить личный презенс
		server->routeStanza(stanza.to().hostname(), stanza);
	}
	else
	{
		client_presence.priority = atoi(stanza->getChildValue("priority", "0").c_str()); // TODO
		client_presence.status_text = stanza->getChildValue("status", "");
		client_presence.setShow(stanza->getChildValue("show", "Available"));
		
		// Разослать этот presence контактам ростера
		if ( initialPresenceSent || stanza->hasAttribute("type") ) {
			vhost->broadcastPresence(stanza);
		} else {
			vhost->initialPresence(stanza);
			initialPresenceSent = true;
		}
	}
}

/**
* Событие: начало потока
*/
void XMPPClient::onStartStream(const std::string &name, const attributes_t &attributes)
{
	fprintf(stderr, "#%d new stream to %s\n", getWorkerId(), attributes.find("to")->second.c_str());
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
		vhost = dynamic_cast<VirtualHost*>(server->getHostByName(attributes.find("to")->second));
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
			SASLServer::mechanisms_t list = vhost->getMechanisms();
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
void XMPPClient::onEndStream()
{
	cerr << "session closed" << endl;
	endElement("stream:stream");
}

/**
* JID потока
*/
JID XMPPClient::jid() const
{
	return client_jid;
}
