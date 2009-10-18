#include <virtualhost.h>

/**
* Конструктор
* @param srv ссылка на сервер
* @param aName имя хоста
* @param config конфигурация хоста
*/
VirtualHost::VirtualHost(XMPPServer *srv, const std::string &aName, VirtualHostConfig config): server(srv), name(aName)
{
	
}

/**
* Деструктор
*/
VirtualHost::~VirtualHost()
{
}

/**
* Вернуть имя хоста
*/
const std::string& VirtualHost::hostname()
{
	return name;
}
