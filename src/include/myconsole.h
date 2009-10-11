#ifndef MAWAR_MYCONSOLE_H
#define MAWAR_MYCONSOLE_H

#include <nanosoft/netdaemon.h>
#include <nanosoft/asyncstream.h>

/**
* Класс XMPP-поток
*/
class MyConsole: public AsyncStream
{
private:
	NetDaemon *daemon;
public:
	/**
	* Конструктор потока
	*/
	MyConsole(NetDaemon *d, int sock);
	
	/**
	* Деструктор потока
	*/
	~MyConsole();
	
	/**
	* Событие готовности к чтению
	*
	* Вызывается когда в потоке есть данные,
	* которые можно прочитать без блокирования
	*/
	virtual void onRead();
	
	/**
	* Событие готовности к записи
	*
	* Вызывается, когда в поток готов принять
	* данные для записи без блокировки
	*/
	virtual void onWrite();
	
	/**
	* Событие закрытие соединения
	*
	* Вызывается если peer закрыл соединение
	*/
	virtual void onShutdown();
};

#endif // MAWAR_MYCONSOLE_H
