
#include <virtualhost.h>
#include <configfile.h>
#include <taghelper.h>
#include <attagparser.h>
#include <string>
#include <iostream>
#include <stdio.h>
#include <nanosoft/error.h>
#include <nanosoft/gsaslserver.h>
#include <db.h>
#include <time.h>

using namespace std;
using namespace nanosoft;

/**
* Конструктор
* @param srv ссылка на сервер
* @param aName имя хоста
* @param config конфигурация хоста
*/
VirtualHost::VirtualHost(XMPPServer *srv, const std::string &aName, VirtualHostConfig config):
	XMPPDomain(srv, aName)
{
	TagHelper registration = config["registration"];
	registration_allowed = registration->getAttribute("enabled", "no") == "yes";
	
	TagHelper storage = config["storage"];
	if ( storage->getAttribute("engine", "mysql") != "mysql" ) ::error("[VirtualHost] unknown storage engine: " + storage->getAttribute("engine"));
	
	// Подключаемся к БД
	string server = storage["server"];
	if( server.substr(0, 5) == "unix:" ) {
		if ( ! db.connectUnix(server.substr(5), storage["database"], storage["username"], storage["password"]) ) ::error("[VirtualHost] cannot connect to database");
	} else {
		if ( ! db.connect(server, storage["database"], storage["username"], storage["password"]) ) ::error("[VirtualHost] cannot connect to database");
	}
	
	// кодировка только UTF-8
	db.query("SET NAMES UTF8");
}

/**
* Деструктор
*/
VirtualHost::~VirtualHost() {
}

bool VirtualHost::sendRoster(Stanza stanza) {
	Stanza iq = new ATXmlTag("iq");
	iq->setAttribute("from", stanza.from().full());
	iq->setAttribute("to", stanza.from().full());
	if(stanza->hasAttribute("id")) iq->setAttribute("id", stanza.id());
	iq->setAttribute("type", "result");
	TagHelper query = iq["query"];
	query->setDefaultNameSpaceAttribute("jabber:iq:roster");
	
	// Впихнуть элементы ростера тут
	// TODO: ожидание авторизации (pending)
	std::string subscription;
	ATXmlTag *item;
	DB::result r = db.query("SELECT * FROM roster, users WHERE roster.id_user=users.id_user AND users.user_login=%s", db.quote(stanza.from().username()).c_str());
	for(; ! r.eof(); r.next()) {
		if(r["contact_subscription"] == "F") { // from
			subscription = "from";
		} else if(r["contact_subscription"] == "T") { // to
			subscription = "to";
		} else if(r["contact_subscription"] == "B") { // both
			subscription = "both";
		} else { // none
			subscription = "none";
		}
		item = new ATXmlTag("item");
		item->setAttribute("subscription", subscription.c_str());
		item->setAttribute("jid", r["contact_jid"]);
		item->setAttribute("name", r["contact_nick"]);
		query->insertChildElement(item);
	}
	r.free();
	
	bool result = getClientByJid(stanza.from())->sendStanza(iq);
	delete iq;
	return result;
}

