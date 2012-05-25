#ifndef MAWAR_XMPPSERVEROUTPUT_H
#define MAWAR_XMPPSERVEROUTPUT_H

#include <xmppdomain.h>
#include <xmppstream.h>
#include <nanosoft/asyncdns.h>
#include <nanosoft/mutex.h>

/**
* Класс домена s2s-вывода
*
* Данный класс управляет подключением к внешнему домену
* и списком авторизованных виртуальных хостов.
* Для каждого авторизованного виртуального хоста
* заводиться свой псевдо-поток.
*
* XMPPServerOutput - клиентский сокет, исходящие станзы
* XMPPServerInput - серверный сокет, входящие станзы
*/
class XMPPServerOutput: public XMPPDomain, XMPPStream
{
protected:
	/**
	* Mutex для thread-safe доступа к общим данным
	*/
	nanosoft::Mutex mutex;
	
	/**
	* Состояние s2s-домена
	*/
	enum {
		/**
		* Резолвим DNS
		*/
		RESOLVING,
		
		/**
		* Подключаемся к серверу
		*/
		CONNECTING,
		
		/**
		* Подключены
		*/
		CONNECTED
	} state;
	
	/**
	* Порт s2s-сервера
	*/
	int port;
	
	/**
	* Класс буфера исходящих станз
	*/
	typedef std::list<std::string> buffer_t;
	
	/**
	* Описание виртуального хоста
	*/
	struct vhost_t
	{
		/**
		* Виртуальный хост автризован для передачи данных
		*/
		bool authorized;
		
		/**
		* Буфер станз ожидающих коннекта
		*/
		buffer_t connbuffer;
		
		/**
		* Буфер станз ожидающих авторизации виртуального хоста
		*/
		buffer_t buffer;
	};
	
	/**
	* Список авторизуемых виртуальных хостов
	*/
	typedef std::map<std::string, vhost_t *> vhosts_t;
	
	/**
	* Список авторизуемых виртуальных хостов
	*/
	vhosts_t vhosts;
	
	/**
	* Событие готовности к записи
	*
	* Вызывается, когда в поток готов принять
	* данные для записи без блокировки
	*/
	virtual void onWrite();
	
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
	void onPeerDown();
	
	/**
	* Сигнал завершения работы
	*
	* Сервер решил закрыть соединение, здесь ещё есть время
	* корректно попрощаться с пиром (peer).
	*/
	virtual void onTerminate();
	
	/**
	* Резолвер s2s хоста, запись A (IPv4)
	*/
	static void on_s2s_output_a4(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data);
	
	/**
	* Резолвер s2s хоста, запись SRV (_xmpp-server._tcp)
	*/
	static void on_s2s_output_xmpp_server(struct dns_ctx *ctx, struct dns_rr_srv *result, void *data);
	
	/**
	* Резолвер s2s хоста, запись SRV (_jabber._tcp)
	*/
	static void on_s2s_output_jabber(struct dns_ctx *ctx, struct dns_rr_srv *result, void *data);
	
	/**
	* 2.1.1 Originating Server Generates Outbound Request for Authorization by Receiving Server
	* 
	* XEP-0220: Server Dialback
	*/
	void sendDBRequest(const std::string &from, const std::string &to);
public:
	/**
	* Случайный ключ
	*/
	char key[32];
	
	/**
	* Конструктор s2s-домена
	*/
	XMPPServerOutput(XMPPServer *srv, const char *host);
	
	/**
	* Деструктор s2s-домена
	*/
	~XMPPServerOutput();
	
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

#endif // MAWAR_XMPPSERVEROUTPUT_H
