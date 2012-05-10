
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
	handleIQUnknown(stanza);
}

/**
* XEP-0030: Service Discovery #items
*/
void XMPPGroups::handleIQServiceDiscoveryItems(Stanza stanza)
{
	if( stanza->getAttribute("type") == "get" )
	{
		if ( stanza["query"]->getAttribute("node", "") == "" )
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