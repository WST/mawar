
#include <virtualhost.h>
#include <xmppextension.h>
#include <configfile.h>
#include <taghelper.h>
#include <attagparser.h>
#include <string>
#include <iostream>
#include <stdio.h>
#include <nanosoft/gsaslserver.h>
#include <db.h>
#include <time.h>
#include <functions.h>
#include <command.h>

using namespace std;
using namespace nanosoft;

/**
* Конструктор
* @param srv ссылка на сервер
* @param aName имя хоста
* @param config конфигурация хоста
*/
VirtualHost::VirtualHost(XMPPServer *srv, const std::string &aName, ATXmlTag *cfg): XMPPDomain(srv, aName) {
	
	ATXmlTag *extensions = cfg->firstChild("extensions");
	if ( extensions )
	{
		for(ATXmlTag *ext = extensions->firstChild(); ext; ext = extensions->nextChild(ext))
		{
			addExtension(ext->getAttribute("urn", "").c_str(), ext->getAttribute("fname", "").c_str());
		}
	}
	
	TagHelper registration = cfg->getChild("registration");
	registration_allowed = registration->getAttribute("enabled", "no") == "yes";
	
	TagHelper storage = cfg->getChild("storage");
	if(storage->getAttribute("engine", "mysql") != "mysql") {
		fprintf(stderr, "[VirtualHost] unknown storage engine: %s\n", storage->getAttribute("engine").c_str());
	}
	
	// Подключаемся к БД
	string server = storage["server"];
	if(server.substr(0, 5) == "unix:" ) {
		if ( ! db.connectUnix(server.substr(5), storage["database"], storage["username"], storage["password"]) )
		{
			fprintf(stderr, "[VirtualHost] cannot connect to database\n");
		}
	} else {
		if ( ! db.connect(server, storage["database"], storage["username"], storage["password"]) )
		{
			fprintf(stderr, "[VirtualHost] cannot connect to database\n");
		}
	}
	
	// Очищаем пул Directed Presence (RFC 3921, 5.1.4)
	// TODO возможно, при старте нужно рассылать Presence Unsubscribe...
	db.query("TRUNCATE dp_spool");
	
	// кодировка только UTF-8
	db.query("SET NAMES UTF8");
	
	onliners_number = 0; // можно считать элементы карт, но ето более накладно
	vcard_queries = 0;
	stats_queries = 0;
	xmpp_ping_queries = 0;
	xmpp_error_queries = 0;
	version_requests = 0;
	start_time = time(0);
	
	config = cfg;
}

/**
* Деструктор
*/
VirtualHost::~VirtualHost() {
}

/**
* Информационные запросы без атрибута to
* Адресованные клиентом данному узлу
* 
* TODO требуется привести в порядок маршрутизацию
*/
void VirtualHost::handleVHostIq(Stanza stanza)
{
	// если станза адресуется к серверу, то обработать должен сервер
	if ( stanza.to().full() == hostname() )
	{
		handleDirectlyIQ(stanza);
		return;
		// весь ниже лежаший код относиться к handleDirectlyIQ()
		// однако вместо того тупо копипастить, будем по тихоньку
		// делить на функции и запихивать в handleDirectlyIQ()
	}
	
	// если станза адресуется клиенту, то доставить нужно клиенту
	if ( stanza.to().username() != "" )
	{
		// TODO отправить станзу клиенту
		VirtualHost::sessions_t::iterator it;
		VirtualHost::reslist_t reslist;
		VirtualHost::reslist_t::iterator jt;
		
		it = onliners.find(stanza.to().username());
		if( it != onliners.end() )
		{
			// Проверить, есть ли ресурс, если он указан
			JID to = stanza.to();
			if( ! to.resource().empty() )
			{
				// если указан ресурс, то отправить на ресурс
				jt = it->second.find(to.resource());
				if( jt != it->second.end() )
				{
					jt->second->sendStanza(stanza); // TODO — учесть bool
					return;
				}
				// Не отправили на выбранный ресурс, смотрим дальше…
				// TODO сообщить что такого клиента нет
				return;
			}
			else
			{
				// TODO как правильно?
				// а правильно обработать должен сервер, т.е. в данном
				// случае vhost
				handleDirectlyIQ(stanza);
				return;
			}
		}
		else
		{
			// TODO клиент не найден, надо отправить соответствующую станзу
			Stanza result = new ATXmlTag("iq");
			result->setAttribute("from", stanza.to().full());
			result->setAttribute("to", stanza.from().full());
			result->setAttribute("type", "error");
			result->setAttribute("id", stanza->getAttribute("id", ""));
			Stanza err = result["error"];
			err->setAttribute("type", "cancel");
			err["service-unavailable"]->setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas");
			server->routeStanza(result);
			delete result;
			return;
		}
	}
	
	handleIQUnknown(stanza);
}

/**
* Обслуживание обычного презенса
*/
void VirtualHost::serveCommonPresence(Stanza stanza)
{
	JID to = stanza.to();
	JID from = stanza.from();
	
	int priority = atoi(stanza->getChildValue("priority", "0").c_str());
	if(priority >= 0) {
		// Отправить оффлайн-сообщения, если появился ресурс с неотрицательным приоритетом
		// Пока таких ресурсов нет, сообщения сохраняются на сервере.
		// ©WST
		XMPPClient *client = getClientByJid(from);
		if(client) {
			sendOfflineMessages(client);
		}
	}
	
	if ( to.resource() != "" )
	{
		XMPPClient *client = getClientByJid(to);
		if ( client ) client->sendStanza(stanza);
		return;
	}
	
	mutex.lock();
		sessions_t::iterator it = onliners.find(to.username());
		if(it != onliners.end()) {
			for(reslist_t::iterator jt = it->second.begin(); jt != it->second.end(); ++jt)
			{
				stanza->setAttribute("to", jt->second->jid().full());
				jt->second->sendStanza(stanza);
			}
		}
	mutex.unlock();
}

/**
* Отправить presence unavailable со всех ресурсов пользователя
* @param from логин пользователя от имени которого надо отправить
* @param to jid (bare) которому надо отправить
*/
void VirtualHost::broadcastUnavailable(const std::string &from, const std::string &to)
{
	// TODO optimize
	// 1. получаем копию списка ресурсов (иначе dead lock)
	// (c) shade
	mutex.lock();
		sessions_t::iterator user = onliners.find(to);
		reslist_t res = (user != onliners.end()) ? user->second : reslist_t();
	mutex.unlock();
	
	// 2. проходимся по копии списка и делаем рассылку
	// TODO но есть опасность, что пока мы будем работать со списком
	// какой-то из клиентов завершит сеанс и у нас в списке будет битая ссылка
	// впрочем эта проблема могла быть и раньше
	// (c) shade
	Stanza p = new ATXmlTag("presence");
	p->setAttribute("to", from);
	p->setAttribute("type", "unavailable");
	for(reslist_t::iterator jt = res.begin(); jt != res.end(); ++jt)
	{
		p->setAttribute("from", jt->second->jid().full());
		server->routeStanza(p);
	}
	delete p;
}

/**
* Обслуживание Presence Probes
*
* RFC 3921 (5.1.3) Presence Probes
*/
void VirtualHost::servePresenceProbes(Stanza stanza)
{
	JID to = stanza.to();
	JID from = stanza.from();
	
	DB::result r = db.query(
		"SELECT count(*) AS cnt FROM roster JOIN users ON roster.id_user = users.id_user "
		"WHERE user_login = %s AND contact_jid = %s AND contact_subscription IN ('F', 'B')",
			db.quote(to.username()).c_str(), db.quote(from.bare()).c_str());
	if ( r["cnt"] != "0" )
	{
		// TODO optimize
		// 1. получаем копию списка ресурсов (иначе dead lock)
		// (c) shade
		mutex.lock();
			sessions_t::iterator user = onliners.find(stanza.to().username());
			reslist_t res = (user != onliners.end()) ? user->second : reslist_t();
		mutex.unlock();
		
		// 2. проходимся по копии списка и делаем рассылку
		// TODO но есть опасность, что пока мы будем работать со списком
		// какой-то из клиентов завершит сеанс и у нас в списке будет битая ссылка
		// впрочем эта проблема могла быть и раньше
		// (c) shade
		for(reslist_t::iterator jt = res.begin(); jt != res.end(); ++jt)
		{
			Stanza p = Stanza::presence(jt->second->jid(), stanza.from(), jt->second->presence());
			server->routeStanza(p.to().hostname(), p);
			delete p;
		}
	}
	r.free();
	return;
}

