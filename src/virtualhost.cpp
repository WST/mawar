
#include <virtualhost.h>
#include <configfile.h>
#include <taghelper.h>
#include <attagparser.h>
#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
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
VirtualHost::VirtualHost(XMPPServer *srv, const std::string &aName, ATXmlTag *cfg):
	XMPPDomain(srv, aName),
	bind_conflict(bind_override),
	tls_required(false)
{
	TagHelper registration = cfg->getChild("registration");
	registration_allowed = registration->getAttribute("enabled", "no") == "yes";
	
	TagHelper onBindConflict = cfg->getChild("on-bind-conflict");
	if ( onBindConflict )
	{
		string action = onBindConflict->getCharacterData();
		if ( action == "override-resource" ) bind_conflict = bind_override;
		else if ( action == "reject-new" ) bind_conflict = bind_reject_new;
		else if ( action == "remove-old" ) bind_conflict = bind_remove_old;
		switch ( bind_conflict )
		{
			case bind_override:
				printf("on-bind-conflict: bind_override\n");
				break;
			case bind_reject_new:
				printf("on-bind-conflict: bind_reject_new\n");
				break;
			case bind_remove_old:
				printf("on-bind-conflict: bind_remove_old\n");
				break;
		}
	}
	
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
	
#ifdef HAVE_GNUTLS
	initTLS();
#endif // HAVE_GNUTLS
}

/**
* Деструктор
*/
VirtualHost::~VirtualHost()
{
#ifdef HAVE_GNUTLS
	gnutls_certificate_free_credentials (tls_ctx.x509_cred);
	gnutls_priority_deinit (tls_ctx.priority_cache);
#endif // HAVE_GNUTLS
}

/**
* Проверить поддерживает ли виртуальный хост TLS
*/
bool VirtualHost::canTLS()
{
#ifdef HAVE_GNUTLS
	return ssl;
#else
	return false;
#endif // HAVE_GNUTLS
}

#ifdef HAVE_GNUTLS
/**
* Инциализация TLS
*/
void VirtualHost::initTLS()
{
	ssl = false;
	TagHelper cfg = config->firstChild("tls");
	if ( cfg )
	{
		printf("vhost[%s]: TLS config present\n", hostname().c_str());
		string ca = cfg["ca-certificate"]->getCharacterData();
		string cert = cfg["certificate"]->getCharacterData();
		string key = cfg["private-key"]->getCharacterData();
		string crl = cfg["crl"]->getCharacterData();
		string priority = cfg["priority"]->getCharacterData();
		tls_required = cfg->firstChild("required") != 0;
		printf("vhost[%s] ca-certificate: %s\n", hostname().c_str(), ca.c_str());
		printf("vhost[%s] certificate: %s\n", hostname().c_str(), cert.c_str());
		printf("vhost[%s] private-key: %s\n", hostname().c_str(), key.c_str());
		printf("vhost[%s] crl: %s\n", hostname().c_str(), crl.c_str());
		printf("vhost[%s] tls required: %s\n", hostname().c_str(), (tls_required ? "yes" : "no"));
		
		int ret;
		gnutls_certificate_allocate_credentials (&tls_ctx.x509_cred);
		ret = gnutls_certificate_set_x509_trust_file(tls_ctx.x509_cred, ca.c_str(), GNUTLS_X509_FMT_PEM);
		if ( ret < 0 )
		{
			printf("vhost[%s]: \033[22;31mSSL gnutls_certificate_set_x509_trust_file fault\033[0m\n", hostname().c_str());
			return;
		}
		
		/*
		ret = gnutls_certificate_set_x509_crl_file(tls_ctx.x509_cred, crl.c_str(), GNUTLS_X509_FMT_PEM);
		if ( ret < 0 )
		{
			printf("vhost[%s]: \033[22;31mSSL gnutls_certificate_set_x509_crl_file fault\033[0m\n", hostname().c_str());
			return;
		}
		*/
		
		ret = gnutls_certificate_set_x509_key_file (tls_ctx.x509_cred, cert.c_str(), key.c_str(), GNUTLS_X509_FMT_PEM);
		if ( ret < 0 )
		{
			printf("vhost[%s]: \033[22;31mSSL gnutls_certificate_set_x509_key_file fault\033[0m\n", hostname().c_str());
			return;
		}
		
		static const char *default_priority = "NORMAL:+COMP-ALL%SERVER_PRECEDENCE";
		if ( priority.empty() )
		{
			ret = gnutls_priority_init(&tls_ctx.priority_cache, default_priority, NULL);
			if ( ret < 0 )
			{
				printf("gnutls_priority_init(%s) fault\n", default_priority);
			}
		}
		else
		{
			ret = gnutls_priority_init(&tls_ctx.priority_cache, priority.c_str(), NULL);
			if ( ret < 0 )
			{
				printf("gnutls_priority_init(%s) fault, fallback to default: %s\n", priority.c_str(), default_priority);
				ret = gnutls_priority_init(&tls_ctx.priority_cache, default_priority, NULL);
				if ( ret < 0 )
				{
					printf("gnutls_priority_init(%s) fault\n", default_priority);
				}
			}
		}
		
		ssl = true;
		printf("vhost[%s] TLS init ok\n", hostname().c_str());
	}
	else
	{
		printf("vhost[%s]: \033[22;31mno ssl config\033[0m\n", hostname().c_str());
	}
}
#endif // HAVE_GNUTLS