/**
* Добавить/обновить контакт в ростере
* @param client клиент чей ростер обновляем
* @param stanza станза управления ростером
* @param item элемент описывающий изменения в контакте
*/
void VirtualHost::setRosterItem(XMPPClient *client, Stanza stanza, TagHelper item) {
	fprintf(stderr, "[VirtualHost: %s]: roster set in %s item %s\n", hostname().c_str(), client->jid().username().c_str(), item->getAttribute("jid").c_str());
	/*
	The server MUST update the roster information in persistent storage,
	and also push the change out to all of the user's available resources
	that have requested the roster.  This "roster push" consists of an IQ
	stanza of type "set" from the server to the client and enables all
	available resources to remain in sync with the server-based roster
	information.
	*/
	fprintf(stderr, "lookup roster item\n");
	DB::result r = db.query("SELECT * FROM roster WHERE id_user = %d AND contact_jid = %s", client->userId(), db.quote(item->getAttribute("jid")).c_str());
	if(r.eof()) {
		// добавить контакт
		r.free();
		fprintf(stderr, "not found, insert\n");
		db.query("INSERT INTO roster (id_user, contact_jid, contact_nick, contact_group, contact_subscription, contact_pending) VALUES (%d, %s, %s, %s, 'N', 'N')",
			client->userId(), // id_user
			db.quote(item->getAttribute("jid")).c_str(), // contact_jid
			db.quote(item->getAttribute("name")).c_str(), // contact_nick
			db.quote(item["group"]).c_str() // contact_group
		);
		fprintf(stderr, "inserted, broadcast roster set\n");
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("to", "");
		iq->setAttribute("type", "set");
		iq->setAttribute("id", "23234434342"); // random ?
		TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:roster");
			TagHelper item2 = query["item"];
			item2->setAttribute("jid", item->getAttribute("jid"));
			item2->setAttribute("name", item->getAttribute("name"));
			item2->setAttribute("subscription", "none");
			item2["group"] = string(item["group"]);
		broadcast(iq, client->jid().username());
		delete iq;
		fprintf(stderr, "broadcased\n");
	} else {
		// обновить контакт
		fprintf(stderr, "found, before update\n");
		std::string subscription;
		if(r["contact_subscription"] == "F") { // from
			subscription = "from";
		} else if(r["contact_subscription"] == "T") { // to
			subscription = "to";
		} else if(r["contact_subscription"] == "B") { // both
			subscription = "both";
		} else { // none
			subscription = "none";
		}
		std::string name = item->hasAttribute("name") ? item->getAttribute("name") : r["contact_nick"];
		std::string group = item->hasChild("group") ? string(item["group"]) : r["contact_group"];
		std::string contact_jid = r["contact_jid"];
		int contact_id = atoi(r["id_contact"].c_str());
		r.free();
		fprintf(stderr, "update\n");
		db.query("UPDATE roster SET contact_nick = %s, contact_group = %s WHERE id_contact = %d",
			db.quote(name).c_str(),
			db.quote(group).c_str(),
			contact_id
			);
		fprintf(stderr, "updated, broadcast roster set\n");
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("to", "");
		iq->setAttribute("type", "set");
		iq->setAttribute("id", "23234434342"); // random ?
		TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:roster");
			TagHelper item2 = query["item"];
			item2->setAttribute("jid", contact_jid);
			item2->setAttribute("name", name);
			item2->setAttribute("subscription", subscription);
			if ( group != "" ) item2["group"] = group;
		broadcast(iq, client->jid().username());
		delete iq;
		fprintf(stderr, "broadcasted\n");
	}
	
	fprintf(stderr, "send resutl\n");
	Stanza result = new ATXmlTag("iq");
	result->setAttribute("to", client->jid().full());
	result->setAttribute("type", "result");
	result->setAttribute("id", stanza->getAttribute("id"));
	client->sendStanza(result);
	delete result;
	fprintf(stderr, "leave\n");
}

/**
* Удалить контакт из ростера
* @param client клиент чей ростер обновляем
* @param stanza станза управления ростером
* @param item элемент описывающий изменения в контакте
*/
void VirtualHost::removeRosterItem(XMPPClient *client, Stanza stanza, TagHelper item)
{
	DB::result r = db.query("SELECT * FROM roster WHERE id_user = %d AND contact_jid = %s",
		client->userId(),
		db.quote(item->getAttribute("jid")).c_str()
		);
	if ( r.eof() ) {
		r.free();
	} else {
		int contact_id = atoi(r["id_contact"].c_str());
		r.free();
		
		db.query("DELETE FROM roster WHERE id_contact = %d", contact_id);
		
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("to", "");
		iq->setAttribute("type", "set");
		iq->setAttribute("id", "23234434342"); // random ?
		TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:roster");
			TagHelper item2 = query["item"];
			item2->setAttribute("jid", item->getAttribute("jid"));
			item2->setAttribute("subscription", "remove");
		broadcast(iq, client->jid().username());
		delete iq;
	}
	
	Stanza result = new ATXmlTag("iq");
	result->setAttribute("to", client->jid().full());
	result->setAttribute("type", "result");
	result->setAttribute("id", stanza->getAttribute("id"));
	client->sendStanza(result);
	delete result;
}