/**
* RFC 3921 (8.2) Presence Subscribe
*/
void VirtualHost::servePresenceSubscribe(Stanza stanza)
{
	string from = stanza.from().bare();
	string to = stanza.to().username();
	
	DB::result r = db.query("SELECT roster.* FROM roster JOIN users ON roster.id_user = users.id_user WHERE user_login = %s AND contact_jid = %s",
		db.quote(to).c_str(),
		db.quote(from).c_str()
		);
	if ( ! r.eof() )
	{
		if ( r["contact_subscription"] == "F" || r["contact_subscription"] == "B" )
		{ // уже авторизован, просто повторно отправим подтвержение
			Stanza presence = new ATXmlTag("presence");
			presence->setAttribute("from", stanza.to().bare());
			presence->setAttribute("to", from);
			presence->setAttribute("type", "subscribed");
			server->routeStanza(presence);
			delete presence;
			
			r.free();
			return;
		}
	}
	else
	{ // сохранить запрос в БД
		db.query("INSERT INTO roster (id_user, contact_jid, contact_subscription, contact_pending) VALUES (%d, %s, 'N', 'P')", getUserId(to), db.quote(from).c_str());
	}
	r.free();
	
	// TODO broadcast only to whom requested roster
	broadcast(stanza, to);
}

/**
* RFC 3921 (8.2) Presence Subscribed
*/
void VirtualHost::servePresenceSubscribed(Stanza stanza)
{
	string from = stanza.from().bare();
	string to = stanza.to().username();
	
	DB::result r = db.query("SELECT roster.* FROM roster JOIN users ON roster.id_user = users.id_user WHERE user_login = %s AND contact_jid = %s",
		db.quote(to).c_str(),
		db.quote(from).c_str()
		);
	
	if ( ! r.eof() )
	{
		if ( r["contact_subscription"] == "N" || r["contact_subscription"] == "F" )
		{
			Stanza iq = new ATXmlTag("iq");
			TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:roster");
			TagHelper item = query["item"];
			item->setAttribute("jid", from);
			item->setAttribute("subscription", "to");
			if ( r["contact_nick"] != "" ) item->setAttribute("name", r["contact_nick"]);
			if ( r["contact_group"] != "" ) item["group"] = r["contact_group"];
			
			const char *subscription = (r["contact_subscription"] == "N") ? "T" : "B";
			db.query("UPDATE roster SET contact_subscription = '%s' WHERE id_contact = %s",
				subscription,
				r["id_contact"].c_str()
				);
			
			rosterPush(to, iq);
			delete iq;
			
			serveCommonPresence(stanza);
		}
		
		// отправить презенсы со всех ресурсов
		
		// TODO optimize
		// 1. получаем копию списка ресурсов (иначе dead lock)
		// (c) shade
		mutex.lock();
			sessions_t::iterator user = onliners.find(to);
			reslist_t res = (user != onliners.end()) ? user->second : reslist_t();
		mutex.unlock();
		
		// 2. проходимся по копии списка и делаем рассылку
		// TODO но есть опасность, что пока мы будем работать со списком
		// какой-то из клиентов завершит сеанс и у нас в списке будет битая ссылка
		// впрочем эта проблема могла быть и раньше
		// (c) shade
		for(reslist_t::iterator jt = res.begin(); jt != res.end(); ++jt)
		{
			Stanza p = Stanza::presence(jt->second->jid(), stanza.from(), jt->second->presence());
			server->routeStanza(p.to().hostname(), p);
			delete p;
		}
	}
	r.free();
}

/**
* RFC 3921 (8.4) Presence Unsubscribe
*/
void VirtualHost::servePresenceUnsubscribe(Stanza stanza)
{
	string from = stanza.from().bare();
	string to = stanza.to().username();
	
	DB::result r = db.query(
		"SELECT roster.* FROM roster"
		" JOIN users ON roster.id_user = users.id_user"
		" WHERE user_login = %s AND contact_jid = %s",
		db.quote(to).c_str(),
		db.quote(from).c_str()
		);
	if ( ! r.eof() )
	{
		if ( r["contact_subscription"] == "F" || r["contact_subscription"] == "B" )
		{ // уже авторизован, просто повторно отправим подтвержение
			Stanza iq = new ATXmlTag("iq");
			TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:roster");
			TagHelper item = query["item"];
			item->setAttribute("jid", from);
			item->setAttribute("subscription", (r["contact_subscription"] == "B") ? "to" : "none");
			if ( r["contact_nick"] != "" ) item->setAttribute("name", r["contact_nick"]);
			if ( r["contact_group"] != "" ) item["group"] = r["contact_group"];
			
			const char *subscription = (r["contact_subscription"] == "B") ? "T" : "N";
			db.query("UPDATE roster SET contact_subscription = '%s' WHERE id_contact = %s",
				subscription,
				r["id_contact"].c_str()
				);
			
			// TODO broadcast only to whom requested roster
			broadcast(stanza, to);
		}
	}
	r.free();
	
	// отправить презенсы со всех ресурсов
	broadcastUnavailable(from, to);
}

/**
* RFC 3921 (8.2.1) Presence Unsubscribed
*/
void VirtualHost::servePresenceUnsubscribed(Stanza stanza)
{
	string from = stanza.from().bare();
	string to = stanza.to().username();
	
	DB::result r = db.query(
		"SELECT roster.* FROM roster"
		" JOIN users ON roster.id_user = users.id_user"
		" WHERE user_login = %s AND contact_jid = %s",
		db.quote(to).c_str(),
		db.quote(from).c_str()
		);
	
	if ( ! r.eof() )
	{
		if ( r["contact_subscription"] == "T" || r["contact_subscription"] == "B" )
		{
			Stanza iq = new ATXmlTag("iq");
			TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:roster");
			TagHelper item = query["item"];
			item->setAttribute("jid", from);
			item->setAttribute("subscription", (r["contact_subscription"] == "B") ? "from" : "none");
			if ( r["contact_nick"] != "" ) item->setAttribute("name", r["contact_nick"]);
			if ( r["contact_group"] != "" ) item["group"] = r["contact_group"];
			
			const char *subscription = (r["contact_subscription"] == "B") ? "F" : "N";
			db.query("UPDATE roster SET contact_subscription = '%s' WHERE id_contact = %s",
				subscription,
				r["id_contact"].c_str()
				);
			
			rosterPush(to, iq);
			delete iq;
			
			serveCommonPresence(stanza);
		}
	}
	r.free();
}

/**
* Обслуживаение Presence Subscriptions
*
* RFC 3921 (5.1.6) Presence Subscriptions
*/
void VirtualHost::servePresenceSubscriptions(Stanza stanza)
{
	if ( stanza->getAttribute("type") == "subscribe" )
	{
		// RFC 3921 (8.2) Presence Subscribe
		servePresenceSubscribe(stanza);
		return;
	}
	
	if ( stanza->getAttribute("type") == "subscribed" )
	{
		// RFC 3921 (8.2) Presence Subscribed
		servePresenceSubscribed(stanza);
		return;
	}
	
	if ( stanza->getAttribute("type") == "unsubscribe" )
	{
		// RFC 3921 (8.4) Presence Unsubscribe
		servePresenceUnsubscribe(stanza);
		return;
	}
	
	if ( stanza->getAttribute("type") == "unsubscribed" )
	{
		// RFC 3921 (8.2.1) Presence Unsubscribed
		servePresenceUnsubscribed(stanza);
		return;
	}
}

