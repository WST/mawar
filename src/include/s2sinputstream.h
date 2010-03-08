#ifndef MAWAR_S2SINPUTSTREAM_H
#define MAWAR_S2SINPUTSTREAM_H

#include <xmppstream.h>

/**
* Класс потока ввода (s2s input)
*
* Поток ввода авторизуется только для отправки станз
* удалённому хосту, для получения станз от удаленного
* хоста используется другой тип потока - S2SOutputStream
* (s2s output).
*
* S2SOutputStream - клиентский сокет
* S2SInputStream - серверный сокет
*/
class S2SInputStream: public XMPPStream
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
	S2SInputStream(XMPPServer *srv, int sock);
	
	/**
	* Деструктор потока
	*/
	~S2SInputStream();
	
	/**
	* Удаленный хост
	*/
	std::string remoteHost() const;
	
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
	
	/**
	* Завершить сессию
	*
	* thread-safe
	*/
	void terminate();
};

#endif // MAWAR_S2SINPUTSTREAM_H
