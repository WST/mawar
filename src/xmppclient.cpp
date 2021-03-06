#include <xmppclient.h>
#include <xmppstream.h>
#include <xmppserver.h>
#include <virtualhost.h>
#include <functions.h>
#include <db.h>
#include <tagbuilder.h>

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
	XMPPStream(srv, sock), vhost(0), use_roster(false), compression(false),
	 authorized(false), connected(false), available(false)
{
	lock();
}

/**
* Деструктор потока
*/
XMPPClient::~XMPPClient()
{
	if ( vhost ) vhost->onOffline(this);
}

/**
* Пир (peer) закрыл поток.
*
* Мы уже ничего не можем отправить в ответ,
* можем только корректно закрыть соединение с нашей стороны.
*/
void XMPPClient::onPeerDown()
{
	printf("[XMPPClient: %d] onPeerDown\n", getFd());
	terminate();
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void XMPPClient::onTerminate()
{
	if ( vhost )
	{
		if ( available )
		{
			Stanza presence = new XmlTag("presence");
			presence->setAttribute("from", client_jid.full());
			presence->setAttribute("type", "unavailable");
			presence["status"] = "Connection was lost";
			handleUnavailablePresence(presence);
			delete presence;
		}
		
		vhost->unbindResource(client_jid.resource().c_str(), this);
	}
	mutex.lock();
		endElement("stream:stream");
		flush();
		server->daemon->removeObject(this);
	mutex.unlock();
	
	release();
}

/**
* Обработка станз поступивших от клиента
*
* TODO привести в порядок и в соответствии с RFC 6120 XMPP CORE
*/
void XMPPClient::onStanza(Stanza stanza)
{
	// первым делом пофиксим атрибут from - впишем в него полный JID
	// TODO не будет ли это противоречить RFC ?..
	stanza->setAttribute("from", client_jid.full());
	
	string name = stanza->name();
	
	if ( name == "message" )
	{
		onMessageStanza(stanza);
		return;
	}
	
	if ( name == "presence")
	{
		onPresenceStanza(stanza);
		return;
	}
	
	if ( name == "iq")
	{
		onIqStanza(stanza);
		return;
	}
	
	// TODO something
	if ( name == "auth" )
	{
		onAuthStanza(stanza);
		return;
	}
	
	// TODO something
	if ( name == "response" )
	{
		onResponseStanza(stanza);
		return;
	}
	
	if ( name == "compress" )
	{
		handleCompress(stanza);
		return;
	}
	
	if ( name == "starttls" )
	{
		handleStartTLS(stanza);
		return;
	}
}

/**
* Обработчик авторизации
*/
void XMPPClient::onAuthStanza(Stanza stanza)
{
	if ( ! authorized )
	{
		sasl = vhost->start("xmpp", vhost->hostname(), stanza->getAttribute("mechanism"));
		onSASLStep(stanza);
		return;
	}
}

/**
* Обработка этапа авторизации SASL
*/
void XMPPClient::onSASLStep(const std::string &input)
{
	string output;
	Stanza stanza;
	switch ( vhost->step(sasl, input, output) )
	{
	case SASLServer::ok:
		client_jid.setUsername(vhost->getUsername(sasl));
		user_id = vhost->getUserId(client_jid.username());
		vhost->GSASLServer::close(sasl);
		stanza = new XmlTag("success");
		stanza->insertCharacterData(output);
		stanza->setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
		authorized = true;
		depth = 1; // после выхода из onAuthStanza/onStanza() будет стандартный depth--
		resetParser();
		resetWriter();
		sendStanza(stanza);
		delete stanza;
		break;
	case SASLServer::next:
		stanza = new XmlTag("challenge");
		stanza->setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
		stanza->insertCharacterData(output);
		sendStanza(stanza);
		delete stanza;
		break;
	case SASLServer::error:
		vhost->GSASLServer::close(sasl);
		stanza = new XmlTag("failure");
		stanza->setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
		stanza += new XmlTag("temporary-auth-failure");
		sendStanza(stanza);
		delete stanza;
		break;
	}
}

/**
* Обработчик авторизации: ответ клиента
*/
void XMPPClient::onResponseStanza(Stanza stanza)
{
	if ( ! authorized )
	{
		onSASLStep(stanza);
		return;
	}
}

/**
* Обработчик запроса компрессии
*/
void XMPPClient::handleCompress(Stanza stanza)
{
	if ( ! canCompression( stanza["method"]->getCharacterData().c_str() ) )
	{
		Stanza failure = new XmlTag("failure");
		failure->setDefaultNameSpaceAttribute("http://jabber.org/protocol/compress");
		failure["unsupported-method"];
		sendStanza(failure);
		delete failure;
		return;
	}
	
	compression = true;
	Stanza compressed = new XmlTag("compressed");
	compressed->setDefaultNameSpaceAttribute("http://jabber.org/protocol/compress");
	sendStanza(compressed);
	delete compressed;
	
	if ( ! enableCompression(stanza["method"]->getCharacterData().c_str()) )
	{
		terminate();
		return;
	}
	
	depth = 1; // после выхода из onAuthStanza/onStanza() будет стандартный depth--
	resetParser();
	resetWriter();
}

/**
* Обработчик запроса TLS
*/
void XMPPClient::handleStartTLS(Stanza stanza)
{
	if ( ! vhost->ssl || ! canTLS() || isTLSEnable() )
	{
		// TODO ?
		return;
	}
	
#ifdef HAVE_GNUTLS
	Stanza proceed = new XmlTag("proceed");
	proceed->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-tls");
	sendStanza(proceed);
	delete proceed;
	
	if ( ! enableTLS(&vhost->tls_ctx) )
	{
		terminate();
		return;
	}
	
	depth = 1; // после выхода из onAuthStanza/onStanza() будет стандартный depth--
	resetParser();
	resetWriter();
#endif // HAVE_GNUTLS
}

/**
* Устаревший обработчик iq roster
*
* TODO необходима ревизия, скорее всего надо перенести в VirtualHost
* или в отдельный модуль
*/
void XMPPClient::handleIQRoster(Stanza stanza)
{
	handleRosterIq(stanza);
}

/**
* Обработчик iq-станзы
*/
void XMPPClient::onIqStanza(Stanza stanza)
{
	Stanza body = stanza->firstChild();
	std::string xmlns = body ? body->getAttribute("xmlns", "") : "";
	
	if ( xmlns == "jabber:iq:register" )
	{
		if ( ! connected )
		{
			handleIQRegister(stanza);
			return;
		}
	}
	
	if ( xmlns == "urn:ietf:params:xml:ns:xmpp-bind" )
	{
		if ( ! connected )
		{
			vhost->handleIQBind(stanza, this);
			return;
		}
	}
	
	if ( ! connected )
	{
		//vhost->xmpp_error_queries++;
		
		Stanza result = new XmlTag("iq");
		result->setAttribute("from", vhost->hostname());
		result->setAttribute("type", "error");
		result->setAttribute("id", stanza->getAttribute("id"));
		Stanza error = result["error"];
			error->setAttribute("type", "auth");
			error->setAttribute("code", "401");
			error["not-authorized"]->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-stanzas");
		sendStanza(result);
		delete result;
		return;
	}
	
	if ( xmlns == "jabber:iq:roster" )
	{
		handleIQRoster(stanza);
		return;
	}
	
	// RFC 6120, 10.3.  No 'to' Address
	// Если атрибута 'to' нет, то станзу обработать должен
	// сам сервер (vhost).
	if ( ! stanza->hasAttribute("to") )
	{
		if ( vhost )
		{
			vhost->handleDirectlyIQ(stanza);
			return;
		}
		
		fprintf(stderr, "unexpected XMPPClient::vhost = NULL\n");
		return;
	}
	
	// Если атрибут 'to' передаем маршрутизацию серверу
	server->routeStanza(stanza);
}

ClientPresence XMPPClient::presence()
{
	return client_presence;
}

/**
* RFC 3921 (5.1.1) Initial Presence
*/
void XMPPClient::handleInitialPresence(Stanza stanza)
{
	available = true;
	handlePresenceProbes();
	handlePresenceBroadcast(stanza);
}

/**
* RFC 3921 (5.1.2) Presence Broadcast
*/
void XMPPClient::handlePresenceBroadcast(Stanza stanza)
{
	client_presence.priority = atoi(stanza->getChildValue("priority", "0").c_str()); // TODO
	client_presence.status_text = stanza->getChildValue("status", "");
	client_presence.show = stanza->getChildValue("show", "Available");
	
	DB::result r = vhost->db.query("SELECT contact_jid FROM roster JOIN users ON roster.user_id = users.user_id WHERE user_username = %s AND contact_subscription IN ('F', 'B')", vhost->db.quote(client_jid.username()).c_str());
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
	Stanza probe = new XmlTag("presence");
	probe->setAttribute("type", "probe");
	probe->setAttribute("from", client_jid.full());
	DB::result r = vhost->db.query("SELECT contact_jid FROM roster JOIN users ON roster.user_id = users.user_id WHERE user_username = %s AND contact_subscription IN ('T', 'B')", vhost->db.quote(client_jid.username()).c_str());
	for(; ! r.eof(); r.next()) {
		probe->setAttribute("to", r["contact_jid"]);
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
	if ( stanza->hasAttribute("type") && stanza->getAttribute("type") != "unavailable" )
	{
		// хм... у такой станзы type может быть только "unavailable"
		// в RFC не сказано что делать в случае нарушения
		// наверное можно послать ошибку, но мы не будет делать
		// лишних движений и просто спустим эту станзу в /dev/null
		return;
	}
	
	// В RFC много слов о том, что такую станзу надо отправить как есть
	// и забыть о ней. Но есть один маленький момент, который
	// обычно вроде игнорируется.
	//
	// По хорошему мы должны быть злопамятны и если кому отправили
	// презенс, то должны отправить и презенс unavailable в случае разрыва
	// связи (или корректного ухода в офлайн) - очень неприятно, когда у
	// собеседника обрывается связь, а ты об этом не знаешь.
	if ( stanza->getAttribute("type", "") == "unavailable" )
	{
		vhost->db.query("DELETE FROM dp_spool WHERE user_jid = %s AND contact_jid = %s",
			vhost->db.quote(stanza.from().full()).c_str(),
			vhost->db.quote(stanza.to().full()).c_str()
			);
	}
	else
	{
		vhost->db.query("REPLACE INTO dp_spool (user_jid, contact_jid) VALUES (%s, %s)",
			vhost->db.quote(stanza.from().full()).c_str(),
			vhost->db.quote(stanza.to().full()).c_str()
			);
	}
	server->routeStanza(stanza.to().hostname(), stanza);
}

/**
* RFC 3921 (5.1.5) Unavailable Presence
*/
void XMPPClient::handleUnavailablePresence(Stanza stanza)
{
	available = false;
	
	stanza->setAttribute("from", client_jid.full());
	
	DB::result r = vhost->db.query("SELECT contact_jid FROM roster JOIN users ON roster.user_id = users.user_id WHERE user_username = %s AND contact_subscription IN ('F', 'B')", vhost->db.quote(client_jid.username()).c_str());
	for(; ! r.eof(); r.next()) {
		stanza->setAttribute("to", r["contact_jid"]);
		server->routeStanza(stanza.to().hostname(), stanza);
	}
	r.free();
	
	// TODO broadcast presence to other yourself resources
}

/**
* RFC 3921 (8.2) Presence Subscribe
*/
void XMPPClient::handlePresenceSubscribe(Stanza stanza)
{
	std::string to = stanza.to().bare();
	
	Stanza iq = new XmlTag("iq");
	iq->setAttribute("type", "set");
	TagHelper query = iq["query"];
	query->setDefaultNameSpaceAttribute("jabber:iq:roster");
	TagHelper item = query["item"];
	item->setAttribute("jid", to);
	item->setAttribute("subscription", "none");
	item->setAttribute("ask", "subscribe");
	
	DB::result r = vhost->db.query("SELECT roster.* FROM roster JOIN users ON roster.user_id = users.user_id WHERE user_username = %s AND contact_jid = %s",
		vhost->db.quote(client_jid.username()).c_str(),
		vhost->db.quote(to).c_str()
		);
	if ( ! r.eof() )
	{
		if ( r["contact_nick"] != "" ) item->setAttribute("name", r["contact_nick"]);
		if ( r["contact_group"] != "" ) item["group"] = r["contact_group"];
	}
	r.free();
	
	vhost->rosterPush(client_jid.username(), iq);
	delete iq;
	
	stanza->setAttribute("to", to);
	stanza->setAttribute("from", stanza.from().bare());
	server->routeStanza(stanza);
}

/**
* RFC 3921 (8.2) Presence Subscribed
*/
void XMPPClient::handlePresenceSubscribed(Stanza stanza)
{
	std::string to = stanza.to().bare();
	
	Stanza iq = new XmlTag("iq");
	TagHelper query = iq["query"];
	query->setDefaultNameSpaceAttribute("jabber:iq:roster");
	TagHelper item = query["item"];
	item->setAttribute("jid", to);
	
	DB::result r = vhost->db.query("SELECT roster.* FROM roster JOIN users ON roster.user_id = users.user_id WHERE user_username = %s AND contact_jid = %s",
		vhost->db.quote(client_jid.username()).c_str(),
		vhost->db.quote(to).c_str()
		);
	if ( ! r.eof() )
	{
		item->setAttribute("subscription", (r["contact_subscription"] == "N" || r["contact_subscription"] == "F") ? "from" : "both");
		if ( r["contact_nick"] != "" ) item->setAttribute("name", r["contact_nick"]);
		if ( r["contact_group"] != "" ) item["group"] = r["contact_group"];
		
		const char *subscription = (r["contact_subscription"] == "N" || r["contact_subscription"] == "F") ? "F" : "B";
		vhost->db.query("UPDATE roster SET contact_subscription = '%s' WHERE contact_id = %s",
			subscription,
			vhost->db.quote(r["contact_id"]).c_str()
		);
	}
	else
	{
		item->setAttribute("subscription", "from");
		vhost->db.query("INSERT INTO roster (user_id, contact_jid, contact_subscription, contact_pending) VALUES (%d, %s, 'F', 'N')",
			user_id,
			vhost->db.quote(to).c_str()
		);
	}
	r.free();
	
	vhost->rosterPush(client_jid.username(), iq);
	delete iq;
	
	stanza->setAttribute("from", client_jid.bare());
	server->routeStanza(stanza);
}

/**
* RFC 3921 (8.4) Presence Unsubscribe
*/
void XMPPClient::handlePresenceUnsubscribe(Stanza stanza)
{
	std::string to = stanza.to().bare();
	
	DB::result r = vhost->db.query(
		"SELECT roster.* FROM roster"
		" JOIN users ON roster.user_id = users.user_id"
		" WHERE user_username = %s AND contact_jid = %s",
		vhost->db.quote(client_jid.username()).c_str(),
		vhost->db.quote(to).c_str()
		);
	if ( ! r.eof() )
	{
		Stanza iq = new XmlTag("iq");
		TagHelper query = iq["query"];
		query->setDefaultNameSpaceAttribute("jabber:iq:roster");
		TagHelper item = query["item"];
		item->setAttribute("jid", to);
		item->setAttribute("subscription", (r["contact_subscription"] == "F" || r["contact_subscription"] == "B") ? "from" : "none");
		if ( r["contact_nick"] != "" ) item->setAttribute("name", r["contact_nick"]);
		if ( r["contact_group"] != "" ) item["group"] = r["contact_group"];
		
		const char *subscription = (r["contact_subscription"] == "F" || r["contact_subscription"] == "B") ? "F" : "B";
		vhost->db.query("UPDATE roster SET contact_subscription = '%s' WHERE contact_id = %s",
			subscription,
			vhost->db.quote(r["contact_id"]).c_str()
		);
		
		vhost->rosterPush(client_jid.username(), iq);
		delete iq;
		
	}
	r.free();
	
	stanza->setAttribute("from", client_jid.bare());
	server->routeStanza(stanza);
}

/**
* RFC 3921 (8.2.1) Presence Unsubscribed
*/
void XMPPClient::handlePresenceUnsubscribed(Stanza stanza)
{
	std::string to = stanza.to().bare();
	
	DB::result r = vhost->db.query(
		"SELECT roster.* FROM roster"
		" JOIN users ON roster.user_id = users.user_id"
		" WHERE user_username = %s AND contact_jid = %s",
		vhost->db.quote(client_jid.username()).c_str(),
		vhost->db.quote(to).c_str()
		);
	
	if ( ! r.eof() )
	{
		if ( r["contact_subscription"] == "F" || r["contact_subscription"] == "B" )
		{
			Stanza iq = new XmlTag("iq");
			TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:roster");
			TagHelper item = query["item"];
			item->setAttribute("jid", to);
			item->setAttribute("subscription", (r["contact_subscription"] == "B") ? "to" : "none");
			if ( r["contact_nick"] != "" ) item->setAttribute("name", r["contact_nick"]);
			if ( r["contact_group"] != "" ) item["group"] = r["contact_group"];
			
			const char *subscription = (r["contact_subscription"] == "B") ? "T" : "N";
			vhost->db.query("UPDATE roster SET contact_subscription = '%s' WHERE contact_id = %s",
				subscription,
				vhost->db.quote(r["contact_id"]).c_str()
			);
			
			vhost->rosterPush(client_jid.username(), iq);
			delete iq;
		}
	}
	
	stanza->setAttribute("from", client_jid.bare());
	server->routeStanza(stanza);
}

/**
* RFC 3921 (5.1.6) Presence Subscriptions
*/
void XMPPClient::handlePresenceSubscriptions(Stanza stanza)
{
	if ( stanza->getAttribute("type") == "subscribe" )
	{
		// RFC 3921 (8.2.4) Presence Subscribe
		handlePresenceSubscribe(stanza);
		return;
	}
	
	if ( stanza->getAttribute("type") == "subscribed" )
	{
		// RFC 3921 (8.2) Presence Subscribed
		handlePresenceSubscribed(stanza);
		return;
	}
	
	if ( stanza->getAttribute("type") == "unsubscribe" )
	{
		// RFC 3921 (8.4) Presence Unsubscribe
		handlePresenceUnsubscribe(stanza);
		return;
	}
	
	if ( stanza->getAttribute("type") == "unsubscribed" )
	{
		// RFC 3921 (8.2.1) Presence Unsubscribed
		handlePresenceUnsubscribed(stanza);
		return;
	}
}

void XMPPClient::onPresenceStanza(Stanza stanza)
{
	if ( ! connected )
	{
		if ( stanza->getAttribute("type", "") == "error" ) return;
		
		//vhost->xmpp_error_queries++;
		
		Stanza result = new XmlTag("presence");
		result->setAttribute("from", stanza.to().full());
		result->setAttribute("to", stanza.from().full());
		result->setAttribute("type", "error");
		if ( stanza->hasAttribute("id") ) result->setAttribute("id", stanza->getAttribute("id"));
		Stanza error = result["error"];
			error->setAttribute("type", "auth");
			error->setAttribute("code", "401");
			error["not-authorized"]->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-stanzas");
		
		sendStanza(result);
		delete result;
		return;
	}
	
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
			handleUnavailablePresence(stanza);
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
}

/**
* Обработчик message-станзы
*/
void XMPPClient::onMessageStanza(Stanza stanza)
{
	if ( connected )
	{
		server->routeStanza(stanza);
		return;
	}
	
	if ( stanza->getAttribute("type", "") == "error" ) return;
	
	//vhost->xmpp_error_queries++;
	
	Stanza result = new XmlTag("message");
	result->setAttribute("from", stanza.to().full());
	result->setAttribute("to", stanza.from().full());
	result->setAttribute("type", "error");
	if ( stanza->hasAttribute("id") ) result->setAttribute("id", stanza->getAttribute("id"));
	Stanza error = result["error"];
		error->setAttribute("type", "auth");
		error->setAttribute("code", "401");
		error["not-authorized"]->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-stanzas");
	
	sendStanza(result);
	delete result;
}

/**
* Чтение ростера клиентом
*
* RFC 3921 (7.3) Retrieving One's Roster on Login
*/
void XMPPClient::handleRosterGet(Stanza stanza)
{
	use_roster = true;
	
	Stanza iq = new XmlTag("iq");
	iq->setAttribute("to", client_jid.full());
	iq->setAttribute("type", "result");
	if( stanza->hasAttribute("id") ) iq->setAttribute("id", stanza.id());
	
	TagHelper query = iq["query"];
	query->setDefaultNameSpaceAttribute("jabber:iq:roster");
	
	// Впихнуть элементы ростера тут
	// TODO: ожидание авторизации (pending)
	const char *subscription;
	XmlTag *item;
	
	DB::result r = vhost->db.query(
		"SELECT roster.* FROM roster"
		" JOIN users ON roster.user_id = users.user_id"
		" WHERE users.user_username=%s",
		vhost->db.quote(client_jid.username()).c_str()
		);
	
	for(; ! r.eof(); r.next())
	{
		if ( r["contact_subscription"] == "F" ) subscription = "from";
		else if( r["contact_subscription"] == "T" ) subscription = "to";
		else if( r["contact_subscription"] == "B" ) subscription = "both";
		else subscription = "none";
		
		item = new XmlTag("item");
		item->setAttribute("subscription", subscription);
		item->setAttribute("jid", r["contact_jid"]);
		item->setAttribute("name", r["contact_nick"]);
		query += item;
	}
	r.free();
	
	sendStanza(iq);
	delete iq;
}

/**
* Добавить/обновить контакт в ростере
*
* RFC 3921 (7.4) Adding a Roster Item
* RFC 3921 (7.5) Updating a Roster Item
*/
void XMPPClient::handleRosterItemSet(TagHelper item)
{
	// The server MUST update the roster information in persistent storage,
	// and also push the change out to all of the user's available resources
	// that have requested the roster.  This "roster push" consists of an IQ
	// stanza of type "set" from the server to the client and enables all
	// available resources to remain in sync with the server-based roster
	// information.
	
	string jid_bare = JID(item->getAttribute("jid")).bare();
	
	DB::result r = vhost->db.query(
		"SELECT * FROM roster WHERE user_id = %d AND contact_jid = %s",
		user_id,
		vhost->db.quote(item->getAttribute("jid")).c_str()
		);
	
	if(r.eof())
	{
		// добавить контакт: RFC 3921 (7.4) Adding a Roster Item
		vhost->db.query("INSERT INTO roster (user_id, contact_jid, contact_nick, contact_group, contact_subscription, contact_pending) VALUES (%d, %s, %s, %s, 'N', 'N')",
			user_id,
			vhost->db.quote(jid_bare).c_str(), // contact_jid
			vhost->db.quote(item->getAttribute("name")).c_str(), // contact_nick
			vhost->db.quote(item["group"]).c_str() // contact_group
		);
		
		Stanza iq = new XmlTag("iq");
		iq->setAttribute("type", "set");
		TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:roster");
			TagHelper item2 = query["item"];
			item2->setAttribute("jid", jid_bare);
			item2->setAttribute("name", item->getAttribute("name"));
			item2->setAttribute("subscription", "none");
			if ( item["group"]->getCharacterData() != "" ) item2["group"] += item["group"]->getCharacterData();
		vhost->rosterPush(client_jid.username(), iq);
		delete iq;
	}
	else
	{
		// обновить контакт: RFC 3921 (7.5) Updating a Roster Item
		
		const char *subscription;
		if( r["contact_subscription"] == "F" ) subscription = "from";
		else if ( r["contact_subscription"] == "T" ) subscription = "to";
		else if ( r["contact_subscription"] == "B") subscription = "both";
		else subscription = "none";
		
		std::string name = item->hasAttribute("name") ? item->getAttribute("name") : r["contact_nick"];
		std::string group = item->hasChild("group") ? item["group"] : r["contact_group"];
		std::string contact_jid = r["contact_jid"];
		int contact_id = atoi(r["contact_id"].c_str());
		
		vhost->db.query("UPDATE roster SET contact_nick = %s, contact_group = %s WHERE contact_id = %d",
			vhost->db.quote(name).c_str(),
			vhost->db.quote(group).c_str(),
			contact_id
			);
		
		Stanza iq = new XmlTag("iq");
		iq->setAttribute("type", "set");
		TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:roster");
			TagHelper item2 = query["item"];
			item2->setAttribute("jid", contact_jid);
			item2->setAttribute("name", name);
			item2->setAttribute("subscription", subscription);
			if ( group != "" ) item2["group"] = group;
		
		vhost->rosterPush(client_jid.username(), iq);
		delete iq;
	}
	r.free();
}

/**
* Удалить контакт из ростера
*
* RFC 3921 (7.6) Deleting a Roster Item
* RFC 3921 (8.6) Removing a Roster Item and Canceling All Subscriptions
*/
void XMPPClient::handleRosterItemRemove(TagHelper item)
{
	DB::result r = vhost->db.query("SELECT * FROM roster WHERE user_id = %d AND contact_jid = %s",
		user_id,
		vhost->db.quote(item->getAttribute("jid")).c_str()
		);
	
	if ( r.eof() )
	{
		r.free();
		return;
	}
	
	int contact_id = atoi(r["contact_id"].c_str());
	r.free();
	
	Stanza presence = new XmlTag("presence");
	presence->setAttribute("from", client_jid.bare());
	presence->setAttribute("to", item->getAttribute("jid"));
	
	vhost->broadcastUnavailable(client_jid.username(), item->getAttribute("jid"));
	
	presence->setAttribute("type", "unsubscribed");
	server->routeStanza(presence);
	
	presence->setAttribute("type", "unsubscribe");
	server->routeStanza(presence);
	
	delete presence;
	
	vhost->db.query("DELETE FROM roster WHERE contact_id = %d", contact_id);
	
	Stanza iq = new XmlTag("iq");
	iq->setAttribute("type", "set");
	TagHelper query = iq["query"];
		query->setDefaultNameSpaceAttribute("jabber:iq:roster");
		TagHelper item2 = query["item"];
		item2->setAttribute("jid", item->getAttribute("jid"));
		item2->setAttribute("subscription", "remove");
	vhost->rosterPush(client_jid.username(), iq);
	delete iq;
}

/**
* Проверить корректность запроса RosterSet
*/
bool XMPPClient::checkRosterSet(Stanza stanza)
{
	bool status = true;
	TagHelper query = stanza["query"];
	TagHelper item = query->firstChild("item");
	while ( item )
	{
		string jid = item->getAttribute("jid");
		if ( ! verifyJID(jid) || ! JID(jid).resource().empty() )
		{
			status = false;
			break;
		}
		item = query->nextChild("item", item);
	}
	
	return status;
}

/**
* Обновить ростер
*/
void XMPPClient::handleRosterSet(Stanza stanza)
{
	if ( ! checkRosterSet(stanza) )
	{
		Stanza error = Stanza::iqError(stanza, "not-allowed", "cancel");
		error->setAttribute("from", vhost->hostname());
		sendStanza(error);
		delete error;
		return;
	}
	
	TagHelper query = stanza["query"];
	TagHelper item = query->firstChild("item");
	while ( item )
	{
		if ( item->getAttribute("subscription", "") != "remove" )
		{
			// RFC 3921 (7.4) Adding a Roster Item
			// RFC 3921 (7.5) Updating a Roster Item
			handleRosterItemSet(item);
		}
		else
		{
			// RFC 3921 (7.6) Deleting a Roster Item
			handleRosterItemRemove(item);
		}
		item = query->nextChild("item", item);
	}
	
	Stanza result = new XmlTag("iq");
	result->setAttribute("to", client_jid.full());
	result->setAttribute("type", "result");
	if ( stanza->hasAttribute("to") ) result->setAttribute("id", stanza.id());
	sendStanza(result);
	delete result;
}

/**
* Обработка станз jabber:iq:roster
*
* RFC 3921 (7.) Roster Managment
*/
void XMPPClient::handleRosterIq(Stanza stanza)
{
	if ( stanza->getAttribute("type") == "get" )
	{
		handleRosterGet(stanza);
		return;
	}
	
	if ( stanza->getAttribute("type") == "set" )
	{
		handleRosterSet(stanza);
		return;
	}
	
	return;
}

/**
* XEP-0077: In-Band Registration
* 
* c2s-версия: регистрация происходит как правило до полноценной
* авторизации клиента, соответственно vhost не может адресовать
* такого клиента, поэтому приходиться сделать две отдельные
* версии регистрации: одну для c2s, другую для s2s
*/
void XMPPClient::handleIQRegister(Stanza stanza)
{
	printf("XMPPClient[%d]::handleIQRegister: %s\n", getFd(), stanza->asString().c_str());
	
	if ( ! vhost->registration_allowed )
	{
		Stanza error = Stanza::iqError(stanza, "forbidden", "cancel");
		error->setAttribute("from", vhost->hostname());
		sendStanza(error);
		delete error;
		return;
	}
	
	if ( stanza->getAttribute("type") == "get" )
	{
		Stanza iq = new XmlTag("iq");
		iq->setAttribute("from", vhost->hostname());
		iq->setAttribute("type", "result");
		iq->setAttribute("id", stanza->getAttribute("id"));
		TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:register");
			query["instructions"] = "Choose a username and password for use with this service";
			query["username"];
			query["password"];
		sendStanza(iq);
		delete iq;
		return;
	}
	
	if ( stanza->getAttribute("type") == "set" )
	{
		string username = stanza["query"]["username"]->getCharacterData();
		string password = stanza["query"]["password"]->getCharacterData();
		
		if ( ! verifyUsername(username) || password.empty() )
		{
			Stanza error = Stanza::iqError(stanza, "forbidden", "cancel");
			sendStanza(error);
			delete error;
			return;
		}
		
		if ( vhost->userExists(username) )
		{
			Stanza error = Stanza::iqError(stanza, "conflict", "cancel");
			sendStanza(error);
			delete error;
			return;
		}
		
		if ( vhost->addUser(username, password) )
		{
			Stanza iq = new XmlTag("iq");
			iq->setAttribute("type", "result");
			iq->setAttribute("from", vhost->hostname());
			iq->setAttribute("id", stanza->getAttribute("id"));
			sendStanza(iq);
			delete iq;
			return;
		}
		else
		{
			Stanza error = Stanza::iqError(stanza, "service-unavailable", "wait");
			sendStanza(error);
			delete error;
			return;
		}
	}
	
	Stanza error = Stanza::iqError(stanza, "service-unavailable", "cancel");
	sendStanza(error);
	delete error;
}

/**
* Событие: начало потока
*/
void XMPPClient::onStartStream(const std::string &name, const attributes_t &attributes)
{
	attributes_t::const_iterator it = attributes.find("to");
	string tohost = (it != attributes.end()) ? it->second : string();
	printf("XMPPClient[%d]: new stream to %s version: %d.%d\n", getFd(), tohost.c_str(), ver_major, ver_minor);
	initXML();
	startElement("stream:stream");
	setAttribute("xmlns", "jabber:client");
	setAttribute("xmlns:stream", "http://etherx.jabber.org/streams");
	setAttribute("id", getUniqueId());
	setAttribute("from", tohost);
	setAttribute("version", "1.0");
	setAttribute("xml:lang", "en");
	
	if ( ! authorized  )
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
	flush();
	
	Stanza features = new XmlTag("stream:features");
	if ( ! authorized )
	{
		client_jid.setHostname(vhost->hostname());
		Stanza stanza = features["mechanisms"];
		stanza->setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
		SASLServer::mechanisms_t list = vhost->getMechanisms();
		for(SASLServer::mechanisms_t::const_iterator pos = list.begin(); pos != list.end(); ++pos)
		{
			if ( *pos == "EXTERNAL" ) continue;
			if ( *pos == "ANONYMOUS" ) continue;
			if ( ! isTLSEnable() )
			{
				if ( *pos == "PLAIN" ) continue;
				if ( *pos == "LOGIN" ) continue;
				if ( *pos == "SECURID" ) continue;
			}
			
			Stanza mechanism = new XmlTag("mechanism");
			mechanism->insertCharacterData(*pos);
			stanza->insertChildElement(mechanism);
		}
		
		stanza = features["register"];
		stanza->setAttribute("xmlns", "http://jabber.org/features/iq-register");
		
		if ( vhost->canTLS() && canTLS() && ! isTLSEnable() && ! isCompressionEnable() )
		{
			stanza = features["starttls"];
			stanza->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-tls");
			if ( vhost->tls_required )
			{
				stanza["required"];
			}
		}
		
		if ( canCompression() && ! isCompressionEnable() && ! isTLSEnable() )
		{
			stanza = features["compression"];
			stanza->setDefaultNameSpaceAttribute("http://jabber.org/features/compress");
			const compression_method_t *methods = getCompressionMethods();
			
			while ( *methods )
			{
				XmlTag *method = new XmlTag("method");
				method->insertCharacterData(*methods);
				stanza += method;
				methods++;
			}
		}
	}
	else
	{
		Stanza stanza = features["bind"];
		stanza->setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-bind");
		
		// shade, не игнорируй session, без его поддержки не работают многие клиенты. Раскомментировал.
		stanza = features["session"];
		stanza->setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-session");
	}
	sendStanza(features);
	delete features;
}

/**
* Событие: конец потока
*/
void XMPPClient::onEndStream()
{
	printf("XMPPClient[%d] end of stream\n", getFd());
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
	return authorized;
}

bool XMPPClient::isActive() {
	return authorized;
}