/**
* Серверная часть обработки станзы presence
*
* Вся клиентская часть находиться в классе XMPPClinet.
* Сюда попадают только станзы из сети (s2s)
* или из других виртуальных хостов.
*
* @todo обработка атрибута type
* @todo добавить отправку оффлайн-сообщений при смене приоритета с отрицательного на положительный
*/
void VirtualHost::servePresence(Stanza stanza)
{
	if ( ! stanza->hasAttribute("type") || stanza->getAttribute("type") == "unavailable" )
	{
		serveCommonPresence(stanza);
		return;
	}
	
	if ( stanza->hasAttribute("type") )
	{
		if ( stanza->getAttribute("type") == "error" )
		{
			fprintf(stderr, "[VirtualHost]: drop presence error: %s\n", stanza->asString().c_str());
			return;
		}
		
		if ( stanza->getAttribute("type") == "probe" )
		{
			servePresenceProbes(stanza);
			return;
		}
		
		servePresenceSubscriptions(stanza);
		return;
	}
	
	return;
}

/**
* Отправить станзу всем ресурсам указаного пользователя
*/
void VirtualHost::broadcast(Stanza stanza, const std::string &login)
{
	mutex.lock();
	if( onliners.find(login) != onliners.end() ) {
		const reslist_t *r = &onliners[login];
		for(reslist_t::const_iterator iter = r->begin(); iter != r->end(); ++iter)
		{
			stanza->setAttribute("to", iter->second->jid().full());
			iter->second->sendStanza(stanza);
		}
	}
	mutex.unlock();
}

bool VirtualHost::userExists(std::string username) {
	bool retval;
	DB::result r = db.query("SELECT count(*) AS cnt FROM users WHERE user_login=%s", db.quote(username).c_str());
	retval = atoi(r["cnt"].c_str()) == 1;
	r.free();
	return retval;
}

void VirtualHost::saveOfflineMessage(Stanza stanza) {
	// При флуд-атаках нимбуззеров сообщения часто идут быстрым потоком
	// При этом крайне нежелательно порождать трафик с MySQL тучей запросов count
	// Было бы классно кешировать число оффлайн-сообщений, например, в какой-то карте…
	// © WST
	if(!userExists(stanza.to().username())) {
		return;
	}
	DB::result r = db.query("SELECT count(*) AS cnt FROM spool WHERE message_to=%s", db.quote(stanza.to().bare()).c_str());
	if(atoi(r["cnt"].c_str()) < 100) { // TODO — брать максимальное число оффлайн-сообщений из конфига
		std::string data = stanza->asString();
		if(data.length() > 61440) {
			Stanza error = Stanza::iqError(stanza, "resouce-constraint", "cancel");
			server->routeStanza(error);
			delete error;
			r.free();
			return;
		}
		db.query("INSERT INTO spool (message_to, message_stanza, message_when) VALUES (%s, %s, %d)", db.quote(stanza.to().bare()).c_str(), db.quote(data).c_str(), time(NULL));
	} else {
		Stanza error = Stanza::iqError(stanza, "resouce-constraint", "cancel");
		server->routeStanza(error);
		delete error;
	}
	r.free();
}

void VirtualHost::handleMessage(Stanza stanza) {
	
	VirtualHost::sessions_t::iterator it;
	VirtualHost::reslist_t reslist;
	VirtualHost::reslist_t::iterator jt;
	
	it = onliners.find(stanza.to().username());
	if(it != onliners.end()) {
		// Проверить, есть ли ресурс, если он указан
		JID to = stanza.to();
		if(!to.resource().empty()) {
			jt = it->second.find(to.resource());
			if(jt != it->second.end()) {
				jt->second->sendStanza(stanza); // TODO — учесть bool
				return;
			}
			// Не отправили на выбранный ресурс, смотрим дальше…
		}
		std::list<XMPPClient *> sendto_list;
		std::list<XMPPClient *>::iterator kt;
		int max_priority = 0;
		for(jt = it->second.begin(); jt != it->second.end(); jt++) {
			if(jt->second->presence().priority == max_priority) {
				sendto_list.push_back(jt->second);
			} else if(jt->second->presence().priority > max_priority) {
				sendto_list.clear();
				sendto_list.push_back(jt->second);
			}
		}
		if(sendto_list.empty()) {
			if(stanza.type() == "normal" || stanza.type() == "chat") {
				saveOfflineMessage(stanza);
			}
			return;
		}
		for(kt = sendto_list.begin(); kt != sendto_list.end(); kt++) {
			(*kt)->sendStanza(stanza); // TODO — учесть bool
		}
	} else {
		if(stanza.type() == "normal" || stanza.type() == "chat") {
			saveOfflineMessage(stanza);
		}
	}
}

/**
* RFC 6120, 10.3.  No 'to' Address
* 
* If the stanza possesses no 'to' attribute, the server MUST handle
* it directly on behalf of the entity that sent it, where the meaning
* of "handle it directly" depends on whether the stanza is message,
* presence, or IQ. Because all stanzas received from other servers
* MUST possess a 'to' attribute, this rule applies only to stanzas
* received from a local entity (typically a client) that is connected
* to the server. 
*/
void VirtualHost::handleDirectly(Stanza stanza)
{
	if (stanza->name() == "message" ) handleDirectlyMessage(stanza);
	else if (stanza->name() == "presence") handleDirectlyPresence(stanza);
	else if (stanza->name() == "iq") handleDirectlyIQ(stanza);
	else ; // ??
}

/**
* RFC 6120, 10.3.1  No 'to' Address, Message
* 
* If the server receives a message stanza with no 'to' attribute,
* it MUST treat the message as if the 'to' address were the bare
* JID <localpart@domainpart> of the sending entity. 
*/
void VirtualHost::handleDirectlyMessage(Stanza stanza)
{
}

/**
* RFC 6120, 10.3.2  No 'to' Address, Presence
* 
* If the server receives a presence stanza with no 'to' attribute,
* it MUST broadcast it to the entities that are subscribed to the
* sending entity's presence, if applicable ([XMPP‑IM] defines the
* semantics of such broadcasting for presence applications). 
*/
void VirtualHost::handleDirectlyPresence(Stanza stanza)
{
}

/**
* RFC 6120, 10.3.3  No 'to' Address, IQ
* 
* If the server receives an IQ stanza with no 'to' attribute, it MUST
* process the stanza on behalf of the account from which received
* the stanza
*/
void VirtualHost::handleDirectlyIQ(Stanza stanza)
{
	// сначала ищем в модулях-расширениях
	Stanza body = stanza->firstChild();
	std::string xmlns = body ? body->getAttribute("xmlns", "") : "";
	
	// ищем модуль-расширение
	extlist_t::iterator it = ext.find(xmlns);
	if ( it != ext.end() )
	{
		ext[xmlns]->handleStanza(stanza);
		return;
	}
	
	if ( xmlns == "urn:xmpp:ping" )
	{
		handleIQPing(stanza);
		return;
	}
	
	if ( xmlns == "jabber:iq:register" )
	{
		// Запрос регистрации на s2s
		handleRegisterIq(0, stanza);
		return;
	}
	
	if ( xmlns == "jabber:iq:version" )
	{
		handleIQVersion(stanza);
		return;
	}
	
	if ( xmlns == "jabber:iq:last" )
	{
		handleIQLast(stanza);
		return;
	}
	
	if ( xmlns == "jabber:iq:private" )
	{
		handleIQPrivateStorage(stanza);
		return;
	}
	
	if ( xmlns == "http://jabber.org/protocol/disco#info" )
	{
		handleIQServiceDiscoveryInfo(stanza);
		return;
	}
	
	if ( xmlns == "http://jabber.org/protocol/disco#items" )
	{
		handleIQServiceDiscoveryItems(stanza);
		return;
	}
	
	if ( xmlns == "http://jabber.org/protocol/commands" )
	{
		handleIQAdHocCommands(stanza);
		return;
	}
	
	if ( xmlns == "http://jabber.org/protocol/stats" )
	{
		handleIQStats(stanza);
		return;
	}
	
	if ( xmlns == "vcard-temp" )
	{
		handleIQVCardTemp(stanza);
		return;
	}
	
	handleIQUnknown(stanza);
}

