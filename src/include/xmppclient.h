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
		terminating,
		
		/**
		* начал свою сессию
		*/
		session
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
	* Показывает, что клиент начал сессию
	*/
	bool isActive();
	
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
	* RFC 3921 (8.2) Presence Subscribe
	*/
	void handlePresenceSubscribe(Stanza stanza);
	
	/**
	* RFC 3921 (8.2) Presence Subscribed
	*/
	void handlePresenceSubscribed(Stanza stanza);
	
	/**
	* RFC 3921 (8.4) Presence Unsubscribe
	*/
	void handlePresenceUnsubscribe(Stanza stanza);
	
	/**
	* RFC 3921 (8.2.1) Presence Unsubscribed
	*/
	void handlePresenceUnsubscribed(Stanza stanza);
	
	/**
	* RFC 3921 (5.1.6) Presence Subscriptions
	*/
	void handlePresenceSubscriptions(Stanza stanza);
	
	/**
	* Обработчик presence-станзы
	*/
	virtual void onPresenceStanza(Stanza stanza);
	
	/**
	* Чтение ростера клиентом
	*
	* RFC 3921 (7.3) Retrieving One's Roster on Login
	*/
	void handleRosterGet(Stanza stanza);
	
	/**
	* Добавить/обновить контакт в ростере
	*
	* RFC 3921 (7.4) Adding a Roster Item
	* RFC 3921 (7.5) Updating a Roster Item
	*/
	void handleRosterItemSet(TagHelper item);
	
	/**
	* Удалить контакт из ростера
	*
	* RFC 3921 (7.6) Deleting a Roster Item
	* RFC 3921 (8.6) Removing a Roster Item and Canceling All Subscriptions
	*/
	void handleRosterItemRemove(TagHelper item);
	
	/**
	* Обновить ростер
	*/
	void handleRosterSet(Stanza stanza);
	
	/**
	* Обработка станз jabber:iq:roster
	*
	* RFC 3921 (7.) Roster Managment
	*/
	void handleRosterIq(Stanza stanza);
};

#endif // MAWAR_XMPPCLIENT_H
