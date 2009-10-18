#ifndef MAWAR_VIRTUALHOST_H
#define MAWAR_VIRTUALHOST_H

#include <xmppserver.h>
#include <configfile.h>
#include <string>

/**
* Класс виртуального узла
*/
class VirtualHost
{
public:
	/**
	* Конструктор
	* @param srv ссылка на сервер
	* @param aName имя хоста
	* @param config конфигурация хоста
	*/
	VirtualHost(XMPPServer *srv, const std::string &aName, VirtualHostConfig config);
	
	/**
	* Деструктор
	*/
	~VirtualHost();
	
	/**
	* Вернуть имя хоста
	*/
	const std::string& hostname();
	
private:
	/**
	* Ссылка на сервер которому принадлежит виртуальный хост
	*/
	XMPPServer *server;
	
	/**
	* Имя виртуального узла
	*/
	std::string name;
};

#endif // MAWAR_VIRTUALHOST_H