/**
* XEP-0012: Last Activity
*
* TODO привести в порядок
*/
void VirtualHost::handleIQLast(Stanza stanza)
{
	if ( stanza->getAttribute("type") == "get" )
	{
		unsigned long int uptime = time(0) - start_time;
		
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("from", stanza.to().full());
		iq->setAttribute("to", stanza.from().full());
		iq->setAttribute("id", stanza->getAttribute("id", ""));
		iq->setAttribute("type", "result");
		Stanza query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:last");
			query->setAttribute("seconds", mawarPrintInteger(uptime));
		server->routeStanza(iq);
		delete iq;
		return;
	}
	
	handleIQUnknown(stanza);
}

/**
* XEP-0030: Service Discovery #info
* 
* TODO требуется ревизия и поддержка модульности: должен уметь представлять
* возможности других модулей
*/
void VirtualHost::handleIQServiceDiscoveryInfo(Stanza stanza)
{
	if ( stanza->getAttribute("type") == "get")
	{
		// TODO segfault possible
		std::string node = stanza["query"]->getAttribute("node", "");
		if( node == "config" )
		{
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("from", name);
			iq->setAttribute("to", stanza.from().full());
			iq->setAttribute("type", "result");
			TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("http://jabber.org/protocol/disco#info");
			query->setAttribute("node", "config");
			iq->setAttribute("id", stanza->getAttribute("id", ""));
				ATXmlTag *identity = new ATXmlTag("identity");
				identity->setAttribute("category", "automation");
				identity->setAttribute("type", "command-node");
				identity->setAttribute("name", "Configure Service");
				query->insertChildElement(identity);
			
				ATXmlTag *feature;
				feature = new ATXmlTag("feature");
					feature->setAttribute("var", "http://jabber.org/protocol/commands");
					query->insertChildElement(feature);
				
				feature = new ATXmlTag("feature");
					feature->setAttribute("var", "jabber:x:data");
					query->insertChildElement(feature);
				
			server->routeStanza(iq);
			return;
		}
		else
		{
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("from", name);
			iq->setAttribute("to", stanza.from().full());
			iq->setAttribute("type", "result");
			TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("http://jabber.org/protocol/disco#info");
			iq->setAttribute("id", stanza->getAttribute("id", ""));
				ATXmlTag *identity = new ATXmlTag("identity");
				identity->setAttribute("category", "server");
				identity->setAttribute("type", "im");
				identity->setAttribute("name", "@}->--"); // TODO: макросы с именем, версией итп
				query->insertChildElement(identity);
			
				ATXmlTag *feature;
			
				feature = new ATXmlTag("feature");
				feature->setAttribute("var", "http://jabber.org/protocol/disco#info");
				query->insertChildElement(feature);
			
				feature = new ATXmlTag("feature");
				feature->setAttribute("var", "http://jabber.org/protocol/disco#items");
				query->insertChildElement(feature);
			
				feature = new ATXmlTag("feature");
				feature->setAttribute("var", "http://jabber.org/protocol/stats");
				query->insertChildElement(feature);
			
				feature = new ATXmlTag("feature");
				feature->setAttribute("var", "jabber:iq:version");
				query->insertChildElement(feature);
			
				feature = new ATXmlTag("feature");
				feature->setAttribute("var", "msgoffline");
				query->insertChildElement(feature);
			
				feature = new ATXmlTag("feature");
				feature->setAttribute("var", "vcard-temp");
				query->insertChildElement(feature);
			
				// XEP-0050 ad-hoc commands
				feature = new ATXmlTag("feature");
				feature->setAttribute("var", "http://jabber.org/protocol/commands");
				query->insertChildElement(feature);
			
				if( registration_allowed )
				{
					feature = new ATXmlTag("feature");
					feature->setAttribute("var", "jabber:iq:register");
					query->insertChildElement(feature);
				}
			
			server->routeStanza(iq);
			delete iq;
			return;
		}
	}
	
	handleIQUnknown(stanza);
}

/**
* XEP-0030: Service Discovery #items
*
* TODO требуется ревизия и поддержка модульности: должен уметь представлять
* возможности других модулей
*/
void VirtualHost::handleIQServiceDiscoveryItems(Stanza stanza)
{
	if( stanza->getAttribute("type") == "get" )
	{
		std::string node = stanza["query"]->getAttribute("node", "");
		if( node == "http://jabber.org/protocol/commands" )
		{
			// Нода команд ad-hoc
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("from", name);
			iq->setAttribute("to", stanza.from().full());
			iq->setAttribute("type", "result");
			iq->setAttribute("id", stanza->getAttribute("id", ""));
			TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("http://jabber.org/protocol/disco#items");
			query->setAttribute("node", "http://jabber.org/protocol/commands");
		
			ATXmlTag *item;
			// здесь можно засунуть команды, доступные простым юзерам
			if(isAdmin(stanza.from().bare()))
			{
				item = new ATXmlTag("item");
					item->setAttribute("jid", name);
					item->setAttribute("node", "enable_registration");
					item->setAttribute("name", "Enable/disable user's registation");
					query->insertChildElement(item);
				
				item = new ATXmlTag("item");
					item->setAttribute("jid", name);
					item->setAttribute("node", "stop");
					item->setAttribute("name", "Stop Mawar daemon");
					query->insertChildElement(item);
				
				item = new ATXmlTag("item");
					item->setAttribute("jid", name);
					item->setAttribute("node", "stop-vhost");
					item->setAttribute("name", "Stop a virtual host");
					query->insertChildElement(item);
				
				item = new ATXmlTag("item");
					item->setAttribute("jid", name);
					item->setAttribute("node", "start-vhost");
					item->setAttribute("name", "Start a virtual host");
					query->insertChildElement(item);
				
				item = new ATXmlTag("item");
					item->setAttribute("jid", name);
					item->setAttribute("node", "create-vhost");
					item->setAttribute("name", "Create a new virtual host");
					query->insertChildElement(item);
				
				item = new ATXmlTag("item");
					item->setAttribute("jid", name);
					item->setAttribute("node", "drop-vhost");
					item->setAttribute("name", "Delete virtual host");
					query->insertChildElement(item);
				
				item = new ATXmlTag("item");
					item->setAttribute("jid", name);
					item->setAttribute("node", "route-stanza");
					item->setAttribute("name", "Push stanza to the router");
					query->insertChildElement(item);
			}
		
			server->routeStanza(iq);
			delete iq;
			return;
		}
		else
		{
			// Нода не указана или неизвестна
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("from", name);
			iq->setAttribute("to", stanza.from().full());
			iq->setAttribute("type", "result");
			TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("http://jabber.org/protocol/disco#items");
			iq->setAttribute("id", stanza->getAttribute("id", ""));
			
			ATXmlTag *item;
			for(ATXmlTag *i = config->find("disco/item"); i; i = config->findNext("disco/item", i))
			{
				item = new ATXmlTag("item");
				item->setAttribute("jid", i->getAttribute("jid", name));
				item->setAttribute("node", i->getAttribute("node"));
				item->setAttribute("name", i->getCharacterData());
				query->insertChildElement(item);
			}
			// TODO: здесь также неплохо бы добавить приконнекченные компоненты…
			// Хотя, конечно, можно обойтись и ручным указанием списка элементов обзора
			// © WST
			
			server->routeStanza(iq);
			delete iq;
			return;
		}
	}
	
	handleIQUnknown(stanza);
}