/**
* Информационные запросы без атрибута to
* Адресованные клиентом данному узлу
* TODO: передавать ответы роутеру, сейчас жёстко завязаны на c2s
*/
void VirtualHost::handleVHostIq(Stanza stanza) {
	if(stanza->hasChild("query")) {
		// Входящие запросы информации
		
		std::string query_xmlns = stanza["query"]->getAttribute("xmlns");
		std::string stanza_type = stanza.type();
		
		if(query_xmlns == "jabber:iq:version" && stanza_type == "get") {
			Stanza version = Stanza::serverVersion(hostname(), stanza.from(), stanza.id());
			//getClientByJid(stanza.from())->sendStanza(version); // c2s only
			server->routeStanza(stanza.from().hostname(), version);
			delete version;
			return;
		}
		
		if(query_xmlns == "http://jabber.org/protocol/disco#info" && stanza_type == "get") {
			// TODO
		}
		
		if(query_xmlns == "http://jabber.org/protocol/disco#items" && stanza_type == "get") {
			// TODO
		}
		
		if(query_xmlns == "jabber:iq:private") {
			// private storage
			if(stanza_type == "get") {
				Stanza iq = new ATXmlTag("iq");
				iq->setAttribute("from", name);
				iq->setAttribute("to", stanza.from().full());
				iq->setAttribute("type", "result");
				if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
				TagHelper query = iq["query"];
				query->setDefaultNameSpaceAttribute("jabber:iq:private");
				
				TagHelper block = stanza["query"]->firstChild(); // запрашиваемый блок
				DB::result r = db.query("SELECT block_data FROM private_storage WHERE id_user = %d AND block_xmlns = %s", id_users[stanza.from().username()], db.quote(block->getAttribute("xmlns")).c_str());
				if(!r.eof()) {
					ATXmlTag *res = parse_xml_string("<?xml version=\"1.0\" ?>\n" + r["block_data"]);
					iq["query"]->insertChildElement(res);
				}
				getClientByJid(stanza.from())->sendStanza(iq);
				r.free();
			} else { // set
				TagHelper block = stanza["query"]->firstChild();
				db.query("REPLACE INTO private_storage (id_user, block_xmlns, block_data) VALUES (%d, %s, %s)", id_users[stanza.from().username()], db.quote(block->getAttribute("xmlns")).c_str(), db.quote(block->asString()).c_str());
				Stanza iq = new ATXmlTag("iq");
				iq->setAttribute("from", name);
				iq->setAttribute("to", stanza.from().full());
				iq->setAttribute("type", "result");
				if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
				TagHelper query = iq["query"];
				query->setDefaultNameSpaceAttribute("jabber:iq:private");
				getClientByJid(stanza.from())->sendStanza(iq);
			}
		}
	}
}

