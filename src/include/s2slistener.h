#ifndef MAWAR_S2SLISTENER_H
#define MAWAR_S2SLISTENER_H

#include <nanosoft/asyncserver.h>
#include <nanosoft/mutex.h>
#include <nanosoft/asyncdns.h>
#include <stanza.h>

/**
* Класс сокета слушающего S2S
*
* И по совместительсвую ИО менеджера s2s :-D
*/
class S2SListener: public AsyncServer
{
protected:
	/**
	* Сигнал завершения работы
	*
	* Объект должен закрыть файловый дескриптор
	* и освободить все занимаемые ресурсы
	*/
	virtual void onTerminate();
	
public:
	/**
	* Ссылка на объект сервера
	*/
	class XMPPServer *server;
	
	/**
	* Конструктор сервера
	*/
	S2SListener(class XMPPServer *srv);
	
	/**
	* Деструктор сервера
	*/
	~S2SListener();
	
	/**
	* Обработчик события: подключение s2s
	*
	* thread-safe
	*/
	void onAccept();
};

#endif // MAWAR_S2SLISTENER_H
