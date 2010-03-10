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
	state(init), available(false), use_roster(false)
{
}

/**
* Деструктор потока
*/
XMPPClient::~XMPPClient()
{
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void XMPPClient::onTerminate()
{
	fprintf(stderr, "#%d: [XMPPClient: %d] onTerminate\n", getWorkerId(), fd);
	
	// if ( state == authorized ) server->onOffline(this);
	
	mutex.lock();
		endElement("stream:stream");
		flush();
		shutdown(WRITE);
	mutex.unlock();
}

/**
* Обработчик станз
*/
void XMPPClient::onStanza(Stanza stanza)
{
	fprintf(stderr, "#%d: [XMPPClient: %d] stanza: %s\n", getWorkerId(), fd, stanza->name().c_str());
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
		user_id = vhost->getUserId(client_jid.username());
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
	
	TagHelper query = stanza->firstChild("query");
	if ( query ) {
		if(query->getAttribute("xmlns") == "jabber:iq:register") {
			vhost->handleRegisterIq(this, stanza);
		}
		
		if ( query->getAttribute("xmlns") == "jabber:iq:roster" ) {
			vhost->handleRosterIq(this, stanza);
			return;
		}
	}
	
	if ( ! stanza->hasAttribute("to") ) {
		vhost->routeStanza(stanza);
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

/**
* RFC 3921 (5.1.1) Initial Presence
*/
void XMPPClient::handleInitialPresence(Stanza stanza)
{
	fprintf(stderr, "#%d: [XMPPClient: %d] RFC 3921 (5.1.1) Initial Presence\n", getWorkerId(), fd);
	available = true;
	handlePresenceProbes();
	handlePresenceBroadcast(stanza);
}

/**
* RFC 3921 (5.1.2) Presence Broadcast
*/
void XMPPClient::handlePresenceBroadcast(Stanza stanza)
{
	fprintf(stderr, "#%d: [XMPPClient: %d] RFC 3921 (5.1.2) Presence Broadcast\n", getWorkerId(), fd);
	
	client_presence.priority = atoi(stanza->getChildValue("priority", "0").c_str()); // TODO
	client_presence.status_text = stanza->getChildValue("status", "");
	client_presence.setShow(stanza->getChildValue("show", "Available"));
	
	DB::result r = vhost->db.query("SELECT contact_jid FROM roster JOIN users ON roster.id_user = users.id_user WHERE user_login = %s AND contact_subscription IN ('F', 'B')", vhost->db.quote(client_jid.username()).c_str());
	for(; ! r.eof(); r.next()) {
		stanza->setAttribute("to", r["contact_jid"]);
		server->routeStanza(stanza.to().hostname(), stanza);
	}
	r.free();
	
	// TODO broadcast presence to other yourself resources
}

/**
* RFC 3921 (5.1.3) Presence Probes
*/
void XMPPClient::handlePresenceProbes()
{
	fprintf(stderr, "#%d: [XMPPClient: %d] RFC 3921 (5.1.2) Presence Probes\n", getWorkerId(), fd);
	
	Stanza probe = new ATXmlTag("presence");
	probe->setAttribute("type", "probe");
	probe->setAttribute("from", client_jid.full());
	DB::result r = vhost->db.query("SELECT contact_jid FROM roster JOIN users ON roster.id_user = users.id_user WHERE user_login = %s AND contact_subscription IN ('T', 'B')", vhost->db.quote(client_jid.username()).c_str());
	for(; ! r.eof(); r.next()) {
		probe->setAttribute("to", r["contact_jid"]);
		fprintf(stderr, "#%d: [XMPPClient: %d] RFC 3921 (5.1.2) Presence Probes: %s\n", getWorkerId(), fd, probe->asString().c_str());
		server->routeStanza(probe.to().hostname(), probe);
	}
	r.free();
	delete probe;
}

/**
* RFC 3921 (5.1.4) Directed Presence
*/
void XMPPClient::handleDirectedPresence(Stanza stanza)
{
	fprintf(stderr, "#%d: [XMPPClient: %d] RFC 3921 (5.1.4) Directed Presence\n", getWorkerId(), fd);
	
	if ( stanza->hasAttribute("type") && stanza->getAttribute("type") != "unavailable" )
	{
		// хм... у такой станзы type может быть только "unavailable"
		// в RFC не сказано что делать в случае нарушения
		// наверное можно послать ошибку, но мы не будет делать
		// лишних движений и просто спустим эту станзу в /dev/null
		fprintf(stderr, "#%d: [XMPPClient: %d] drop wrong presence: %s\n", getWorkerId(), fd, stanza->asString().c_str());
		return;
	}
	
	// В RFC много слов о том, что такую станзу надо отправить как есть
	// и забыть о ней. Но есть один маленький момент, который
	// обычно вроде игнорируется.
	//
	// TODO По хорошему мы должны быть злопамятны и если кому отправили
	// презенс, то должны отправить и презенс unavailable в случае разрыва
	// связи (или корректного ухода в офлайн) - очень неприятно, когда у
	// собеседника обрывается связь, а ты об этом не знаешь. В RFC есть
	// слабый намёк на это, но похоже его мало кто замечает и реализует.
	server->routeStanza(stanza.to().hostname(), stanza);
}

/**
* RFC 3921 (5.1.5) Unavailable Presence
*/
void XMPPClient::handleUnavailablePresence()
{
	fprintf(stderr, "#%d: [XMPPClient: %d] RFC 3921 (5.1.5) Unavailable Presence\n", getWorkerId(), fd);
	available = false;
	
	Stanza presence = new ATXmlTag("presence");
	presence->setAttribute("type", "unavailable");
	presence->setAttribute("from", client_jid.full());
	DB::result r = vhost->db.query("SELECT contact_jid FROM roster JOIN users ON roster.id_user = users.id_user WHERE user_login = %s AND contact_subscription IN ('F', 'B')", vhost->db.quote(client_jid.username()).c_str());
	for(; ! r.eof(); r.next()) {
		presence->setAttribute("to", r["contact_jid"]);
		server->routeStanza(presence.to().hostname(), presence);
	}
	r.free();
	
	// TODO broadcast presence to other yourself resources
	
	delete presence;
}

/**
* RFC 3921 (5.1.6) Presence Subscriptions
*/
void XMPPClient::handlePresenceSubscriptions(Stanza stanza)
{
	fprintf(stderr, "#%d: [XMPPClient: %d] RFC 3921 (5.1.6) Presence Subscriptions\n", getWorkerId(), fd);
}

void XMPPClient::onPresenceStanza(Stanza stanza) {
	stanza->setAttribute("from", client_jid.full());
	
	if ( ! stanza->hasAttribute("to") )
	{
		if ( ! stanza->hasAttribute("type") )
		{
			if ( ! available )
			{
				// RFC 3921 (5.1.1) Initial Presence
				handleInitialPresence(stanza);
				return;
			}
			
			// RFC 3921 (5.1.2) Presence Broadcast
			handlePresenceBroadcast(stanza);
			return;
		}
		else if ( stanza->getAttribute("type") == "unavailable" )
		{
			// RFC 3921 (5.1.5) Unavailable Presence
			handleUnavailablePresence();
			return;
		}
	}
	else // have "to"
	{
		if ( ! stanza->hasAttribute("type") || stanza->getAttribute("type") == "unavailable" )
		{
			// RFC 3921 (5.1.4) Directed Presence
			handleDirectedPresence(stanza);
			return;
		}
		else
		{
			// RFC 3921 (5.1.6) Presence Subscriptions
			handlePresenceSubscriptions(stanza);
			return;
		}
	}
	
	// левый презенс, нет ни атрибута to, ни атрибута type
	// что с ним делать в RFC не нашел, можно конечно послать ошибку
	// не будем напрягаться, просто выкинем станзу в /dev/null
	// (c) shade
	fprintf(stderr, "#%d: [XMPPClient: %d] drop unknown presence: %s\n", getWorkerId(), fd, stanza->asString().c_str());
}

/**
* Событие: начало потока
*/
void XMPPClient::onStartStream(const std::string &name, const attributes_t &attributes)
{
	attributes_t::const_iterator it = attributes.find("to");
	string tohost = (it != attributes.end()) ? it->second : string();
	fprintf(stderr, "#%d: [XMPPClient: %d] new stream to %s\n", getWorkerId(), fd, tohost.c_str());
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
		vhost = dynamic_cast<VirtualHost*>(server->getHostByName(tohost));
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
	fprintf(stderr, "#%d: [XMPPClient: %d] end of stream\n", getWorkerId(), fd);
	terminate();
}

/**
* JID потока
*/
JID XMPPClient::jid() const
{
	return client_jid;
}

bool XMPPClient::isAuthorized() {
	return state == authorized;
}
