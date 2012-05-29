
#include <xmppgroups.h>
#include <configfile.h>
#include <taghelper.h>
#include <attagparser.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>

using namespace std;
using namespace nanosoft;

/**
* Конструктор сервера групповых сообщений
* @param srv ссылка на сервер
* @param aName имя хоста
* @param config конфигурация хоста
*/
XMPPGroups::XMPPGroups(XMPPServer *srv, const std::string &aName, ATXmlTag *cfg):
	XMPPDomain(srv, aName)
{
	TagHelper storage = cfg->getChild("storage");
	if ( storage->getAttribute("engine", "mysql") != "mysql" )
	{
		fprintf(stderr, "[XMPPGroups] unknown storage engine: %s\n", storage->getAttribute("engine").c_str());
	}
	
	// Подключаемся к БД
	string server = storage["server"];
	if ( server.substr(0, 5) == "unix:" )
	{
		if ( ! db.connectUnix(server.substr(5), storage["database"], storage["username"], storage["password"]) )
		{
			fprintf(stderr, "[XMPPGroups] cannot connect to database\n");
		}
	}
	else
	{
		if ( ! db.connect(server, storage["database"], storage["username"], storage["password"]) )
		{
			fprintf(stderr, "[XMPPGroups] cannot connect to database\n");
		}
	}
	
	// кодировка только UTF-8
	db.query("SET NAMES UTF8");
	
	config = cfg;
}

/**
* Деструктор сервера групповых сообщений
*/
XMPPGroups::~XMPPGroups()
{
}

/**
* Найти подписку по JID
*/
ATXmlTag* XMPPGroups::findSubscribeByJID(const std::string &group, const std::string &jid)
{
	ATXmlTag *item = 0;
	DB::result r = db.query("SELECT * FROM group_subscribers WHERE group_name = %s AND contact_jid = %s LIMIT 1",
		db.quote(group).c_str(), db.quote(jid).c_str()
	);
	if ( r )
	{
		if ( ! r.eof() )
		{
			item = new ATXmlTag("user");
			item->setAttribute("group", r["group_name"]);
			item->setAttribute("jid", r["contact_jid"]);
			item->setAttribute("nickname", r["contact_name"]);
			item->setAttribute("owner", r["contact_owner"] == "1" ? "yes" : "no");
		}
		r.free();
	}
	
	return item;
}

/**
* Найти подписку по Nickname
*/
ATXmlTag* XMPPGroups::findSubscribeByNick(const std::string &group, const std::string &nickname)
{
	ATXmlTag *item = 0;
	DB::result r = db.query("SELECT * FROM group_subscribers WHERE group_name = %s AND contact_name = %s LIMIT 1",
		db.quote(group).c_str(), db.quote(nickname).c_str()
	);
	if ( r )
	{
		if ( ! r.eof() )
		{
			item = new ATXmlTag("user");
			item->setAttribute("group", r["group_name"]);
			item->setAttribute("jid", r["contact_jid"]);
			item->setAttribute("nickname", r["contact_name"]);
			item->setAttribute("owner", r["contact_owner"] == "1" ? "yes" : "no");
		}
		r.free();
	}
	
	return item;
}

/**
* Пописать абонента на группу
*/
void XMPPGroups::subscribeUser(const std::string &group, const std::string &jid, const std::string &nickname)
{
	DB::result r = db.query("INSERT INTO group_subscribers (group_name, contact_name, contact_jid) VALUES (%s, %s, %s)", db.quote(group).c_str(), db.quote(nickname).c_str(), db.quote(jid).c_str());
	if ( r ) r.free();
	
	Stanza presence = new ATXmlTag("presence");
	presence->setAttribute("to", jid);
	presence->setAttribute("from", group);
	presence->setAttribute("type", "subscribe");
	server->routeStanza(presence);
	delete presence;
}

/**
* Отписать абонента от группы
*/
void XMPPGroups::unsubscribeUser(const std::string &group, const std::string &jid)
{
	DB::result r = db.query("DELETE FROM group_subscribers WHERE group_name = %s AND contact_jid = %s", db.quote(group).c_str(), db.quote(jid).c_str());
	if ( r ) r.free();
}

