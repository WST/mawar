#ifndef MAWAR_SERVERSTATUS_H
#define MAWAR_SERVERSTATUS_H

#include <nanosoft/asyncserver.h>
#include <stdio.h>

/**
* Класс сервера статистики
*
* Очень простой сервер, всем входящим соединениям выдает краткую статистику
*/
class ServerStatus: public AsyncServer
{
protected:
	/**
	* Сигнал завершения работы
	*/
	virtual void onTerminate();
public:
	/**
	* Ссылка на объект сервера
	*/
	class XMPPServer *server;
	
	/**
	* Конструктор сервера статистики
	*/
	ServerStatus(class XMPPServer *srv);
	
	/**
	* Деструктор сервера статистики
	*/
	~ServerStatus();
	
	/**
	* Выдать статистику в указанный файл
	*
	* @param status файл|сокет в который надо записать статистику
	*/
	void handleStatus(FILE *status);
	
	/**
	* Обработчик события: подключение s2s
	*
	* thread-safe
	*/
	void onAccept();
};

#endif // MAWAR_SERVERSTATUS_H