/**
* XEP-0039: Statistics Gathering
*/
void VirtualHost::handleIQStats(Stanza stanza)
{
	if( stanza->getAttribute("type") == "get")
	{
		stats_queries++;
		mawarWarning("Served incoming stats request");
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("from", name);
		iq->setAttribute("to", stanza.from().full());
		iq->setAttribute("type", "result");
		TagHelper query = iq["query"];
		query->setDefaultNameSpaceAttribute("http://jabber.org/protocol/stats");
		iq->setAttribute("id", stanza->getAttribute("id", ""));
		
		ATXmlTag *stat = new ATXmlTag("stat");
			stat->setAttribute("name", "users/online");
			stat->setAttribute("value", mawarPrintInteger(onliners_number));
			stat->setAttribute("units", "users");
			query->insertChildElement(stat);
		
		stat = new ATXmlTag("stat");
			stat->setAttribute("name", "users/total");
			DB::result r = db.query("SELECT count(*) AS cnt FROM users");
			stat->setAttribute("value", r["cnt"]);
			r.free();
			stat->setAttribute("units", "users");
			query->insertChildElement(stat);
			
		stat = new ATXmlTag("stat");
			stat->setAttribute("name", "queries/vcard");
			stat->setAttribute("value", mawarPrintInteger(vcard_queries));
			stat->setAttribute("units", "queries");
			query->insertChildElement(stat);
		
		stat = new ATXmlTag("stat");
			stat->setAttribute("name", "queries/stats");
			stat->setAttribute("value", mawarPrintInteger(stats_queries));
			stat->setAttribute("units", "queries");
			query->insertChildElement(stat);
			
		stat = new ATXmlTag("stat");
			stat->setAttribute("name", "queries/xmpp-pings");
			stat->setAttribute("value", mawarPrintInteger(xmpp_ping_queries));
			stat->setAttribute("units", "queries");
			query->insertChildElement(stat);
		
		stat = new ATXmlTag("stat");
			stat->setAttribute("name", "queries/xmpp-errors");
			stat->setAttribute("value", mawarPrintInteger(xmpp_error_queries));
			stat->setAttribute("units", "queries");
			query->insertChildElement(stat);
		
		stat = new ATXmlTag("stat");
			stat->setAttribute("name", "queries/version");
			stat->setAttribute("value", mawarPrintInteger(version_requests));
			stat->setAttribute("units", "queries");
			query->insertChildElement(stat);
			
		stat = new ATXmlTag("stat");
			stat->setAttribute("name", "misc/registration_allowed");
			stat->setAttribute("value", registration_allowed ? "yes," : "no,");
			stat->setAttribute("units", registration_allowed ? "allowed" : "not allowed");
			query->insertChildElement(stat);
		
		stat = new ATXmlTag("stat");
			stat->setAttribute("name", "misc/uptime");
			stat->setAttribute("value", mawarPrintInteger(time(0) - start_time));
			stat->setAttribute("units", "seconds");
			query->insertChildElement(stat);
		
		// TODO: другая статистика
		
		server->routeStanza(iq);
		delete iq;
		
		return;
	}
	
	handleIQUnknown(stanza);
}

/**
* XEP-0049: Private XML Storage
*/
void VirtualHost::handleIQPrivateStorage(Stanza stanza)
{
	mawarWarning("Served incoming private storage request");
	
	if ( stanza->getAttribute("type") == "get" )
	{
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("from", name);
		iq->setAttribute("to", stanza.from().full());
		iq->setAttribute("type", "result");
		iq->setAttribute("id", stanza->getAttribute("id", ""));
		TagHelper query = iq["query"];
		query->setDefaultNameSpaceAttribute("jabber:iq:private");
		
		TagHelper block = stanza["query"]->firstChild(); // запрашиваемый блок
		DB::result r = db.query("SELECT block_data FROM private_storage WHERE username = %s AND block_xmlns = %s", db.quote(stanza.from().username()).c_str(), db.quote(block->getAttribute("xmlns")).c_str());
		if(!r.eof()){
			ATXmlTag *res = parse_xml_string("<?xml version=\"1.0\" ?>\n" + r["block_data"]);
			iq["query"]->insertChildElement(res);
		}
		getClientByJid(stanza.from())->sendStanza(iq);
		r.free();
		return;
	}
	else if ( stanza->getAttribute("type") == "set" )
	{
		TagHelper block = stanza["query"]->firstChild();
		db.query("REPLACE INTO private_storage (username, block_xmlns, block_data) VALUES (%s, %s, %s)", db.quote(stanza.from().username()).c_str(), db.quote(block->getAttribute("xmlns")).c_str(), db.quote(block->asString()).c_str());
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("from", name);
		iq->setAttribute("to", stanza.from().full());
		iq->setAttribute("type", "result");
		if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
		TagHelper query = iq["query"];
		query->setDefaultNameSpaceAttribute("jabber:iq:private");
		getClientByJid(stanza.from())->sendStanza(iq);
		return;
	}
	
	handleIQUnknown(stanza);
}

/**
* XEP-0050: Ad-Hoc Commands
*
* TODO разбить эту портянку кода на несколько функций
* провести ревизию на предмет разыменования NULL и утечек памяти
*/
void VirtualHost::handleIQAdHocCommands(Stanza stanza)
{
	if( ! isAdmin(stanza.from().bare()) )
	{
		handleIQForbidden(stanza);
		return;
	}
	
	Command *cmd = new Command(stanza->getChild("command"));
	// TODO segfault possible
	if ( ! cmd )
	{
		handleIQUnknown(stanza);
		return;
	}
	
	if ( cmd->action() == "cancel" )
	{
		// Отмена любой команды
		Stanza reply = Command::commandCancelledStanza(name, stanza);
		server->routeStanza(reply);
		delete reply;
		delete cmd;
		return;
	}
	
	std::string node = cmd->node();
	
	if ( node == "enable_registration" )
	{
		handleIQAdHocEnableRegistration(stanza);
		return;
	}
	
	if ( node == "stop" )
	{
		Stanza reply = Command::commandDoneStanza(name, stanza);
		server->routeStanza(reply);
		delete reply;
		delete cmd;
		mawarWarning("Stopping daemon by request from administrator");
		exit(0); // TODO: сделать корректный останов
		return;
	}
	
	if ( node == "create-vhost" )
	{
		if ( cmd->form() )
		{
			// Обработчик формы тут
			Stanza reply = Command::commandDoneStanza(name, stanza);
			server->routeStanza(reply);
			delete reply;
			delete cmd;
			return;
		}
		Command *reply = new Command();
		reply->setNode(node);
		reply->setStatus("executing");
		reply->createForm("form");
		reply->form()->setTitle("Create a virtual host");
		reply->form()->insertLineEdit("vhost-name", "Host name", "", true);
		reply->form()->insertLineEdit("db-hostname", "MySQL server", "", true);
		reply->form()->insertLineEdit("db-name", "MySQL db", "", true);
		reply->form()->insertLineEdit("db-user", "MySQL user", "", true);
		reply->form()->insertLineEdit("db-passwd", "MySQL password", "", true);
		server->routeStanza(reply->asIqStanza(name, stanza.from().full(), "result", stanza.id()));
		delete reply;
		delete cmd;
		
		return;
	}
	
	if ( node == "drop-vhost" )
	{
		if ( cmd->form() )
		{
			mawarWarning("Delete virtual host: " + cmd->form()->getFieldValue("vhost-name", ""));
			Stanza reply = Command::commandDoneStanza(name, stanza);
			server->routeStanza(reply);
			delete reply;
			delete cmd;
			return;
		}
		Command *reply = new Command();
		reply->setNode(node);
		reply->setStatus("executing");
		reply->createForm("form");
		reply->form()->setTitle("Delete a virtual host");
		reply->form()->insertLineEdit("vhost-name", "Host name", "", true);
		server->routeStanza(reply->asIqStanza(name, stanza.from().full(), "result", stanza.id()));
		delete reply;
		delete cmd;
		
		return;
	} 
	
	if ( node == "stop-vhost" || node == "start-vhost" )
	{
		if ( cmd->form() )
		{
			// Кстати, если что-то в присланных данных неверно, можно сделать проверку типа такой:
			bool valid = true; // установить флаг верности
			if ( valid )
			{
				// Обработчик формы тут
				Stanza reply = Command::commandDoneStanza(name, stanza);
				server->routeStanza(reply);
				delete reply;
				delete cmd;
				return;
			}
			// Это приведёт к показу формы заново… Как в веб :)
			// © WST
		}
		
		Command *reply = new Command();
		reply->setNode(node);
		reply->setStatus("executing");
		reply->createForm("form");
		reply->form()->setTitle("Start/stop a virtual host");
		reply->form()->insertLineEdit("vhost-name", "Host name", "", true);
		server->routeStanza(reply->asIqStanza(name, stanza.from().full(), "result", stanza.id()));
		delete reply;
		delete cmd;
		
		return;
	}
	
	if ( node == "route-stanza" )
	{
		if( cmd->form() )
		{
			// parse_xml_string должно возвращать 0 при ошибках парсинга, что возвращается я ХЗ © WST
			ATXmlTag *custom_tag = parse_xml_string("<?xml version=\"1.0\" ?>\n" + cmd->form()->getFieldValue("rawxml", ""));
			if ( custom_tag )
			{
				Stanza custom_stanza = custom_tag;
				mawarWarning("Routing admin’s custom stanza");
				server->routeStanza(custom_stanza);
				delete custom_stanza;
			}
			else
			{
				mawarWarning("Failed to parse admin’s custom stanza");
			}
			Stanza reply = Command::commandDoneStanza(name, stanza);
			server->routeStanza(reply);
			delete reply;
			delete cmd;
			
			return;
		}
		Command *reply = new Command();
		reply->setNode(node);
		reply->setStatus("executing");
		reply->createForm("form");
		reply->form()->setTitle("Push stanza to the router");
		reply->form()->insertTextEdit("rawxml", "Stanza", "", true);
		server->routeStanza(reply->asIqStanza(name, stanza.from().full(), "result", stanza.id()));
		delete reply;
		delete cmd;
		
		return;
	}
	
	// TODO ??? может handleIQUnknown(stanza); ???
	Stanza reply = Command::commandDoneStanza(name, stanza);
	server->routeStanza(reply);
	delete reply;
	delete cmd;
}