/**
* Роутер входящих станз (thread-safe)
*
* @note Данный метод вызывается из глобального маршрутизатора станз XMPPServer::routeStanza()
*   вызывать его напрямую из других мест не рекомендуется - используйте XMPPServer::routeStanza()
*
* @param to адресат которому надо направить станзу
* @param stanza станза
* @return @deprecated
*/
bool XMPPGroups::routeStanza(Stanza stanza)
{
	if ( stanza->name() == "presence" )
	{
		handlePresence(stanza);
		return true;
	}
	
	if ( stanza->name() == "message" )
	{
		handleMessage(stanza);
		return true;
	}
	
	if ( stanza->name() == "iq" )
	{
		handleIQ(stanza);
		return true;
	}
	
	return false;
}

/**
* Обработка presence-станзы
*/
void XMPPGroups::handlePresence(Stanza stanza)
{
	if ( stanza.to().username().empty() )
	{
		handleServerPresence(stanza);
		return;
	}
	else
	{
		if ( stanza.to().resource().empty() )
		{
			handleGroupPresence(stanza);
			return;
		}
	}
}

/**
* Обработка presence-станзы для сервера
*/
void XMPPGroups::handleServerPresence(Stanza stanza)
{
}

/**
* Обработка presence-станзы для группы
*/
void XMPPGroups::handleGroupPresence(Stanza stanza)
{
	string type = stanza->getAttribute("type");
	
	if ( type == "subscribe" )
	{
		handleGroupPresenceSubscribe(stanza);
		return;
	}
}

/**
* Обработка станзы presence-subscribe для группы
*/
void XMPPGroups::handleGroupPresenceSubscribe(Stanza stanza)
{
	Stanza presence = new ATXmlTag("presence");
	presence->setAttribute("from", stanza.to().bare());
	presence->setAttribute("to", stanza.from().bare());
	presence->setAttribute("type", "subscribed");
	server->routeStanza(presence);
	
	presence->removeAttribute("type");
	server->routeStanza(presence);
	delete presence;
}

/**
* Обработка message-станзы
*/
void XMPPGroups::handleMessage(Stanza stanza)
{
}

/**
* Обработка IQ-станзы
*/
void XMPPGroups::handleIQ(Stanza stanza)
{
	Stanza body = stanza->firstChild();
	std::string xmlns = body ? body->getAttribute("xmlns", "") : "";
	
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
		handleIQAdHocCommand(stanza);
		return;
	}
	
	handleIQUnknown(stanza);
}

/**
* XEP-0030: Service Discovery #info
*/
void XMPPGroups::handleIQServiceDiscoveryInfo(Stanza stanza)
{
	if ( stanza->getAttribute("type") == "get" )
	{
		string node = stanza["query"]->getAttribute("node");
		
		if( node  == "" )
		{
			if ( stanza.to().username().empty() )
			{
				discoverServerInfo(stanza);
				return;
			}
			else
			{
				discoverGroupInfo(stanza);
				return;
			}
		}
	}
	
	handleIQUnknown(stanza);
}

/**
* Обзор информации о сервере групповых сообщений
* 
* XEP-0030: Service Discovery #info
*/
void XMPPGroups::discoverServerInfo(Stanza stanza)
{
	Stanza iq = new ATXmlTag("iq");
	iq->setAttribute("from", stanza.to().full());
	iq->setAttribute("to", stanza.from().full());
	iq->setAttribute("id", stanza->getAttribute("id"));
	iq->setAttribute("type", "result");
	TagHelper query = iq["query"];
	query->setDefaultNameSpaceAttribute("http://jabber.org/protocol/disco#info");
	
	ATXmlTag *identity = new ATXmlTag("identity");
	identity->setAttribute("category", "conference");
	identity->setAttribute("type", "text");
	identity->setAttribute("name", "Message broadcast groups");
	query->insertChildElement(identity);
	
	ATXmlTag *feature;

	feature = new ATXmlTag("feature");
	feature->setAttribute("var", "http://jabber.org/protocol/disco#info");
	query->insertChildElement(feature);

	feature = new ATXmlTag("feature");
	feature->setAttribute("var", "http://jabber.org/protocol/disco#items");
	query->insertChildElement(feature);
	
	// XEP-0050 ad-hoc commands
	feature = new ATXmlTag("feature");
	feature->setAttribute("var", "http://jabber.org/protocol/commands");
	query->insertChildElement(feature);
	
	server->routeStanza(iq);
	delete iq;
	return;
}

/**
* Обзор списка команд применимых к серверу групповых сообщений
* 
* XEP-0030: Service Discovery #info
*/
void XMPPGroups::discoverServerCommands(Stanza stanza)
{
	handleIQUnknown(stanza);
}

