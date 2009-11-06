#ifndef MAWAR_XEP0114LISTENER_H
#define MAWAR_XEP0114LISTENER_H

#include <nanosoft/netdaemon.h>
#include <nanosoft/asyncserver.h>
#include <nanosoft/mutex.h>

class XMPPServer;

/**
* Класс сокета принимающего коннекты от внешних компонентов
*/
class XEP0114Listener: public AsyncServer
{
protected:
	/**
	* Mutex для thread-safe доступа к общим данным
	*/
	nanosoft::Mutex mutex;
	
	/**
	* Ссылка на сервер
	*/
	XMPPServer *server;
	
	/**
	* Обработчик события: подключение компонента
	*
	* thread-safe
	*/
	virtual AsyncObject* onAccept();
	
	/**
	* Сигнал завершения работы
	*
	* Объект должен закрыть файловый дескриптор
	* и освободить все занимаемые ресурсы
	*/
	virtual void onTerminate();
	
public:
	
	/**
	* Конструктор
	*/
	XEP0114Listener(XMPPServer *srv);
	
	/**
	* Деструктор
	*/
	~XEP0114Listener();
};

#endif // MAWAR_XEP0114LISTENER_H