/**
* Обслуживание обычного презенса
*/
void VirtualHost::serveCommonPresence(Stanza stanza)
{
	JID to = stanza.to();
	JID from = stanza.from();
	
	fprintf(stderr, "[VirtualHost]: serve common presence from: %s to: %s\n", from.full().c_str(), to.full().c_str());
	
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
* Обслуживание Presence Probes
*
* RFC 3921 (5.1.3) Presence Probes
*/
void VirtualHost::servePresenceProbes(Stanza stanza)
{
	JID to = stanza.to();
	JID from = stanza.from();
	
	fprintf(stderr, "[VirtualHost]: RFC 3921 (5.1.3) Presence Probes from: %s to: %s\n", from.full().c_str(), to.full().c_str());
	
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
* RFC 3921 (8.2.6) Presence Subscribe
*/
void VirtualHost::servePresenceSubscribe(Stanza stanza)
{
	string from = stanza.from().bare();
	string to = stanza.to().username();
	
	fprintf(stderr, "[VirtualHost: %s]: RFC 3921 (8.2.6) Presence Subscribe from: %s to: %s\n", hostname().c_str(), from.c_str(), to.c_str());
	
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
		db.query("INSERT INTO roster (id_user, contact_jid, contact_subscription, contact_pending) VALUES (%d, %s, 'N', 'P')", id_users[to], db.quote(from).c_str());
	}
	r.free();
	
	// TODO broadcast only to whom requested roster
	broadcast(stanza, to);
}

/**
* RFC 3921 (8.2.7) Presence Subscribed
*/
void VirtualHost::servePresenceSubscribed(Stanza stanza)
{
	string from = stanza.from().bare();
	string to = stanza.to().username();
	
	fprintf(stderr, "[VirtualHost: %s]: RFC 3921 (8.2.7) Presence Subscribed from: %s to: %s\n", hostname().c_str(), from.c_str(), to.c_str());
	
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
	fprintf(stderr, "[VirtualHost]: RFC 3921 (5.1.6) Presence Subscriptions\n");
	
	if ( stanza->getAttribute("type") == "subscribe" )
	{
		servePresenceSubscribe(stanza);
		return;
	}
	
	if ( stanza->getAttribute("type") == "subscribed" )
	{
		servePresenceSubscribed(stanza);
		return;
	}
	
	fprintf(stderr, "[VirtualHost]: drop unknown presence subscription: %s\n", stanza->asString().c_str());
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
	
	fprintf(stderr, "[VirtualHost]: drop unknown presence: %s\n", stanza->asString().c_str());
	return;
}

/**
* Обработать presence[type=subscribed]
*/
void VirtualHost::handleSubscribed(Stanza stanza)
{
	cout << stanza->asString() << endl;
	DB::result r = db.query("SELECT * FROM users WHERE user_login = %s", db.quote(stanza.to().username()).c_str());
	if ( r.eof() ) {
		r.free();
		Stanza error = Stanza::presenceError(stanza, "item-not-found", "cancel");
		server->routeStanza(error.to().hostname(), error);
		return;
	}
	int user_id = atoi(r["id_user"].c_str());
	r.free();
	r = db.query("SELECT * FROM roster WHERE id_user = %d AND contact_jid = %s", user_id, db.quote(stanza.from().bare()).c_str());
	if ( r.eof() ) {
		r.free();
		return ; // ???
	}
	r.free();
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

void VirtualHost::saveOfflineMessage(Stanza stanza) {
	db.query("INSERT INTO spool (message_to, message_stanza, message_when) VALUES (%s, %s, %d)", db.quote(stanza.to().bare()).c_str(), db.quote(stanza->asString()).c_str(), time(NULL));
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
			saveOfflineMessage(stanza);
			return;
		}
		for(kt = sendto_list.begin(); kt != sendto_list.end(); kt++) {
			(*kt)->sendStanza(stanza); // TODO — учесть bool
		}
	} else {
		saveOfflineMessage(stanza);
	}
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
			// TODO вставить отметку времени
			// <message from="admin@underjabber.net.ru/home" xml:lang="ru-RU" to="averkov@jabberid.org" id="aef3a" >
			// <subject>test</subject>
			// <body>Test msg</body>
			// <nick xmlns="http://jabber.org/protocol/nick">WST</nick>
			// <x xmlns="jabber:x:delay" stamp="20091025T14:39:32" />
			// </message>
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
	fprintf(stderr, "[VirtualHost]: online %s\n", client->jid().full().c_str());
	
	mutex.lock();
		sessions_t::iterator user = onliners.find(client->jid().username());
		if( user != onliners.end())
		{
			user->second[client->jid().resource()] = client;
		}
		else
		{
			onliners[client->jid().username()][client->jid().resource()] = client;
			DB::result r = db.query("SELECT id_user FROM users WHERE user_login = %s", db.quote(client->jid().username()).c_str());
			id_users[client->jid().username()] = atoi(r["id_user"].c_str());
			r.free();
		}
	mutex.unlock();
	sendOfflineMessages(client);
}

