
#include <virtualhost.h>
#include <xmppstream.h>
#include <configfile.h>
#include <taghelper.h>
#include <string>
#include <iostream>
#include <nanosoft/error.h>
#include <nanosoft/mysql.h>
#include <nanosoft/gsaslserver.h>

using namespace std;
using namespace nanosoft;

/**
* Конструктор
* @param srv ссылка на сервер
* @param aName имя хоста
* @param config конфигурация хоста
*/
VirtualHost::VirtualHost(XMPPServer *srv, const std::string &aName, VirtualHostConfig config): server(srv), name(aName) {
	TagHelper storage = config["storage"];
	if ( storage->getAttribute("engine", "mysql") != "mysql" ) ::error("[VirtualHost] unknown storage engine: " + storage->getAttribute("engine"));
	
	// Подключаемся к БД
	string server = storage["server"];
	if( server.substr(0, 5) == "unix:" ) {
		if ( ! db.connectUnix(server.substr(5), storage["database"], storage["username"], storage["password"]) ) ::error("[VirtualHost] cannot connect to database");
	} else {
		if ( ! db.connectUnix(server, storage["database"], storage["username"], storage["password"]) ) ::error("[VirtualHost] cannot connect to database");
	}
	
	// кодировка только UTF-8
	db.query("SET NAMES UTF8");
}

/**
* Деструктор
*/
VirtualHost::~VirtualHost() {
}

/**
* Вернуть имя хоста
*/
const std::string& VirtualHost::hostname() {
	return name;
}

void VirtualHost::handleVHostIq(Stanza stanza) {
	if(stanza->hasChild("query") && stanza.type() == "get") {
		// Входящие запросы информации
		std::string query_xmlns = stanza["query"]->getAttribute("xmlns");
		
		if(query_xmlns == "jabber:iq:version") {
			Stanza version = Stanza::serverVersion(name, stanza.from(), stanza.id());
			getStreamByJid(stanza.from())->sendStanza(version);
			delete version;
			return;
		}
		
		if(query_xmlns == "http://jabber.org/protocol/disco#info") {
			// Информация о возможностях сервера
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("from", name);
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
			iq->setAttribute("from", name);
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
			Stanza iq = new ATXmlTag("iq");
			//iq->setAttribute("from", name);
			iq->setAttribute("from", stanza.from().full());
			iq->setAttribute("to", stanza.from().full());
			if(stanza->hasAttribute("id")) iq->setAttribute("id", stanza.id());
			iq->setAttribute("type", "result");
			TagHelper query = iq["query"];
				query->setDefaultNameSpaceAttribute("jabber:iq:roster");
				
				// Впихнуть элементы ростера тут
				MySQL::result r = db.query("SELECT user_login FROM users");
				for(; ! r.eof(); r.next()) {
					ATXmlTag *item = new ATXmlTag("item");
					item->setAttribute("subscription", "both");
					item->setAttribute("jid", r["user_login"] + "@" + hostname());
					query->insertChildElement(item);
				}
				r.free();
			
			getStreamByJid(stanza.from())->sendStanza(iq);
			delete iq;
			return;
		}
		
		if(query_xmlns == "jabber:iq:private") { // private storage
			if(stanza["query"]->hasChild("storage") && stanza["query"]["storage"]->getAttribute("xmlns") == "storage:bookmarks") {
				// Выдача закладок
				MySQL::result r = db.query("SELECT b.bookmark_name, b.bookmark_jid, b.bookmark_nick FROM bookmarks AS b, users AS u WHERE u.id_user=b.id_user AND u.user_login=%s", db.quote(stanza.from().username()).c_str());
				for(; !r.eof(); r.next()) {
					//
				}
				r.free();
			}
		}
	}
}

void VirtualHost::handlePresence(Stanza stanza) {
	// from уже определено ранее
	// TODO: декостылизация
	JID to;
	to.setHostname(name);
	for(VirtualHost::sessions_t::iterator it = onliners.begin(); it != onliners.end(); it++) {
		to.setUsername(it->first);
		for(VirtualHost::reslist_t::iterator jt = it->second.begin(); jt != it->second.end(); jt++) {
			to.setResource(jt->first);
			stanza->setAttribute("to", to.bare());
			jt->second->sendStanza(stanza);
		}
	}
}

void VirtualHost::saveOfflineMessage(Stanza stanza) {
	db.query("INSERT INTO spool (message_to, message_stanza, message_when) VALUES (%s, %d)", db.quote(stanza->asString()).c_str(), 123456789);
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

XMPPStream *VirtualHost::getStreamByJid(JID jid) {
	sessions_t::iterator it = onliners.find(jid.username());
	if(it != onliners.end()) {
		reslist_t::iterator jt = it->second.find(jid.resource());
		if(jt != it->second.end()) {
			return jt->second;
		}
	}
	return 0;
}

/**
* Событие появления нового онлайнера
* @param stream поток
*/
void VirtualHost::onOnline(XMPPStream *stream) {
	reslist_t reslist;
	reslist[stream->jid().resource()] = stream;
	onliners[stream->jid().username()] = reslist;
	//cout << stream->jid().full() << " is online :-)\n";
}

/**
* Событие ухода пользователя в офлайн
*/
void VirtualHost::onOffline(XMPPStream *stream) {
	onliners[stream->jid().bare()].erase(stream->jid().resource());
	if(onliners[stream->jid().username()].empty()) {
		// Если карта ресурсов пуста, то соответствующий элемент вышестоящей карты нужно удалить
		onliners.erase(stream->jid().username());
	}
	//cout << stream->jid().full() << " is offline :-(\n";
}

/**
* Вернуть пароль пользователя по логину
* @param realm домен
* @param login логин пользователя
* @return пароль пользователя или "" если нет такого пользователя
*/
std::string VirtualHost::getUserPassword(const std::string &realm, const std::string &login)
{
	MySQL::result r = db.query("SELECT user_password FROM users WHERE user_login = %s", db.quote(login).c_str());
	string pwd = r.eof() ? string() : r["user_password"];
	r.free();
	cout << "host: " << hostname() << ", login: " << login << ", password: " << pwd << endl;	
	return pwd;
}
