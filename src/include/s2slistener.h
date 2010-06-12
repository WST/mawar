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
private:
	/**
	* Класс буфера исходящих станз
	*/
	typedef std::list<std::string> buffer_t;
	
	/**
	* Структура описывающая ожидающие s2s-соединения
	* И хранящая буфер исходящих станз
	*/
	struct pending_t
	{
		/**
		* Ссылка на сервер
		*/
		class XMPPServer *server;
		
		std::string to;
		std::string from;
		int port;
		
		/**
		* Буфер исходящих станз
		*/
		buffer_t buffer;
	};
	
	/**
	* Класс списка ожидающих соединений
	*/
	typedef std::map<std::string, pending_t *> pendings_t;
	
	/**
	* Список ожидающих соединений
	*/
	pendings_t pendings;
	
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
protected:
	/**
	* Mutex для thread-safe доступа к общим данным
	*/
	nanosoft::Mutex mutex;
	
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
	
	/**
	* Роутер исходящих s2s-станз (thread-safe)
	* @param host домент в который надо направить станзу
	* @param stanza станза
	* @return TRUE - станза была отправлена, FALSE - станзу отправить не удалось
	*/
	bool routeStanza(const char *host, Stanza stanza);
	
	/**
	* Отправить буферизованные станзы
	*/
	void flush(class S2SOutputStream *stream);
};

#endif // MAWAR_S2SLISTENER_H
