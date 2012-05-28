#ifndef MAWAR_XMPPGROUPS_H
#define MAWAR_XMPPGROUPS_H

#include <xmppserver.h>
#include <xmppdomain.h>
#include <configfile.h>
#include <stanza.h>
#include <adhoccommand.h>
#include <jid.h>
#include <nanosoft/mutex.h>
#include <db.h>

/**
* Класс сервера групповых сообщений
*/
class XMPPGroups: public XMPPDomain
{
private:
	/**
	* Конфигурация сервера групповых сообщений
	*/
	ATXmlTag *config;
	
public:
	/**
	* База данных
	*/
	DB db;
	
	/**
	* Mutex для thread-safe доступа к общим данным
	*/
	nanosoft::Mutex mutex;
	
	/**
	* Конструктор сервера групповых сообщений
	* @param srv ссылка на сервер
	* @param aName имя хоста
	* @param config конфигурация хоста
	*/
	XMPPGroups(XMPPServer *srv, const std::string &aName, ATXmlTag *config);
	
	/**
	* Деструктор сервера групповых сообщений
	*/
	~XMPPGroups();
	
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
	virtual bool routeStanza(Stanza stanza);
	
	/**
	* Обработка presence-станзы
	*/
	void handlePresence(Stanza stanza);
	
	/**
	* Обработка message-станзы
	*/
	void handleMessage(Stanza stanza);
	
	/**
	* Обработка IQ-станзы
	*/
	void handleIQ(Stanza stanza);
	
	/**
	* XEP-0030: Service Discovery #info
	*/
	void handleIQServiceDiscoveryInfo(Stanza stanza);
	
	/**
	* Обзор информации о сервере групповых сообщений
	* 
	* XEP-0030: Service Discovery #info
	*/
	void discoverServerInfo(Stanza stanza);
	
	/**
	* Обзор списка команд применимых к серверу групповых сообщений
	* 
	* XEP-0030: Service Discovery #info
	*/
	void discoverServerCommands(Stanza stanza);
	
	/**
	* Обзор информации о гуппе
	* 
	* XEP-0030: Service Discovery #info
	*/
	void discoverGroupInfo(Stanza stanza);
	
	/**
	* Обзор списока команд применимых к группе
	* 
	* XEP-0030: Service Discovery #info
	*/
	void discoverGroupCommands(Stanza stanza);
	
	/**
	* XEP-0030: Service Discovery #items
	*/
	void handleIQServiceDiscoveryItems(Stanza stanza);
	
	/**
	* Обзор групп сервера групповых сообщений
	* 
	* XEP-0030: Service Discovery #items
	*/
	void discoverServerItems(Stanza stanza);
	
	/**
	* Обзор подписчиков групповых сообщений
	* 
	* XEP-0030: Service Discovery #items
	*/
	void discoverGroupItems(Stanza stanza);
	
	/**
	* Обработка Ad-Hoc комманд
	*
	* XEP-0050: Ad-Hoc Commands
	*/
	void handleIQAdHocCommand(AdHocCommand cmd);
	
	/**
	* Обработка Ad-Hoc комманд для сервера
	*/
	void handleServerCommand(AdHocCommand cmd);
	
	/**
	* Обработка Ad-Hoc комманд для группы
	*/
	void handleGroupCommand(AdHocCommand cmd);

	/**
	* Обработка Ad-Hoc комманды 'subscribe'
	* 
	* Подписаться на группу рассылок
	*/
	void handleGroupSubscribe(AdHocCommand cmd);
	
	/**
	* Обработка Ad-Hoc комманды 'unsubscribe'
	* 
	* Отписаться от группы рассылок
	*/
	void handleGroupUnsubscribe(AdHocCommand cmd);
	
	/**
	* Обработка запрещенной IQ-станзы (недостаточно прав)
	*/
	void handleIQForbidden(Stanza stanza);
	
	/**
	* Обработка неизвестной IQ-станзы
	*/
	void handleIQUnknown(Stanza stanza);
};

#endif // MAWAR_XMPPGROUPS_H