/**
* XEP-0054: vcard-temp
*/
void VirtualHost::handleIQVCardTemp(Stanza stanza)
{
	if ( ! stanza->hasAttribute("to") )
	{
		// Обращение пользователя к собственной vcard
		// vcard-temp
		// http://xmpp.org/extensions/xep-0054.html
		if ( stanza.type() == "get" )
		{
			// Получение собственной vcard
			// If no vCard exists, the server MUST return a stanza error (which SHOULD be <item-not-found/>)
			// or an IQ-result containing an empty <vCard/> element.
			vcard_queries++;
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("to", stanza.from().full());
			iq->setAttribute("type", "result");
			if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
			
			DB::result r = db.query("SELECT vcard_data FROM vcard WHERE id_user = %d", getUserId(stanza.from().username()));
			if( r.eof() )
			{
				// Вернуть пустой vcard
				ATXmlTag *vCard = new ATXmlTag("vCard");
				vCard->setDefaultNameSpaceAttribute("vcard-temp");
				iq->insertChildElement(vCard);
			}
			else
			{
				iq->insertChildElement(parse_xml_string("<?xml version=\"1.0\" ?>\n" + r["vcard_data"]));
			}
			r.free();
			server->routeStanza(iq);
			delete iq;
			return;
		}
		else if ( stanza.type() == "set" )
		{
			// Установка собственной vcard
			std::string data = stanza["vCard"]->asString();
			if( data.length() > 61440 )
			{
				Stanza error = Stanza::iqError(stanza, "resouce-constraint", "cancel");
				error.setFrom(name);
				server->routeStanza(error);
				delete error;
				return;
			}
			db.query("REPLACE INTO vcard (id_user, vcard_data) VALUES (%d, %s)", getUserId(stanza.from().username()), db.quote(data).c_str());
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("to", stanza.from().full());
			iq->setAttribute("type", "result");
			iq->setAttribute("id", stanza->getAttribute("id", ""));
			ATXmlTag *vCard = new ATXmlTag("vCard");
			vCard->setDefaultNameSpaceAttribute("vcard-temp");
			iq->insertChildElement(vCard);
			server->routeStanza(iq);
			delete iq;
			return;
		}
	}
	else
	{
		// Запрос чужого vcard (или своего способом псей)
		// If no vCard exists, the server MUST return a stanza error (which SHOULD be <item-not-found/>)
		// or an IQ-result containing an empty <vCard/> element.
		vcard_queries++;
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("to", stanza.from().full());
		iq->setAttribute("from", stanza.to().bare());
		iq->setAttribute("type", "result");
		iq->setAttribute("id", stanza->getAttribute("id", ""));
		
		if( stanza.to().bare() == hostname() )
		{
			// запрос вкарда сервера
			iq->insertChildElement(parse_xml_string("<?xml version=\"1.0\" ?><vCard xmlns=\"vcard-temp\"><FN>Mawar Jabber/XMPP daemon</FN><NICKNAME>@}-&gt;--</NICKNAME><BDAY>2010-06-13</BDAY><ADR><HOME/><LOCALITY>Tangerang</LOCALITY><REGION>Banten</REGION><CTRY>Indonesia</CTRY></ADR><TITLE>Server</TITLE><ORG><ORGNAME>SmartCommunity</ORGNAME><ORGUNIT>Jabber/XMPP</ORGUNIT></ORG><URL>http://mawar.jsmart.web.id</URL></vCard>"));
		}
		else
		{
			DB::result r = db.query("SELECT vcard_data FROM vcard WHERE id_user = (SELECT id_user FROM users WHERE user_login = %s)", db.quote(stanza.to().username()).c_str());
			if( r.eof() )
			{
				// Вернуть пустой vcard
				ATXmlTag *vCard = new ATXmlTag("vCard");
				vCard->setDefaultNameSpaceAttribute("vcard-temp");
				iq->insertChildElement(vCard);
			}
			else
			{
				iq->insertChildElement(parse_xml_string("<?xml version=\"1.0\" ?>\n" + r["vcard_data"]));
			}
			r.free();
		}
		server->routeStanza(iq);
		delete iq;
		return;
	}
	
	handleIQUnknown(stanza);
}

/**
* XEP-0092: Software Version
*/
void VirtualHost::handleIQVersion(Stanza stanza)
{
	if( stanza->getAttribute("type") == "get" )
	{
		version_requests++;
		mawarWarning("Served incoming version request");
		Stanza version = Stanza::serverVersion(hostname(), stanza.from(), stanza.id());
		server->routeStanza(version);
		delete version;
		return;
	}
	
	handleIQUnknown(stanza);
}

/**
* XEP-0199: XMPP Ping
*/
void VirtualHost::handleIQPing(Stanza stanza)
{
	if ( stanza->getAttribute("type") == "get" )
	{
		xmpp_ping_queries++;
		Stanza result = new ATXmlTag("iq");
		result->setAttribute("from", stanza.to().full());
		result->setAttribute("to", stanza.from().full());
		result->setAttribute("type", "result");
		result->setAttribute("id", stanza->getAttribute("id", ""));
		server->routeStanza(result);
		delete result;
		return;
	}
	
	handleIQUnknown(stanza);
}

