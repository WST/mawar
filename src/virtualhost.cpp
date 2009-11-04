
#include <virtualhost.h>
#include <xmppstream.h>
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
	
	bool result = getStreamByJid(stanza.from())->sendStanza(iq);
	delete iq;
	return result;
}

void VirtualHost::addRosterItem(Stanza stanza, std::string jid, std::string name, std::string group) {
	db.query("INSERT INTO roster (contact_jid, contact_nick, contact_group, contact_subscription, contact_pending) VALUES (%s, %s, %s, 'N', 'B')", db.quote(jid).c_str(), db.quote(name).c_str(), db.quote(group).c_str());
	
	Stanza iq = new ATXmlTag("iq");
	TagHelper query = iq["query"];
	query->setDefaultNameSpaceAttribute("jabber:iq:roster");
	ATXmlTag *item = stanza->find("query/item");
	if(item == 0) return;
	item->setAttribute("subscription", "none");
	query->insertChildElement(item);
	getStreamByJid(stanza.from())->sendStanza(iq);
	
	Stanza iqres = new ATXmlTag("iq");
	iqres->setAttribute("type", "result");
	iqres->setAttribute("id", stanza.id());
	getStreamByJid(stanza.from())->sendStanza(iqres);
}

void VirtualHost::handleVHostIq(Stanza stanza) {
	if(stanza->hasChild("query")) {
		std::string query_xmlns = stanza["query"]->getAttribute("xmlns");
		if(stanza.type() == "get") {
			// Входящие запросы информации
			if(query_xmlns == "jabber:iq:version") {
				Stanza version = Stanza::serverVersion(hostname(), stanza.from(), stanza.id());
				getStreamByJid(stanza.from())->sendStanza(version);
				delete version;
				return;
			}
			
			if(query_xmlns == "http://jabber.org/protocol/disco#info") {
				// Информация о возможностях сервера
				Stanza iq = new ATXmlTag("iq");
				iq->setAttribute("from", hostname());
				iq->setAttribute("to", stanza.from().full());
				iq->setAttribute("type", "result");
				if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
				TagHelper query = iq["query"];
					query->setDefaultNameSpaceAttribute("http://jabber.org/protocol/disco#info");
					//	<identity category="server" type="im" name="Mawar" /><feature var="msgoffline" /> (последнее — оффлайновые сообщения, видимо)
					// TODO: при расширении функционала сервера нужно дописывать сюда фичи
					// В том числе и динамические (зависящие от вирт.хоста)
					// Ну и register, конечно
					query["identity"]->setAttribute("category", "server");
					query["identity"]->setAttribute("type", "im");
					query["identity"]->setAttribute("name", "Mawar Jabber/XMPP engine");
					ATXmlTag *feature = new ATXmlTag("feature");
					feature->insertAttribute("var", "jabber:iq:version");
					query->insertChildElement(feature);
				
				getStreamByJid(stanza.from())->sendStanza(iq);
				delete iq;
				return;
			}
			
			if(query_xmlns == "http://jabber.org/protocol/disco#items") {
				// Информация о компонентах сервера и прочей фигне в обзоре служб (команды, транспорты)
				Stanza iq = new ATXmlTag("iq");
				iq->setAttribute("from", hostname());
				iq->setAttribute("to", stanza.from().full());
				iq->setAttribute("type", "result");
				if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
				TagHelper query = iq["query"];
					query->setDefaultNameSpaceAttribute("http://jabber.org/protocol/disco#items");
				
				getStreamByJid(stanza.from())->sendStanza(iq);
				delete iq;
				return;
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
				getStreamByJid(stanza.from())->sendStanza(iq);
				r.free();
			}
		} else { // set
			if(query_xmlns == "jabber:iq:roster") {
				// Обновление элемента ростера — добавление, правка или удаление
				TagHelper query = stanza["query"];
				ATXmlTag *item = stanza->firstChild("query/item");
				if(item != 0) {
					if(!item->hasAttribute("subscription")) {
						addRosterItem(stanza, item->getAttribute("jid"), item->getAttribute("name"), query->getChildValue("group", ""));
					}
				}
				delete item;
			}
			
			if(query_xmlns == "jabber:iq:private") { // private storage
				TagHelper block = stanza["query"]->firstChild();
				db.query("DELETE FROM private_storage WHERE id_user = %d AND block_xmlns = %s", id_users[stanza.from().username()], db.quote(block->getAttribute("xmlns")).c_str());
				db.query("INSERT INTO private_storage (id_user, block_xmlns, block_data) VALUES (%d, %s, %s)", id_users[stanza.from().username()], db.quote(block->getAttribute("xmlns")).c_str(), db.quote(block->asString()).c_str());
				// send result iq
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
		return;
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
		XMPPStream *stream = getStreamByJid(stanza.to());
		if ( stream ) stream->sendStanza(stanza);
	}
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
		std::list<XMPPStream *> sendto_list;
		std::list<XMPPStream *>::iterator kt;
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

void VirtualHost::handleIq(Stanza stanza) {
	if(!stanza->hasAttribute("to") || stanza.to().username().empty()) {
		handleVHostIq(stanza);
		return;
	}
	
	if(stanza.to().resource().empty()) {
		/*
		<iq from="***" type="error" to="**" id="blah" >
		<query xmlns="foo"/>
		<error type="modify" code="400" >
		<bad-request xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/>
		</error>
		</iq>
		*/
	}
	XMPPStream *stream = getStreamByJid(stanza.to());
	if(stream == 0) {
		/*
		<iq from="averkov@jabberid.org/test" type="error" xml:lang="ru-RU" to="admin@underjabber.net.ru/home" id="blah" >
		<query xmlns="foo"/>
		<error type="wait" code="404" >
		<recipient-unavailable xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/>
		</error>
		</iq>
		*/
		return;
	}
	stream->sendStanza(stanza);
}

/**
* Найти поток по JID (thread-safe)
*
* @note возможно в нем отпадет необходимость по завершении routeStanza()
*/
XMPPStream *VirtualHost::getStreamByJid(const JID &jid) {
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
* @param stream поток
*/
void VirtualHost::onOnline(XMPPStream *stream) {
	mutex.lock();
	if(onliners.find(stream->jid().username()) != onliners.end()) {
		onliners[stream->jid().username()][stream->jid().resource()] = stream;
	} else {
		reslist_t reslist;
		reslist[stream->jid().resource()] = stream;
		onliners[stream->jid().username()] = reslist;
		DB::result r = db.query("SELECT id_user FROM users WHERE user_login = %s", db.quote(stream->jid().username()).c_str());
		id_users[stream->jid().username()] = atoi(r["id_user"].c_str());
		r.free();
	}
	mutex.unlock();
	
	DB::result r = db.query("SELECT * FROM spool WHERE message_to = %s ORDER BY message_when ASC", db.quote(stream->jid().bare()).c_str());
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
			stream->sendStanza(msg);
			delete msg;
		}
	}
	r.free();
	db.query("DELETE FROM spool WHERE message_to = %s", db.quote(stream->jid().bare()).c_str());
}

/**
* Событие: Пользователь ушел в offline (thread-safe)
* @param stream поток
*/
void VirtualHost::onOffline(XMPPStream *stream) {
	mutex.lock();
		onliners[stream->jid().username()].erase(stream->jid().resource());
		if(onliners[stream->jid().username()].empty()) {
			// Если карта ресурсов пуста, то соответствующий элемент вышестоящей карты нужно удалить
			onliners.erase(stream->jid().username());
			id_users.erase(stream->jid().username());
		}
		//cout << stream->jid().full() << " is offline :-(\n";
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
	if ( stanza->name() == "message" ) {
		handleMessage(stanza);
		return true;
	}
	
	if ( stanza->name() == "presence" ) {
		handlePresence(stanza);
		return true;
	}
	
	XMPPStream *stream = 0;
	
	// TODO корректный роутинг станз возможно на основе анализа типа станзы
	// NB код марштрутизации должен быть thread-safe, getStreamByJID сейчас thread-safe
	stream = getStreamByJid(stanza.to());
	
	if ( stream ) {
		cerr << "stream found" << endl;
		stream->sendStanza(stanza);
		return true;
	}
	
	cerr << "stream not found" << endl;
	
	return false;
}