/**
* Обзор информации о гуппе
* 
* XEP-0030: Service Discovery #info
*/
void XMPPGroups::discoverGroupInfo(Stanza stanza)
{
	DB::result r = db.query(
		"SELECT * FROM xmpp_groups WHERE group_name=%s",
		db.quote(stanza.to().username()).c_str()
	);
	if ( r )
	{
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("from", stanza.to().full());
		iq->setAttribute("to", stanza.from().full());
		iq->setAttribute("id", stanza->getAttribute("id"));
		iq->setAttribute("type", "result");
		TagHelper query = iq["query"];
		query->setDefaultNameSpaceAttribute("http://jabber.org/protocol/disco#info");
		
		ATXmlTag *identity = new ATXmlTag("identity");
		identity->setAttribute("category", "conference");
		identity->setAttribute("type", "text");
		identity->setAttribute("name", r["group_title"]);
		query->insertChildElement(identity);
		
		ATXmlTag *feature;
	
		feature = new ATXmlTag("feature");
		feature->setAttribute("var", "http://jabber.org/protocol/disco#info");
		query->insertChildElement(feature);
	
		feature = new ATXmlTag("feature");
		feature->setAttribute("var", "http://jabber.org/protocol/disco#items");
		query->insertChildElement(feature);
		
		// XEP-0050 ad-hoc commands
		feature = new ATXmlTag("feature");
		feature->setAttribute("var", "http://jabber.org/protocol/commands");
		query->insertChildElement(feature);
		
		server->routeStanza(iq);
		delete iq;
		r.free();
		return;
	}
	
	Stanza result = new ATXmlTag("iq");
	result->setAttribute("from", stanza.to().full());
	result->setAttribute("to", stanza.from().full());
	result->setAttribute("type", "error");
	result->setAttribute("id", stanza->getAttribute("id"));
	Stanza error = result["error"];
	error->setAttribute("type", "cancel");
	error["service-unavailable"]->setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas");
	server->routeStanza(result);
	delete result;
}

/**
* Обзор списока команд применимых к группе
* 
* XEP-0030: Service Discovery #info
*/
void XMPPGroups::discoverGroupCommands(Stanza stanza)
{
	printf("XMPPGroups[%s]::discoverGroupCommands()\n", hostname().c_str());
	// Нода команд ad-hoc
	Stanza iq = new ATXmlTag("iq");
	iq->setAttribute("from", stanza.to().full());
	iq->setAttribute("to", stanza.from().full());
	iq->setAttribute("type", "result");
	iq->setAttribute("id", stanza->getAttribute("id"));
	TagHelper query = iq["query"];
	query->setDefaultNameSpaceAttribute("http://jabber.org/protocol/disco#items");
	query->setAttribute("node", "http://jabber.org/protocol/commands");
	
	ATXmlTag *item;
	
	item = new ATXmlTag("item");
		item->setAttribute("jid", stanza.to().bare());
		item->setAttribute("node", "subscribe");
		item->setAttribute("name", "Subscribe to the group");
		query->insertChildElement(item);
	
	item = new ATXmlTag("item");
		item->setAttribute("jid", stanza.to().bare());
		item->setAttribute("node", "unsubscribe");
		item->setAttribute("name", "Unsubscribe from the group");
		query->insertChildElement(item);
	
	server->routeStanza(iq);
	delete iq;
	return;
	handleIQUnknown(stanza);
}

/**
* XEP-0030: Service Discovery #items
*/
void XMPPGroups::handleIQServiceDiscoveryItems(Stanza stanza)
{
	if( stanza->getAttribute("type") == "get" )
	{
		string node = stanza["query"]->getAttribute("node");
		
		if ( node == "" )
		{
			if ( stanza.to().username().empty() )
			{
				discoverServerItems(stanza);
				return;
			}
			else
			{
				discoverGroupItems(stanza);
				return;
			}
		}
		
		if ( node == "http://jabber.org/protocol/commands" )
		{
			if ( stanza.to().username().empty() )
			{
				discoverServerCommands(stanza);
				return;
			}
			else
			{
				discoverGroupCommands(stanza);
				return;
			}
		}
	}
	
	handleIQUnknown(stanza);
}