/**
* Обработка запрещенной IQ-станзы (недостаточно прав)
*/
void VirtualHost::handleIQForbidden(Stanza stanza)
{
	if ( stanza->getAttribute("type", "") == "error" ) return;
	if ( stanza->getAttribute("type", "") == "result" ) return;
	
	xmpp_error_queries++;
	
	Stanza result = new ATXmlTag("iq");
	result->setAttribute("from", stanza.to().full());
	result->setAttribute("to", stanza.from().full());
	result->setAttribute("type", "error");
	result->setAttribute("id", stanza->getAttribute("id", ""));
		Stanza error = result["error"];
		error->setAttribute("type", "auth");
		error->setAttribute("code", "403");
		error["forbidden"]->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-stanzas");
	
	server->routeStanza(result);
	delete result;
}

/**
* Обработка неизвестной IQ-станзы
*/
void VirtualHost::handleIQUnknown(Stanza stanza)
{
	if ( stanza->getAttribute("type", "") == "error" ) return;
	if ( stanza->getAttribute("type", "") == "result" ) return;
	
	xmpp_error_queries++;
	
	Stanza result = new ATXmlTag("iq");
	result->setAttribute("from", stanza.to().full());
	result->setAttribute("to", stanza.from().full());
	result->setAttribute("type", "error");
	result->setAttribute("id", stanza->getAttribute("id", ""));
		Stanza error = result["error"];
		error->setAttribute("type", "cancel");
		error->setAttribute("code", "501");
		error["feature-not-implemented"]->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-stanzas");
	
	server->routeStanza(result);
	delete result;
}

/**
* Enable/disable user registration command
*
* XEP-0050: Ad-Hoc Commands
*/
void VirtualHost::handleIQAdHocEnableRegistration(Stanza stanza)
{
	Stanza form = stanza["command"]->firstChild("x");
	if ( form && form->getAttribute("xmlns") == "jabber:x:data" )
	{
		string value = form["field"]["value"]->getCharacterData();
		if ( value == "yes" )
		{
			registration_allowed = true;
			
			Stanza reply = Command::commandDoneStanza(name, stanza);
			server->routeStanza(reply);
			delete reply;
			return;
		}
		else if ( value == "no" )
		{
			registration_allowed = false;
			
			Stanza reply = Command::commandDoneStanza(name, stanza);
			server->routeStanza(reply);
			delete reply;
			return;
		}
	}
	
	Command *reply = new Command();
	reply->setNode("enable_registration");
	reply->setStatus("executing");
	reply->createForm("form");
	reply->form()->setTitle("Enable/disable user registation");
	reply->form()->insertLineEdit("enabe-registation", "Enable registation (yes/no)", registration_allowed ? "yes" : "no", true);
	server->routeStanza(reply->asIqStanza(name, stanza.from().full(), "result", stanza.id()));
	delete reply;
}

/**
* Найти клиента по JID (thread-safe)
*
* @note возможно в нем отпадет необходимость по завершении routeStanza()
*/
XMPPClient *VirtualHost::getClientByJid(const JID &jid) {
	mutex.lock();
		sessions_t::iterator it = onliners.find(jid.username());
		if(it != onliners.end()) {
			reslist_t::iterator jt = it->second.find(jid.resource());
			if(jt != it->second.end()) {
				mutex.unlock(); // NB: unlock перед каждым return!
				return jt->second;
			}
		}
	mutex.unlock();
	return 0;
}

void VirtualHost::sendOfflineMessages(XMPPClient *client) {
	DB::result r = db.query("SELECT * FROM spool WHERE message_to = %s ORDER BY message_when ASC", db.quote(client->jid().bare()).c_str());
	for(; !r.eof(); r.next())
	{
		Stanza msg = parse_xml_string("<?xml version=\"1.0\" ?>\n" + r["message_stanza"]);
		if ( msg )
		{
			// TODO вставить отметку времени © WST
			client->sendStanza(msg);
			delete msg;
		}
	}
	r.free();
	db.query("DELETE FROM spool WHERE message_to = %s", db.quote(client->jid().bare()).c_str());
}

/**
* Событие: Пользователь появился в online (thread-safe)
* @param client поток
*/
void VirtualHost::onOnline(XMPPClient *client)
{
	// NOTE вызывается только при успешной инициализации session (после bind)
	mutex.lock();
	sessions_t::iterator user = onliners.find(client->jid().username());
	if(user != onliners.end()) {
		reslist_t::iterator resource = user->second.find(client->jid().resource());
		if(resource != user->second.end()) {
			//replaced by new connection
			Stanza offline = parse_xml_string("<?xml version=\"1.0\"\n<presence type=\"unavailable\"><status>Replaced by new connection</status></presence>");
			onliners[client->jid().username()][client->jid().resource()]->handleUnavailablePresence(offline);
			onliners[client->jid().username()][client->jid().resource()]->terminate();
			delete offline;
			delete onliners[client->jid().username()][client->jid().resource()]; // если удалять элемент карты, в деструкторе XMPPClient вызовется повторное удаление!
			user->second[client->jid().resource()] = client;
			// Число онлайнов не изменилось, onliners_number менять не надо
		} else {
			user->second[client->jid().resource()] = client;
			onliners_number++;
		}
	} else {
		onliners[client->jid().username()][client->jid().resource()] = client;
		onliners_number++;
	}
	mutex.unlock();
	sendOfflineMessages(client);
}

/**
* Отправить Presence Unavailable всем, кому был отправлен Directed Presence
* @param client клиент, который отправлял Directed Presence
*/
void VirtualHost::unavailableDirectedPresence(XMPPClient *client)
{
	DB::result r = db.query("SELECT contact_jid FROM dp_spool WHERE user_jid = %s",
		db.quote(client->jid().full()).c_str()
		);
	if ( r )
	{
		Stanza presence = new ATXmlTag("presence");
		presence->setAttribute("type", "unavailable");
		presence->setAttribute("from", client->jid().full());
		while ( ! r.eof() )
		{
			presence->setAttribute("to", r["contact_jid"]);
			server->routeStanza(presence);
			r.next();
		}
		delete presence;
		r.free();
	}
	
	db.query("DELETE FROM dp_spool WHERE user_jid = %s",
		db.quote(client->jid().full()).c_str()
		);
}

/**
* Событие: Пользователь ушел в offline (thread-safe)
* @param client поток
*/
void VirtualHost::onOffline(XMPPClient *client)
{
	unavailableDirectedPresence(client);
	
	// TODO presence broadcast (unavailable)
	mutex.lock();
		if(client->isActive()) {
			onliners_number--;
			onliners[client->jid().username()].erase(client->jid().resource());
			if(onliners[client->jid().username()].empty()) {
				// Если карта ресурсов пуста, то соответствующий элемент вышестоящей карты нужно удалить © WST
				onliners.erase(client->jid().username());
			}
		}
	mutex.unlock();
}

/**
* Вернуть пароль пользователя по логину
* @param realm домен
* @param login логин пользователя
* @return пароль пользователя или "" если нет такого пользователя
*/
std::string VirtualHost::getUserPassword(const std::string &realm, const std::string &login)
{
	DB::result r = db.query("SELECT user_password FROM users WHERE user_login = %s", db.quote(login).c_str());
	string pwd = r.eof() ? string() : r["user_password"];
	r.free();
	return pwd;
}

/**
* Вернуть ID пользователя
* @param login логин пользователя
* @return ID пользователя
*/
int VirtualHost::getUserId(const std::string &login) {
	DB::result r = db.query("SELECT id_user FROM users WHERE user_login = %s", db.quote(login).c_str());
	int user_id = r.eof() ? 0 : atoi(r["id_user"].c_str());
	r.free();
	return user_id;
}