/**
* Информационные запросы без атрибута to
* Адресованные клиентом данному узлу
* 
* TODO требуется привести в порядок маршрутизацию
*/
void VirtualHost::handleVHostIq(Stanza stanza)
{
	// если станза адресуется к серверу, то обработать должен сервер
	if ( stanza.to().resource() == "" )
	{
		handleDirectlyIQ(stanza);
		return;
	}
	else
	{
		XMPPClient *client = getClientByJid(stanza.to());
		if ( client )
		{
			client->sendStanza(stanza);
			return;
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

/**
* Проверить есть зарегистрированный клиентс указанными ником
*/
bool VirtualHost::userExists(const std::string &username)
{
	bool retval;
	DB::result r = db.query("SELECT count(*) AS cnt FROM users WHERE user_login=%s", db.quote(username).c_str());
	retval = atoi(r["cnt"].c_str()) == 1;
	r.free();
	return retval;
}

/**
* Добавить пользователя
*/
bool VirtualHost::addUser(const std::string &username, const std::string &password)
{
	DB::result r = db.query("INSERT INTO users (user_login, user_password) VALUES (%s, %s)", db.quote(username).c_str(), db.quote(password).c_str());
	if ( r ) r.free();
	return true;
}

/**
* Удалить пользователя
*/
bool VirtualHost::removeUser(const std::string &username)
{
	DB::result r = db.query("DELETE FROM users WHERE user_login = %s", db.quote(username).c_str());
	if ( r ) r.free();
	// TODO: удалять мусор из других таблиц
	return true;
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
			Stanza error = Stanza::iqError(stanza, "resource-constraint", "cancel");
			server->routeStanza(error);
			delete error;
			r.free();
			return;
		}
		db.query("INSERT INTO spool (message_to, message_stanza, message_when) VALUES (%s, %s, %d)", db.quote(stanza.to().bare()).c_str(), db.quote(data).c_str(), time(NULL));
	} else {
		Stanza error = Stanza::iqError(stanza, "resource-constraint", "cancel");
		server->routeStanza(error);
		delete error;
	}
	r.free();
}

/**
* Обработка message-станзц
*/
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
	
	if ( xmlns == "urn:ietf:params:xml:ns:xmpp-session" )
	{
		handleIQSession(stanza);
		return;
	}
	
	if ( xmlns == "jabber:iq:register" )
	{
		handleIQRegister(stanza);
		return;
	}

	if ( xmlns == "urn:xmpp:ping" )
	{
		handleIQPing(stanza);
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
* RFC 6120, 7. Resource Binding
*/
void VirtualHost::handleIQBind(Stanza stanza, XMPPClient *client)
{
	if ( client )
	{
		if ( stanza->getAttribute("type") == "set" )
		{
			Stanza resource = stanza["bind"]->firstChild("resource");
			if ( resource )
			{
				// RFC 6120, 7.7. Client-Submitted Resource Identifier
				client->client_jid.setResource(resource);
			}
			else
			{
				// RFC 6120, 7.6. Server-Generated Resource Identifier
				client->client_jid.setResource(genResource(client->client_jid.username().c_str()));
			}
			
			if ( bindResource(client->client_jid.resource().c_str(), client) )
			{
				Stanza iq = new ATXmlTag("iq");
					iq->setAttribute("type", "result");
					iq->setAttribute("id", stanza->getAttribute("id"));
					TagHelper bind = iq["bind"];
						bind->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-bind");
						bind["jid"] = client->client_jid.full();
				client->sendStanza(iq);
				delete iq;
			}
			else
			{
				Stanza iq = new ATXmlTag("iq");
					iq->setAttribute("type", "error");
					iq->setAttribute("id", stanza->getAttribute("id"));
					TagHelper error = iq["error"];
						error->setAttribute("type", "wait");
						error["resource-constraint"]->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-stanzas");
				client->sendStanza(iq);
				delete iq;
			}
			
			return;
		}
	}
	
	handleIQUnknown(stanza);
}

/**
* RFC 6121, Appendix E. Differences From RFC 3921
* 
* The protocol for session establishment was determined to be
* unnecessary and therefore the content previously defined in
* Section 3 of RFC 3921 was removed. However, for the sake of
* backward-compatibility server implementations are encouraged
* to advertise support for the feature, even though session
* establishment is a "no-op". 
*/
void VirtualHost::handleIQSession(Stanza stanza)
{
	if( stanza->getAttribute("type") == "set" )
	{
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("to", stanza.from().full());
		iq->setAttribute("from", hostname());
		iq->setAttribute("id", stanza->getAttribute("id"));
		iq->setAttribute("type", "result");
		iq["session"]->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-session");
		server->routeStanza(iq);
		delete iq;
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
			
				if( registration_allowed || isAdmin(stanza.from().bare()) )
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
			stat->setAttribute("name", "queries/stanzas");
			stat->setAttribute("value", mawarPrintInteger(XMPPStream::getStanzaCount()));
			stat->setAttribute("units", "queries");
			query->insertChildElement(stat);
		
		stat = new ATXmlTag("stat");
			stat->setAttribute("name", "queries/max-tags-per-stanza");
			stat->setAttribute("value", mawarPrintInteger(XMPPStream::getMaxTagsPerStanza()));
			stat->setAttribute("units", "queries");
			query->insertChildElement(stat);
		
		stat = new ATXmlTag("stat");
			stat->setAttribute("name", "queries/tags-leak");
			stat->setAttribute("value", mawarPrintInteger(XMPPStream::getTagsLeak()));
			stat->setAttribute("units", "queries");
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
	if(stanza->getAttribute("type") == "get") {
		TagHelper block = stanza["query"]->firstChild(); // запрашиваемый блок
		if(block) {
			ATXmlTag *data = 0;
			
			DB::result r = db.query("SELECT block_data FROM private_storage WHERE username = %s AND block_xmlns = %s", db.quote(stanza.from().username()).c_str(), db.quote(block->getAttribute("xmlns")).c_str());
			if(r) {
				if(!r.eof()) {
					data = parse_xml_string("<?xml version=\"1.0\" ?>\n" + r["block_data"]);
				}
				r.free();
			}
			
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("from", hostname());
			iq->setAttribute("to", stanza.from().full());
			iq->setAttribute("type", "result");
			iq->setAttribute("id", stanza->getAttribute("id"));
			TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:private");
			if(data) {
				iq["query"]->insertChildElement(data);
			} else {
				iq["query"]->insertChildElement(block->clone());
			}
			server->routeStanza(iq);
			delete iq;
			return;
		}
		
		Stanza error = Stanza::iqError(stanza, "bad-request", "modify");
		error->setAttribute("from", hostname());
		server->routeStanza(error);
		delete error;
		return;
	}
	
	if ( stanza->getAttribute("type") == "set" )
	{
		TagHelper block = stanza["query"]->firstChild();
		if ( block )
		{
			db.query("REPLACE INTO private_storage (username, block_xmlns, block_data) VALUES (%s, %s, %s)", db.quote(stanza.from().username()).c_str(), db.quote(block->getAttribute("xmlns")).c_str(), db.quote(block->asString()).c_str());
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("from", hostname());
			iq->setAttribute("to", stanza.from().full());
			iq->setAttribute("type", "result");
			iq->setAttribute("id", stanza->getAttribute("id"));
			TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:private");
			server->routeStanza(iq);
			delete iq;
			return;
		}
		
		Stanza error = Stanza::iqError(stanza, "bad-request", "modify");
		error->setAttribute("from", hostname());
		server->routeStanza(error);
		delete error;
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
				Stanza error = Stanza::iqError(stanza, "resource-constraint", "cancel");
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
* XEP-0077: In-Band Registration
*/
void VirtualHost::handleIQRegister(Stanza stanza)
{
	printf("vhost[%s] handleRegisterIq: %s\n", hostname().c_str(), stanza->asString().c_str());
	
	if ( ! registration_allowed && ! isAdmin(stanza.from().bare()) )
	{
		Stanza error = Stanza::iqError(stanza, "forbidden", "cancel");
		error->setAttribute("from", hostname());
		server->routeStanza(error);
		delete error;
		return;
	}
	
	if ( stanza->getAttribute("type") == "get" )
	{
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("to", stanza.from().full());
		iq->setAttribute("from", hostname());
		iq->setAttribute("type", "result");
		iq->setAttribute("id", stanza->getAttribute("id"));
		TagHelper query = iq["query"];
			query->setDefaultNameSpaceAttribute("jabber:iq:register");
			query["instructions"] = "Choose a username and password for use with this service";
			query["username"];
			query["password"];
		server->routeStanza(iq);
		delete iq;
		return;
	}
	
	if ( stanza->getAttribute("type") == "set" )
	{
		ATXmlTag *remove = stanza->find("query/remove");
		if( remove )
		{
			JID from = stanza.from();
			if ( from.hostname() != hostname() )
			{
				handleIQForbidden(stanza);
				return;
			}
			
			if ( removeUser(from.username()) )
			{
				Stanza iq = new ATXmlTag("iq");
				iq->setAttribute("to", stanza.from().full());
				iq->setAttribute("from", hostname());
				iq->setAttribute("type", "result");
				iq->setAttribute("id", stanza->getAttribute("id"));
				server->routeStanza(iq);
				delete iq;
				return;
			}
			else
			{
				Stanza error = Stanza::iqError(stanza, "service-unavailable", "wait");
				error->setAttribute("from", hostname());
				server->routeStanza(error);
				delete error;
				return;
			}
		}
		
		string username = stanza["query"]["username"]->getCharacterData();
		string password = stanza["query"]["password"]->getCharacterData();
		
		if ( ! verifyUsername(username) || password.empty() )
		{
			Stanza error = Stanza::iqError(stanza, "forbidden", "cancel");
			server->routeStanza(error);
			delete error;
			return;
		}
		
		if ( userExists(username) )
		{
			Stanza error = Stanza::iqError(stanza, "conflict", "cancel");
			server->routeStanza(error);
			delete error;
			return;
		}
		
		if ( addUser(username, password) )
		{
			Stanza iq = new ATXmlTag("iq");
			iq->setAttribute("type", "result");
			iq->setAttribute("from", hostname());
			iq->setAttribute("to", stanza.from().full());
			iq->setAttribute("id", stanza->getAttribute("id"));
			server->routeStanza(iq);
			delete iq;
			return;
		}
		else
		{
			Stanza error = Stanza::iqError(stanza, "service-unavailable", "wait");
			server->routeStanza(error);
			delete error;
			return;
		}
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
XMPPClient *VirtualHost::getClientByJid(const JID &jid)
{
	XMPPClient *client;
	
	mutex.lock();
		client = getClientByJid0(jid);
	mutex.unlock();
	
	return client;
}

/**
* Найти клиента по JID (thread-unsafe)
*
* @note возможно в нем отпадет необходимость по завершении routeStanza()
*/
XMPPClient *VirtualHost::getClientByJid0(const JID &jid)
{
	sessions_t::iterator it = onliners.find(jid.username());
	if ( it != onliners.end() )
	{
		reslist_t::iterator jt = it->second.find(jid.resource());
		if ( jt != it->second.end() )
		{
			return jt->second;
		}
	}
	return 0;
}

/**
* Генерировать случайный ресурс для абонента
*/
std::string VirtualHost::genResource(const char *username)
{
	JID jid;
	jid.setHostname(hostname());
	jid.setUsername(username);
	
	do
	{
		char buf[80];
		sprintf(buf, "%4X-%4X-%4X-%4X", random() % 0x10000, random() % 0x10000, random() % 0x10000, random() % 0x10000);
		jid.setResource(buf);
	}
	while ( getClientByJid(jid) );
	
	return jid.resource();
}

/**
* Привязать ресурс
*/
bool VirtualHost::bindResource(const char *resource, XMPPClient *client)
{
	mutex.lock();
	
	string new_resource = resource;
	XMPPClient *old = getClientByJid0(client->client_jid);
	if ( old )
	{
		if ( bind_conflict == bind_override )
		{
			new_resource = genResource(client->client_jid.username().c_str());
		}
		else if ( bind_conflict == bind_reject_new )
		{
			// отказать в новом соединении
			mutex.unlock();
			return false;
		}
		else if ( bind_conflict == bind_remove_old )
		{
			// выкинуть старого клиента
			Stanza presence = new ATXmlTag("presence");
			presence->setAttribute("type", "unavailable");
			presence["status"] = "Replaced by new connection";
			old->handleUnavailablePresence(presence);
			old->terminate();
			delete presence;
		}
	}
	
	onliners_number++;
	client->client_jid.setResource(new_resource);
	onliners[client->jid().username()][new_resource] = client;
	client->connected = true;
	
	mutex.unlock();
	return true;
}

/**
* Отвязать ресурс
*/
void VirtualHost::unbindResource(const char *resource, XMPPClient *client)
{
	printf("vhost[%s] unbind(%s, %s, %d)\n", hostname().c_str(), resource, client->client_jid.full().c_str(), client->getFd());
	mutex.lock();
	
	sessions_t::iterator user = onliners.find(client->client_jid.username());
	if ( user != onliners.end() )
	{
		reslist_t::iterator res = user->second.find(resource);
		if ( res != user->second.end() )
		{
			if ( res->second == client )
			{
				printf("vhost[%s] remove resource(%s, %s)\n", hostname().c_str(), resource, client->client_jid.full().c_str());
				user->second.erase(res);
			}
			else
			{
				printf("vhost[%s] resource (%s, %d)\n", hostname().c_str(), res->second->client_jid.full().c_str(), res->second->getFd());
			}
		}
	}
	
	mutex.unlock();
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
	// NB код марштрутизации должен быть thread-safe, getClientByJid сейчас thread-safe
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

bool VirtualHost::isAdmin(std::string barejid) {
	if(!config->hasChild("admins")) {
		return false;
	}
	return (bool) config->getChild("admins")->getChildByAttribute("admin", "jid", barejid);
}
