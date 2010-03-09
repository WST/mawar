#ifndef MAWAR_S2SOUTPUTSTREAM_H
#define MAWAR_S2SOUTPUTSTREAM_H

#include <xmppstream.h>
#include <xmppdomain.h>

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
class S2SOutputStream: public XMPPStream, XMPPDomain
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
	std::string local_host;
	
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
	* Сервер решил закрыть соединение, здесь ещё есть время
	* корректно попрощаться с пиром (peer).
	*/
	virtual void onTerminate();
	
public:
	/**
	* Конструктор потока
	*/
	S2SOutputStream(XMPPServer *srv, int sock, const std::string &tohost, const std::string &fromhost);
	
	/**
	* Деструктор потока
	*/
	~S2SOutputStream();
	
	/**
	* Удаленный хост
	*/
	std::string remoteHost() const { return remote_host; }
	
	/**
	* Открыть поток
	*/
	void startStream();
	
	/**
	* Обработка <db:verify>
	*/
	void onDBVerifyStanza(Stanza stanza);
	
	/**
	* Обработка <db:result>
	*/
	void onDBResultStanza(Stanza stanza);
	
	/**
	* Обработчик станзы
	*/
	virtual void onStanza(Stanza stanza);
	
	/**
	* Роутер исходящих станз (thread-safe)
	*
	* Роутер передает станзу нужному потоку.
	*
	* @note Данная функция отвечает только за маршрутизацию, она не сохраняет офлайновые сообщения:
	*   если адресат online, то пересылает ему станзу,
	*   если offline, то вернет FALSE и вызывающая сторона должна сама сохранить офлайновое сообщение.
	*
	* @note Данный метод вызывается из глобального маршрутизатора станз XMPPServer::routeStanza()
	*   вызывать его напрямую из других мест не рекомендуется - используйте XMPPServer::routeStanza()
	*
	* @note Данный метод в будущем станет виртуальным и будет перенесен в специальный
	*   базовый класс, от которого будут наследовать VirtualHost (виртуальные узлы)
	*   и, к примеру, MUC. Виртуальые узлы и MUC имеют общие черты, оба адресуются
	*   доменом, оба принимают входящие станзы, но обрабатывают их по разному,
	*   VirtualHost доставляет сообщения своим пользователям, а MUC доставляет
	*   сообщения участникам комнат.
	*
	* @param to адресат которому надо направить станзу
	* @param stanza станза
	* @return TRUE - станза была отправлена, FALSE - станзу отправить не удалось
	*/
	virtual bool routeStanza(Stanza stanza);
};

#endif // MAWAR_S2SOUTPUTSTREAM_H