/**
* Событие: Пользователь ушел в offline (thread-safe)
* @param client поток
*/
void VirtualHost::onOffline(XMPPClient *client)
{
	fprintf(stderr, "[VirtualHost]: offline %s\n", client->jid().full().c_str());
	
	mutex.lock();
		onliners[client->jid().username()].erase(client->jid().resource());
		if(onliners[client->jid().username()].empty()) {
			// Если карта ресурсов пуста, то соответствующий элемент вышестоящей карты нужно удалить
			onliners.erase(client->jid().username());
			id_users.erase(client->jid().username());
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
int VirtualHost::getUserId(const std::string &login)
{
	DB::result r = db.query("SELECT id_user FROM users WHERE user_login = %s", db.quote(login).c_str());
	int user_id = r.eof() ? 0 : atoi(r["id_user"].c_str());
	r.free();
	return user_id;
	// есть другой костыль — карта id_users… Она хотя бы быстрее %)
}

void VirtualHost::handleVcardRequest(Stanza stanza) {
	if(!stanza->hasAttribute("to")) {
		// Обращение пользователя к собственной vcard
		// vcard-temp
		// http://xmpp.org/extensions/xep-0054.html
		if(stanza.type() == "get") {
			// Получение собственной vcard
			// If no vCard exists, the server MUST return a stanza error (which SHOULD be <item-not-found/>)
			// or an IQ-result containing an empty <vCard/> element.
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("to", stanza.from().full());
			iq->setAttribute("type", "result");
			if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
			
			DB::result r = db.query("SELECT vcard_data FROM vcard WHERE id_user = %d", id_users[stanza.from().username()]);
			if(r.eof()) {
				// Вернуть пустой vcard
				ATXmlTag *vCard = new ATXmlTag("vCard");
				vCard->setDefaultNameSpaceAttribute("vcard-temp");
				iq->insertChildElement(vCard);
			} else {
				iq->insertChildElement(parse_xml_string("<?xml version=\"1.0\" ?>\n" + r["vcard_data"]));
			}
			r.free();
			server->routeStanza(iq);
			delete iq;
		} else if(stanza.type() == "set") {
			// Установка собственной vcard
			db.query("REPLACE INTO vcard (id_user, vcard_data) VALUES (%d, %s)", id_users[stanza.from().username()], db.quote(stanza["vCard"]->asString()).c_str());
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("to", stanza.from().full());
			iq->setAttribute("type", "result");
			if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
			ATXmlTag *vCard = new ATXmlTag("vCard");
			vCard->setDefaultNameSpaceAttribute("vcard-temp");
			iq->insertChildElement(vCard);
			server->routeStanza(iq);
			delete iq;
		}
	} else {
		// Запрос чужого vcard (или своего способом псей)
		// If no vCard exists, the server MUST return a stanza error (which SHOULD be <item-not-found/>)
		// or an IQ-result containing an empty <vCard/> element.
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("to", stanza.from().full());
		iq->setAttribute("from", stanza.to().bare());
		iq->setAttribute("type", "result");
		if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
		
		DB::result r = db.query("SELECT vcard_data FROM vcard WHERE id_user = (SELECT id_user FROM users WHERE user_login = %s)", db.quote(stanza.to().username()).c_str());
		if(r.eof()) {
			// Вернуть пустой vcard
			ATXmlTag *vCard = new ATXmlTag("vCard");
			vCard->setDefaultNameSpaceAttribute("vcard-temp");
			iq->insertChildElement(vCard);
		} else {
			iq->insertChildElement(parse_xml_string("<?xml version=\"1.0\" ?>\n" + r["vcard_data"]));
		}
		r.free();
		server->routeStanza(iq);
		delete iq;
	}
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
	if(stanza->hasChild("vCard") && (stanza->getAttribute("type", "") == "get" || stanza->getAttribute("type", "") == "set")) {
		// Запрос vcard может как содержать атрибут to, так и не содержать его…
		handleVcardRequest(stanza);
		return true;
	}
	
	if(!stanza->hasAttribute("to") || stanza->getAttribute("to", "") == name) {
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
* Обработка ростера
* Вызывается из XMPPClient::onIqStanza()
*/
void VirtualHost::handleRosterIq(XMPPClient *client, Stanza stanza)
{
	TagHelper query = stanza["query"];
	if ( stanza->getAttribute("type") == "get" ) {
		client->use_roster = true;
		sendRoster(stanza);
		return;
	}
	else if ( stanza->getAttribute("type") == "set" ) {
		TagHelper item = query->firstChild("item");
		while ( item ) {
			if ( item->getAttribute("subscription", "") != "remove" ) setRosterItem(client, stanza, item);
			else removeRosterItem(client, stanza, item);
			item = query->nextChild("item", item);
		}
	}
}

/**
* Регистрация пользователей
* Вызывается из XMPPClient::onIqStanza()
*/
void VirtualHost::handleRegisterIq(XMPPClient *client, Stanza stanza) {
	// TODO: учесть registration_allowed
	if(!registration_allowed) {
		Stanza error = Stanza::iqError(stanza, "forbidden", "cancel");
		server->routeStanza(stanza.from().hostname(), error);
		delete error;
		return;
	}
	if(stanza.type() == "get") {
		// Запрос регистрационной формы
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("from", name);
		iq->setAttribute("to", stanza.from().full());
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
		
		client->sendStanza(iq);
		delete iq;
	}
	else { // set
		// Клиент прислал регистрационную информацию
		ATXmlTag *username = stanza->find("query/username");
		ATXmlTag *password = stanza->find("query/username");
		ATXmlTag *remove = stanza->find("query/remove");
		if(remove != 0) {
			// Запрошено удаление учётной записи
			db.query("DELETE FROM users WHERE user_login = %s", db.quote(client->jid().username()).c_str());
			// TODO: удалять мусор из других таблиц
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("type", "result");
			if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
			client->sendStanza(iq);
			delete iq;
			return;
		}
		if(username != 0 && password != 0) {
			// Запрошена регистрация
			
			if(client->isAuthorized()) {
				Stanza iq = new ATXmlTag("iq");
				iq->setAttribute("from", name);
				iq->setAttribute("to", stanza.from().full());
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
				client->sendStanza(iq);
				delete iq;
				return;
			}
			DB::result r = db.query("SELECT count(*) AS cnt FROM users WHERE user_login = %s", db.quote(username->getCharacterData()).c_str());
			bool exists = r["cnt"] == "1";
			r.free();
			
			if(!exists) {
				// Новый пользователь
				db.query("INSERT INTO users (user_login, user_password) VALUES (%s, %s)", db.quote(username->getCharacterData()).c_str(), db.quote(password->getCharacterData()).c_str());
				Stanza iq = new ATXmlTag("iq");
				iq->setAttribute("type", "result");
				if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
				client->sendStanza(iq);
				delete iq;
			} else {
				// Пользователь с таким именем уже существует
				Stanza iq = new ATXmlTag("iq");
				iq->setAttribute("type", "error");
				if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
				TagHelper query = iq["query"];
				query->setDefaultNameSpaceAttribute("jabber:iq:register");
				query->insertChildElement(username);
				query->insertChildElement(password);
				TagHelper error = iq["error"];
				error->insertAttribute("code", "409");
				error->insertAttribute("type", "cancel");
				ATXmlTag *conflict = new ATXmlTag("conflict");
				conflict->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-stanzas");
				client->sendStanza(iq);
				delete iq;
			}
		}
	}
}
