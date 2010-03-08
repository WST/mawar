#ifndef MAWAR_S2SLISTENER_H
#define MAWAR_S2SLISTENER_H

#include <nanosoft/asyncserver.h>

/**
* Класс сокета слушающего S2S
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
	AsyncObject* onAccept();
	
	/**
	* Сигнал завершения
	*/
	void onSigTerm();
	
	/**
	* Сигнал HUP
	*/
	void onSigHup();
};

#endif // MAWAR_S2SLISTENER_H
