#ifndef MAWAR_DCBOT_H
#define MAWAR_DCBOT_H

#include <nanosoft/asyncstream.h>

/**
* Класс XMPP-поток
*/
class DCBot: public AsyncStream
{
protected:
	/**
	* Обработчик прочитанных данных
	*/
	virtual void onRead(const char *data, size_t len);
	
	/**
	* Пир (peer) закрыл поток.
	*
	* Мы уже ничего не можем отправить в ответ,
	* можем только корректно закрыть соединение с нашей стороны.
	*/
	virtual void onPeerDown();
	
	/**
	* Сигнал завершения работы
	*
	* Сервер решил закрыть соединение, здесь ещё есть время
	* корректно попрощаться с пиром (peer).
	*/
	virtual void onTerminate();
public:
	/**
	* Конструктор потока
	*/
	DCBot();
	
	/**
	* Деструктор потока
	*/
	~DCBot();
};

#endif // MAWAR_DCBOT_H