/**
* Роутер исходящих станз (thread-safe)
*
* Роутер передает станзу нужному потоку.
*
* @note Данная функция отвечает только за маршрутизацию, она не сохраняет офлайновые сообщения:
*   если адресат online, то пересылает ему станзу,
*   если offline, то вернет FALSE и вызывающая сторона должна сама сохранить офлайновое сообщение.
*
* @note Данный метод вызывается из глобального маршрутизатора станз XMPPServer::routeStanza()
*   вызывать его напрямую из других мест не рекомендуется - используйте XMPPServer::routeStanza()
*
* @note Данный метод в будущем станет виртуальным и будет перенесен в специальный
*   базовый класс, от которого будут наследовать VirtualHost (виртуальные узлы)
*   и, к примеру, MUC. Виртуальые узлы и MUC имеют общие черты, оба адресуются
*   доменом, оба принимают входящие станзы, но обрабатывают их по разному,
*   VirtualHost доставляет сообщения своим пользователям, а MUC доставляет
*   сообщения участникам комнат.
*
* @param stanza станза
* @return TRUE - станза была отправлена, FALSE - станзу отправить не удалось
*/
bool VirtualHost::routeStanza(Stanza stanza)
{
	if ( stanza->name() == "iq" )
	{
		handleVHostIq(stanza);
		return true;
	}
	
	if ( stanza->name() == "message" ) {
		handleMessage(stanza);
		return true;
	}
	
	if ( stanza->name() == "presence" ) {
		servePresence(stanza);
		return true;
	}
	
	XMPPClient *client = 0;
	
	// TODO корректный роутинг станз возможно на основе анализа типа станзы
	// NB код марштрутизации должен быть thread-safe, getClientByJID сейчас thread-safe
	client = getClientByJid(stanza.to());
	
	if ( client ) {
		client->sendStanza(stanza);
		return true;
	}
	
	if ( stanza->name() == "iq" && (stanza->getAttribute("type", "") == "get" || stanza->getAttribute("type", "") == "set") ) {
		Stanza error = Stanza::iqError(stanza, "service-unavailable", "cancel");
		server->routeStanza(stanza.from().hostname(), error);
		delete error;
		return true;
	}
	
	return false;
}

/**
* Roster Push
*
* Отправить станзу всем активным ресурсам пользователя
* @NOTE если ресурс не запрашивал ростер, то отправлять не нужно
* @param username логин пользователя которому надо отправить
* @param stanza станза которую надо отправить
*/
void VirtualHost::rosterPush(const std::string &username, Stanza stanza)
{
	mutex.lock();
		sessions_t::iterator user = onliners.find(username);
		if( user != onliners.end() )
		{
			for(reslist_t::const_iterator iter = user->second.begin(); iter != user->second.end(); ++iter)
			{
				//stanza->setAttribute("to", iter->second->jid().full());
				iter->second->sendStanza(stanza);
			}
		}
	mutex.unlock();
}

/**
* XEP-0077: In-Band Registration
*
* Регистрация пользователей
* Вызывается из XMPPClient::onIqStanza() и VirtualHost::handleVHostIq()
* во втором случае client = 0
* 
* TODO нужна ревизия, во-первых, нужно разбить эту портянку на несколько
* небольших функций, во-вторых, желательно вынести в отдельный модуль.
* На данный момент вынести в отдельный модуль не представляется возможным.
* Проблема в том, что если клиент ещё неавторизован, то в станзе-запросе
* поле from неполное и невозможно найти автора запроса без вспомогательного
* XMPPClient
*/
void VirtualHost::handleRegisterIq(XMPPClient *client, Stanza stanza) {
	if(!registration_allowed) {
		Stanza error = Stanza::iqError(stanza, "forbidden", "cancel");
		error->setAttribute("from", hostname());
		if(client) {
			client->sendStanza(error);
		} else {
			error->setAttribute("to", stanza.to().full());
			server->routeStanza(error);
		}
		delete error;
		return;
	}
	if(stanza.type() == "get") {
		// Запрос регистрационной формы
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("from", name);
		iq->setAttribute("type", "result");
		if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
		TagHelper query = iq["query"];
		query->setDefaultNameSpaceAttribute("jabber:iq:register");
		ATXmlTag *instructions = new ATXmlTag("instructions");
		instructions->insertCharacterData("Choose a username and password for use with this service.");
		ATXmlTag *username = new ATXmlTag("username");
		ATXmlTag *password = new ATXmlTag("password");
		query->insertChildElement(instructions);
		query->insertChildElement(username);
		query->insertChildElement(password);
		
		if(client) {
			client->sendStanza(iq);
		} else {
			iq->setAttribute("to", stanza.to().full());
			server->routeStanza(iq);
		}
		delete iq;
	}
	else { // set
		// Клиент прислал регистрационную информацию
		ATXmlTag *username = stanza->find("query/username");
		ATXmlTag *password = stanza->find("query/password");
		ATXmlTag *remove = stanza->find("query/remove");
		if(remove != 0 && client != 0) {
			// Запрошено удаление учётной записи
			db.query("DELETE FROM users WHERE user_login = %s", db.quote(client->jid().username()).c_str());
			// TODO: удалять мусор из других таблиц
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("type", "result");
			if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
			if(client) {
				client->sendStanza(iq);
			} else {
				iq->setAttribute("to", stanza.to().full());
				server->routeStanza(iq);
			}
			delete iq;
			return;
		}
		if(username != 0 && password != 0) {
			// Запрошена регистрация
			
			if(client != 0 && client->isAuthorized()) {
				Stanza iq = new ATXmlTag("iq");
				iq->setAttribute("from", name);
				iq->setAttribute("type", "result");
				if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
				TagHelper query = iq["query"];
				query->setDefaultNameSpaceAttribute("jabber:iq:register");
				ATXmlTag *registered = new ATXmlTag("registered");
				ATXmlTag *username = new ATXmlTag("username"); // TODO: вписать сюда имя активного юзера
				ATXmlTag *password = new ATXmlTag("password"); // TODO: вписать пасс активного юзера
				query->insertChildElement(registered);
				query->insertChildElement(username);
				query->insertChildElement(password);
				if(client) {
					client->sendStanza(iq);
				} else {
					iq->setAttribute("to", stanza.to().full());
					server->routeStanza(iq);
				}
				delete iq;
				return;
			}
			DB::result r = db.query("SELECT count(*) AS cnt FROM users WHERE user_login = %s", db.quote(username->getCharacterData()).c_str());
			bool exists = r["cnt"] == "1";
			r.free();
			
			if(!exists) {
				// Новый пользователь
				if(!verifyUsername(username->getCharacterData()) || password->getCharacterData().empty()) {
					Stanza error = Stanza::iqError(stanza, "forbidden", "cancel");
					if(client) {
						client->sendStanza(error);
					} else {
						error->setAttribute("to", stanza.to().full());
						server->routeStanza(error);
					}
					delete error;
					return;
				}
				db.query("INSERT INTO users (user_login, user_password) VALUES (%s, %s)", db.quote(username->getCharacterData()).c_str(), db.quote(password->getCharacterData()).c_str());
				Stanza iq = new ATXmlTag("iq");
				iq->setAttribute("type", "result");
				if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
				if(client) {
					client->sendStanza(iq);
				} else {
					iq->setAttribute("to", stanza.to().full());
					server->routeStanza(iq);
				}
				delete iq;
			} else {
				// Пользователь с таким именем уже существует
				Stanza error = Stanza::iqError(stanza, "conflict", "cancel");
				if(client) {
					client->sendStanza(error);
				} else {
					error->setAttribute("to", stanza.to().full());
					server->routeStanza(error);
				}
				delete error;
				return;
			}
		}
	}
}

bool VirtualHost::isAdmin(std::string barejid) {
	if(!config->hasChild("admins")) {
		return false;
	}
	return (bool) config->getChild("admins")->getChildByAttribute("admin", "jid", barejid);
}

/**
* Добавить расширение
*/
void VirtualHost::addExtension(const char *urn, const char *fname)
{
	ext[urn] = new XMPPExtension(server, urn, fname);
}

/**
* Удалить расширение
*/
void VirtualHost::removeExtension(const char *urn)
{
	ext.erase(urn);
}
