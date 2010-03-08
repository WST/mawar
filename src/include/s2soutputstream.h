#ifndef MAWAR_S2SOUTPUTSTREAM_H
#define MAWAR_S2SOUTPUTSTREAM_H

#include <xmppstream.h>

/**
* Класс потока вывода (s2s output)
*
* Поток вывода авторизуется только для получения станз
* от удаленного хоста, для отправки станз удаленному
* хосту используется другой тип потока - S2SInputStream
* (s2s input).
*
* S2SOutputStream - клиентский сокет
* S2SInputStream - серверный сокет
*/
class S2SOutputStream: public XMPPStream
{
protected:
	/**
	* Виртуальный хост
	*/
	class VirtualHost *vhost;
	
	/**
	* Удаленный хост
	*/
	std::string remote_host;
	
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
	* Сигнал завершения работы
	*
	* Объект должен закрыть файловый дескриптор
	* и освободить все занимаемые ресурсы
	*/
	virtual void onTerminate();
	
public:
	/**
	* Конструктор потока
	*/
	S2SOutputStream(XMPPServer *srv, int sock);
	
	/**
	* Деструктор потока
	*/
	~S2SOutputStream();
	
	/**
	* Удаленный хост
	*/
	std::string remoteHost() const { return remote_host; }
	
	/**
	* Событие закрытие соединения
	*
	* Вызывается если peer закрыл соединение
	*/
	virtual void onShutdown();
	
	/**
	* Обработчик станзы
	*/
	virtual void onStanza(Stanza stanza);
};

#endif // MAWAR_S2SOUTPUTSTREAM_H