/**
* Обзор групп сервера групповых сообщений
* 
* XEP-0030: Service Discovery #items
*/
void XMPPGroups::discoverServerItems(Stanza stanza)
{
	Stanza iq = new ATXmlTag("iq");
	iq->setAttribute("from", stanza.to().full());
	iq->setAttribute("to", stanza.from().full());
	iq->setAttribute("id", stanza->getAttribute("id"));
	iq->setAttribute("type", "result");
	TagHelper query = iq["query"];
	query->setDefaultNameSpaceAttribute("http://jabber.org/protocol/disco#items");
	
	DB::result r = db.query("SELECT * FROM xmpp_groups ORDER BY group_title ASC");
	if ( r )
	{
		ATXmlTag *item;
		for(; ! r.eof(); r.next())
		{
			item = new ATXmlTag("item");
			item->setAttribute("jid", r["group_name"] + "@" + hostname());
			item->setAttribute("name", r["group_title"]);
			query->insertChildElement(item);
		}
		r.free();
	}
	
	server->routeStanza(iq);
	delete iq;
}

/**
* Обзор подписчиков групповых сообщений
* 
* XEP-0030: Service Discovery #items
*/
void XMPPGroups::discoverGroupItems(Stanza stanza)
{
	DB::result r = db.query(
		"SELECT * FROM xmpp_groups "
		"WHERE group_name=%s",
		db.quote(stanza.to().username()).c_str()
	);
	if ( r )
	{
		r.free();
		
		Stanza iq = new ATXmlTag("iq");
		iq->setAttribute("from", stanza.to().full());
		iq->setAttribute("to", stanza.from().full());
		iq->setAttribute("id", stanza->getAttribute("id"));
		iq->setAttribute("type", "result");
		TagHelper query = iq["query"];
		query->setDefaultNameSpaceAttribute("http://jabber.org/protocol/disco#items");
		
		// TODO show subscribers
		
		server->routeStanza(iq);
		delete iq;
		return;
	}
	
	Stanza result = new ATXmlTag("iq");
	result->setAttribute("from", stanza.to().full());
	result->setAttribute("to", stanza.from().full());
	result->setAttribute("id", stanza->getAttribute("id"));
	result->setAttribute("type", "error");
	Stanza error = result["error"];
	error->setAttribute("type", "cancel");
	error["service-unavailable"]->setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas");
	server->routeStanza(result);
	delete result;
}

/**
* Обработка Ad-Hoc комманд
*
* XEP-0050: Ad-Hoc Commands
*/
void XMPPGroups::handleIQAdHocCommand(AdHocCommand cmd)
{
	if ( cmd.to().username().empty() )
	{
		handleServerCommand(cmd);
		return;
	}
	else
	{
		handleGroupCommand(cmd);
		return;
	}
	
	handleIQUnknown(cmd);
}

/**
* Обработка Ad-Hoc комманд для сервера
*/
void XMPPGroups::handleServerCommand(AdHocCommand cmd)
{
	printf("handle server[%s] command: %s\n", cmd.to().hostname().c_str(), cmd->asString().c_str());
	handleIQUnknown(cmd);
}

/**
* Обработка Ad-Hoc комманд для группы
*/
void XMPPGroups::handleGroupCommand(AdHocCommand cmd)
{
	string command = cmd["command"]->getAttribute("node");
	
	if ( command == "subscribe" )
	{
		handleGroupSubscribe(cmd);
		return;
	}
	
	if ( command == "unsubscribe" )
	{
		handleGroupUnsubscribe(cmd);
	}
	
	handleIQUnknown(cmd);
}

