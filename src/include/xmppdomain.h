#ifndef MAWAR_XMPPDOMAIN_H
#define MAWAR_XMPPDOMAIN_H

#include <string>
#include <stanza.h>

class XMPPServer;

/**
* Базовый класс представляющий домен (виртуальный хост, s2s, XEP-0114 и т.п.)
*/
class XMPPDomain
{
protected:
	/**
	* Ссылка на сервер
	*/
	XMPPServer *server;
	
	/**
	* Имя домена
	*/
	std::string name;
	
public:
	/**
	* Конструктор
	* @param srv ссылка на сервер
	* @param aName имя хоста
	*/
	XMPPDomain(XMPPServer *srv, const std::string &aName);
	
	/**
	* Деструктор
	*/
	virtual ~XMPPDomain();
	
	/**
	* Вернуть имя хоста
	*/
	const std::string &hostname() const {
		return name;
	}
	
	XMPPServer* getServer() const {
		return server;
	}
	
	/**
	* Роутер станз (need thread-safe)
	*
	* Данная функция отвечает только за маршрутизацию станз в данном домене
	*
	* @note Данный метод вызывается из глобального маршрутизатора станз XMPPServer::routeStanza()
	*   вызывать его напрямую из других мест не рекомендуется - используйте XMPPServer::routeStanza()
	*
	* @param stanza станза
	* @return TRUE - станза была отправлена, FALSE - станзу отправить не удалось
	*/
	virtual bool routeStanza(Stanza stanza) = 0;
};

#endif // MAWAR_XMPPDOMAIN_H
