
#include <virtualhost.h>
#include <configfile.h>
#include <taghelper.h>
#include <attagparser.h>
#include <string>
#include <iostream>
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
	DB::result r = db.query("SELECT * FROM roster WHERE id_user = %d AND contact_jid = %s",
		client->userId(),
		db.quote(item->getAttribute("jid")).c_str()
		);
	if ( r.eof() ) {
		// добавить контакт
		r.free();
		db.query("INSERT INTO roster (id_user, contact_jid, contact_nick, contact_group, contact_subscription, contact_pending) VALUES (%d, %s, %s, %s, 'N', 'N')",
			client->userId(), // id_user
			db.quote(item->getAttribute("jid")).c_str(), // contact_jid
			db.quote(item->getAttribute("name")).c_str(), // contact_nick
			db.quote(item["group"]).c_str() // contact_group
			);
		
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
	} else {
		// обновить контакт
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
		int contact_id = atoi(r["id_contact"].c_str());
		r.free();
		db.query("UPDATE roster SET contact_nick = %s, contact_group = %s WHERE id_contact = %d",
			db.quote(name).c_str(),
			db.quote(group).c_str(),
			contact_id
			);
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("to", "");
		iq->setAttribute("type", "set");
		iq->setAttribute("id", "23234434342"); // random ?
		TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:roster");
			TagHelper item2 = query["item"];
			item2->setAttribute("jid", r["contact_jid"]);
			item2->setAttribute("name", name);
			item2->setAttribute("subscription", subscription);
			if ( group != "" ) item2["group"] = group;
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
*/
void VirtualHost::handleVHostIq(Stanza stanza) {
	if(stanza->hasChild("vCard")) {
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
				ATXmlTag *vCard = new ATXmlTag("vCard");
				vCard->setDefaultNameSpaceAttribute("vcard-temp");
				iq->insertChildElement(vCard);
			} else {
				iq->insertChildElement(parse_xml_string("<?xml version=\"1.0\" ?>\n" + r["vcard_data"]));
			}
			r.free();
			routeStanza(iq);
				
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
			routeStanza(iq);
		}
	}
	else if(stanza->hasChild("query")) {
		std::string query_xmlns = stanza["query"]->getAttribute("xmlns");
		if(stanza.type() == "get") {
			// Входящие запросы информации
			if(query_xmlns == "jabber:iq:version") {
				Stanza version = Stanza::serverVersion(hostname(), stanza.from(), stanza.id());
				getClientByJid(stanza.from())->sendStanza(version);
				delete version;
				return;
			}
			
			if(query_xmlns == "http://jabber.org/protocol/disco#info") {
				// TODO
			}
			
			if(query_xmlns == "http://jabber.org/protocol/disco#items") {
				// TODO
			}
			
			if(query_xmlns == "jabber:iq:roster") {
				sendRoster(stanza);
			}
			
			if(query_xmlns == "jabber:iq:private") { // private storage
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
			}
		} else { // set
			if(query_xmlns == "jabber:iq:private") { // private storage
				TagHelper block = stanza["query"]->firstChild();
				db.query("DELETE FROM private_storage WHERE id_user = %d AND block_xmlns = %s", id_users[stanza.from().username()], db.quote(block->getAttribute("xmlns")).c_str());
				db.query("INSERT INTO private_storage (id_user, block_xmlns, block_data) VALUES (%d, %s, %s)", id_users[stanza.from().username()], db.quote(block->getAttribute("xmlns")).c_str(), db.quote(block->asString()).c_str());
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
* Обработка станзы presence
* @todo обработка атрибута type
*/
void VirtualHost::handlePresence(Stanza stanza) {
	if ( stanza->getAttribute("type", "") == "error" ) {
		// TODO somethink...
		//return;
	}
	
	if ( stanza->getAttribute("type", "") == "probe" ) {
		cout << "probe " << stanza.to().full() << " from " << stanza.from().full() << endl;
		DB::result r = db.query(
			"SELECT count(*) AS cnt FROM roster JOIN users ON roster.id_user = users.id_user "
			"WHERE user_login = %s AND contact_jid = %s AND contact_subscription IN ('F', 'B')",
				db.quote(stanza.to().username()).c_str(), db.quote(stanza.from().bare()).c_str());
		cout << "status: " << r["cnt"] << endl;
		if ( r["cnt"] != "0" ) {
			//mutex.lock();
				sessions_t::iterator it = onliners.find(stanza.to().username());
				if(it != onliners.end()) {
					for(reslist_t::iterator jt = it->second.begin(); jt != it->second.end(); ++jt)
					{
						Stanza p = Stanza::presence(jt->second->jid(), stanza.from(), jt->second->presence());
						server->routeStanza(p.to().hostname(), p);
						delete p;
					}
				}
			//mutex.unlock();
		}
		r.free();
		return;
	}
	
	if ( stanza->getAttribute("type", "") == "subscribed" ) {
		handleSubscribed(stanza);
		return;
	}
	
	if ( stanza.to().resource() == "" ) {
		mutex.lock();
			sessions_t::iterator it = onliners.find(stanza.to().username());
			if(it != onliners.end()) {
				for(reslist_t::iterator jt = it->second.begin(); jt != it->second.end(); ++jt)
				{
					stanza->setAttribute("to", jt->second->jid().full());
					jt->second->sendStanza(stanza);
				}
			}
		mutex.unlock();
	} else {
		XMPPClient *client = getClientByJid(stanza.to());
		if ( client ) client->sendStanza(stanza);
	}
}

/**
* Обработать presence[type=subscribed]
*/
void VirtualHost::handleSubscribed(Stanza stanza)
{
	cout << stanza->asString() << endl;
	DB::result r = db.query("SELECT * FROM users WHERE user_login = %s",
		db.quote(stanza.to().username()).c_str()
		);
	if ( r.eof() ) {
		r.free();
		Stanza error = Stanza::presenceError(stanza, "item-not-found", "cancel");
		server->routeStanza(error.to().hostname(), error);
		return;
	}
	int user_id = atoi(r["id_user"].c_str());
	r.free();
	r = db.query("SELECT * FROM roster WHERE id_user = %d AND contact_jid = %s",
		user_id,
		db.quote(stanza.from().bare()).c_str()
		);
	if ( r.eof() ) {
		r.free();
		return ; // ???
	}
	r.free();
}

/**
* Presence Broadcast (RFC 3921, 5.1.2)
*/
void VirtualHost::broadcastPresence(Stanza stanza)
{
	cerr << "broadcast presence\n";
	if ( stanza->hasAttribute("type") && stanza->getAttribute("type", "") != "unavailable" ) {
		// костыль? удаляем атрибут type, из RFC мне не понятно как правильно поступать
		// быть может надо возращать ошибку
		// (с) shade
		stanza->removeAttribute("type");
	}
	DB::result r = db.query("SELECT contact_jid FROM roster JOIN users ON roster.id_user = users.id_user WHERE user_login = %s AND contact_subscription IN ('F', 'B')", db.quote(stanza.from().username()).c_str());
	for(; ! r.eof(); r.next()) {
		stanza->setAttribute("to", r["contact_jid"]);
		server->routeStanza(stanza.to().hostname(), stanza);
	}
	r.free();
}

/**
* Presence Broadcast (RFC 3921, 5.1.1)
*/
void VirtualHost::initialPresence(Stanza stanza)
{
	cerr << "initial presence\n";
	Stanza probe = new ATXmlTag("presence");
	probe->setAttribute("type", "probe");
	probe->setAttribute("from", stanza.from().full());
	DB::result r = db.query("SELECT contact_jid FROM roster JOIN users ON roster.id_user = users.id_user WHERE user_login = %s AND contact_subscription IN ('T', 'B')", db.quote(stanza.from().username()).c_str());
	for(; ! r.eof(); r.next()) {
		probe->setAttribute("to", r["contact_jid"]);
		server->routeStanza(probe.to().hostname(), probe);
	}
	r.free();
	delete probe;
	broadcastPresence(stanza);
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

/**
* Событие: Пользователь появился в online (thread-safe)
* @param client поток
*/
void VirtualHost::onOnline(XMPPClient *client) {
	mutex.lock();
	if(onliners.find(client->jid().username()) != onliners.end()) {
		onliners[client->jid().username()][client->jid().resource()] = client;
	} else {
		reslist_t reslist;
		reslist[client->jid().resource()] = client;
		onliners[client->jid().username()] = reslist;
		DB::result r = db.query("SELECT id_user FROM users WHERE user_login = %s", db.quote(client->jid().username()).c_str());
		id_users[client->jid().username()] = atoi(r["id_user"].c_str());
		r.free();
	}
	mutex.unlock();
	
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
* Событие: Пользователь ушел в offline (thread-safe)
* @param client поток
*/
void VirtualHost::onOffline(XMPPClient *client) {
	mutex.lock();
		onliners[client->jid().username()].erase(client->jid().resource());
		if(onliners[client->jid().username()].empty()) {
			// Если карта ресурсов пуста, то соответствующий элемент вышестоящей карты нужно удалить
			onliners.erase(client->jid().username());
			id_users.erase(client->jid().username());
		}
		//cout << client->jid().full() << " is offline :-(\n";
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
	if(!stanza->hasAttribute("to")) {
		handleVHostIq(stanza);
		return true;
	}
	
	if ( stanza->name() == "message" ) {
		handleMessage(stanza);
		return true;
	}
	
	if ( stanza->name() == "presence" ) {
		handlePresence(stanza);
		return true;
	}
	
	XMPPClient *client = 0;
	
	// TODO корректный роутинг станз возможно на основе анализа типа станзы
	// NB код марштрутизации должен быть thread-safe, getClientByJID сейчас thread-safe
	// NB запросы vcard нужно маршрутизовать не к клиентам, а к вхосту…
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
* Обработка ростера
*/
void VirtualHost::handleRosterIq(XMPPClient *client, Stanza stanza)
{
	TagHelper query = stanza["query"];
	if ( stanza->getAttribute("type") == "get" ) {
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