/**
* Обработка Ad-Hoc комманды 'subscribe'
* 
* Подписаться на группу рассылок
*/
void XMPPGroups::handleGroupSubscribe(AdHocCommand cmd)
{
	string user = cmd.from().bare();
	string group = cmd.to().bare();
	printf("subscribe user[%s] to group[%s]\n", user.c_str(), group.c_str());
	
	AdHocCommand reply = AdHocCommand::reply(cmd);
	
	if ( cmd.isCancel() )
	{
		reply.setStatus("canceled");
		server->routeStanza(reply);
		delete reply;
		return;
	}
	
	TagHelper s_user = findSubscribeByJID(group, user);
	if ( s_user )
	{
		string note = "You are already subcribed to " + group + " with nickname " + s_user->getAttribute("contact_name");
		reply.setStatus("completed");
		reply.setNote(note);
		reply.setTitle("Subscribe to the group [" + group + "]");
		reply.setInstructions(note);
		server->routeStanza(reply);
		delete reply;
		delete s_user;
		return;
	}
	
	if ( cmd.isSubmit() )
	{
		string nick = cmd.getFieldValue("username");
		if ( nick.empty() )
		{
			reply.setStatus("executing");
			reply.setButtonEnable("complete");
			reply.setType("form");
			reply.setTitle("Subscribe to the group [" + group + "]");
			reply.setInstructions("Do you want to subscribe to the group \"" + group + "\"?");
			reply.setField("error", "fixed", "Nickname cannot be empty");
			reply.setField("username", "text-single", "Choose username");
			server->routeStanza(reply);
			delete reply;
			return;
		}
		
		TagHelper s_nick = findSubscribeByNick(group, nick);
		if ( s_nick )
		{
			reply.setStatus("executing");
			reply.setButtonEnable("complete");
			reply.setType("form");
			reply.setTitle("Subscribe to the group [" + group + "]");
			reply.setInstructions("Do you want to subscribe to the group \"" + group + "\"?");
			reply.setField("error", "fixed", ("Nickname " + nick + " already busy, choose other...").c_str());
			reply.setField("username", "text-single", "Choose username");
			server->routeStanza(reply);
			delete reply;
			delete s_nick;
			return;
		}
		printf("subscribe user[%s] to group[%s] with resource[%s]\n", user.c_str(), group.c_str(), cmd.getFieldValue("username").c_str());
		
		subscribeUser(group, user, nick);
		
		reply.setStatus("completed");
		reply.setNote("You are subcribed to " + group);
		reply.setTitle("Subscribe to the group [" + group + "]");
		reply.setInstructions("You are subscribed to the group \"" + group + "\"!");
		server->routeStanza(reply);
		delete reply;
		return;
	}
	
	reply.setStatus("executing");
	reply.setButtonEnable("complete");
	reply.setType("form");
	reply.setTitle("Subscribe to the group [" + group + "]");
	reply.setInstructions("Do you want to subscribe to the group \"" + group + "\"?");
	reply.setField("username", "text-single", "Choose username");
	
	server->routeStanza(reply);
	delete reply;
}

/**
* Обработка Ad-Hoc комманды 'unsubscribe'
* 
* Отписаться от группы рассылок
*/
void XMPPGroups::handleGroupUnsubscribe(AdHocCommand cmd)
{
	string user = cmd.from().bare();
	string group = cmd.to().bare();
	printf("unsubscribe user[%s] to group[%s]\n", user.c_str(), group.c_str());
	
	AdHocCommand reply = AdHocCommand::reply(cmd);
	
	if ( cmd.isCancel() )
	{
		reply.setStatus("canceled");
		server->routeStanza(reply);
		delete reply;
		return;
	}
	
	TagHelper s_user = findSubscribeByJID(group, user);
	if ( ! s_user )
	{
		string note = "You are not subcribed to " + group;
		reply.setStatus("completed");
		reply.setNote(note);
		reply.setTitle("Subscribe to the group [" + group + "]");
		reply.setInstructions(note);
		server->routeStanza(reply);
		delete reply;
		return;
	}
	delete s_user;
	
	if ( cmd.isSubmit() )
	{
		unsubscribeUser(group, user);
		
		reply.setStatus("completed");
		reply.setNote("You are unsubcribed from " + group);
		reply.setTitle("Subscribe to the group [" + group + "]");
		reply.setInstructions("You are unsubscribed from the group \"" + group + "\"!");
		server->routeStanza(reply);
		delete reply;
		return;
	}
	
	reply.setStatus("executing");
	reply.setButtonEnable("complete");
	reply.setType("form");
	reply.setTitle("Subscribe to the group [" + group + "]");
	reply.setInstructions("Do you want to unsubscribe from the group \"" + group + "\"?");
	
	server->routeStanza(reply);
	delete reply;
}

/**
* Обработка запрещенной IQ-станзы (недостаточно прав)
*/
void XMPPGroups::handleIQForbidden(Stanza stanza)
{
	if ( stanza->getAttribute("type", "") == "error" ) return;
	if ( stanza->getAttribute("type", "") == "result" ) return;
	
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
void XMPPGroups::handleIQUnknown(Stanza stanza)
{
	if ( stanza->getAttribute("type", "") == "error" ) return;
	if ( stanza->getAttribute("type", "") == "result" ) return;
	
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
