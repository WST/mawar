#ifndef MAWAR_XMPPCLIENT_H
#define MAWAR_XMPPCLIENT_H

#include <xmppstream.h>
#include <nanosoft/gsaslserver.h>
#include <xml_types.h>
#include <stanza.h>
#include <presence.h>

/**
* Класс XMPP-поток (c2s)
*/
class XMPPClient: public XMPPStream
{
protected:
	/**
	* Виртуальный хост
	*/
	class VirtualHost *vhost;
	
	/**
	* Сеанс авторизации SASL
	*/
	GSASLSession *sasl;
	
	enum state_t {
		/**
		* Начальное состояние - инициализация и авторизация
		*/
		init,
		
		/**
		* Авторизован - авторизовался и готов работать
		*/
		authorized,
		
		/**
		* Завершение - сервер завершает свою работу,
		* соединение готовиться к закрытию, нужно
		* корректно попрощаться с клиентом
		*/
		terminating
	} state;
	
	JID client_jid;
	
	/**
	* ID пользователя
	*/
	int user_id;
	
	ClientPresence client_presence;
	
	/**
	* Маркер online/offline
	*
	* TRUE - Initial presense уже отправлен
	* FLASE - Initial presense ещё не отправлен
	*/
	bool available;
	
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
	* Признак использования ростера
	* TRUE - пользователь запросил ростер
	* FALSE - пользователь не запрашивал ростер и хочет с ним работать
	*/
	bool use_roster;
	
	/**
	* Конструктор потока
	*/
	XMPPClient(XMPPServer *srv, int sock);
	
	/**
	* Деструктор потока
	*/
	~XMPPClient();
	
	/**
	* JID потока
	*/
	JID jid() const;

	/**
	* Показывает, что клиент уже авторизовался
	*/
	bool isAuthorized();
	
	/**
	* ID пользователя
	*/
	int userId() const {
		return user_id;
	}

	/**
	* Приоритет ресурса
	*/
	ClientPresence presence();
	
	/**
	* Обработчик станзы
	*/
	virtual void onStanza(Stanza stanza);
	
	/**
	* Обработчик авторизации
	*/
	virtual void onAuthStanza(Stanza stanza);
	
	/**
	* Обработка этапа авторизации SASL
	*/
	virtual void onSASLStep(const std::string &input);
	
	/**
	* Обработчик авторизации: ответ клиента
	*/
	virtual void onResponseStanza(Stanza stanza);
	
	/**
	* Обработчик iq-станзы
	*/
	virtual void onIqStanza(Stanza stanza);
	
	/**
	* Обработчик message-станзы
	*/
	virtual void onMessageStanza(Stanza stanza);
	
	/**
	* RFC 3921 (5.1.1) Initial Presence
	*/
	void handleInitialPresence(Stanza stanza);
	
	/**
	* RFC 3921 (5.1.2) Presence Broadcast
	*/
	void handlePresenceBroadcast(Stanza stanza);
	
	/**
	* RFC 3921 (5.1.3) Presence Probes
	*/
	void handlePresenceProbes();
	
	/**
	* RFC 3921 (5.1.4) Directed Presence
	*/
	void handleDirectedPresence(Stanza stanza);
	
	/**
	* RFC 3921 (5.1.5) Unavailable Presence
	*/
	void handleUnavailablePresence();
	
	/**
	* RFC 3921 (5.1.6) Presence Subscriptions
	*/
	void handlePresenceSubscriptions(Stanza stanza);
	
	/**
	* Обработчик presence-станзы
	*/
	virtual void onPresenceStanza(Stanza stanza);
};

#endif // MAWAR_XMPPCLIENT_H
