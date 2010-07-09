#ifndef MAWAR_S2SINPUTSTREAM_H
#define MAWAR_S2SINPUTSTREAM_H

#include <xmppstream.h>

/**
* Класс потока ввода (s2s input)
*
* Поток ввода авторизуется только для приёма станз
* от удалённого хоста, для отправки станз удалённому хосту
* используется другой тип потока - S2SOutputStream (s2s output).
*
* S2SOutputStream - клиентский сокет, исходящие станзы
* S2SInputStream - серверный сокет, входящие станзы
*/
class S2SInputStream: public XMPPStream
{
protected:
	/**
	* Виртуальный хост
	*/
	class VirtualHost *vhost;
	
	/**
	* Удаленный хост от которого мы принимаем станзы
	*/
	std::string remote_host;
	
	/**
	* ID сеанса
	*/
	std::string id;
	
	/**
	* Состояние потока
	*/
	enum {
		/**
		* Ициализация (ожидаем <stream:stream>)
		*/
		init,
		
		/**
		* Проверка (обрабатываем только <db:result>, <db:verify>)
		*/
		verify,
		
		/**
		* Авторизован (можно пересылать станзы)
		*/
		authorized
	} state;
	
	/**
	* Событие: начало потока
	*/
	virtual void onStartStream(const std::string &name, const attributes_t &attributes);
	
	/**
	* Событие: конец потока
	*/
	virtual void onEndStream();
	
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
	S2SInputStream(XMPPServer *srv, int sock);
	
	/**
	* Деструктор потока
	*/
	~S2SInputStream();
	
	/**
	* Удаленный хост от которого мы принимаем станзы
	*/
	std::string remoteHost() const { return remote_host; }
	
	/**
	* Обработчик станзы
	*/
	virtual void onStanza(Stanza stanza);
	
	/**
	* Обработка <db:verify>
	*/
	void onDBVerifyStanza(Stanza stanza);
	
	/**
	* Обработка <db:result>
	*/
	void onDBResultStanza(Stanza stanza);
};

#endif // MAWAR_S2SINPUTSTREAM_H
