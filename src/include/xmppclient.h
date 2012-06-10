#ifndef MAWAR_XMPPCLIENT_H
#define MAWAR_XMPPCLIENT_H

#include <xmppstream.h>
#include <nanosoft/gsaslserver.h>
#include <xml_types.h>
#include <stanza.h>
#include <presence.h>
#include "zlib.h"

/**
* Класс XMPP-поток (c2s)
*/
class XMPPClient: public XMPPStream
{
private:
	/**
	* Контекст компрессора zlib
	*/
	z_stream zin;
	
protected:
	/**
	* Виртуальный хост
	*/
	class VirtualHost *vhost;
	
	/**
	* Сеанс авторизации SASL
	*/
	GSASLSession *sasl;
	
	/**
	* ID пользователя
	*/
	int user_id;
	
	ClientPresence client_presence;
	
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
	* Признак использования ростера
	* TRUE - пользователь запросил ростер
	* FALSE - пользователь не запрашивал ростер и хочет с ним работать
	*/
	bool use_roster;
	
	/**
	* Флаг компрессии
	* XEP-0138: Stream Compression
	*
	* TRUE - компрессия включена
	* FALSE - компрессия отключена
	*/
	bool compression;
	
	/**
	* Клиент авторизовался
	* 
	* TRUE - клиент авторизовался
	* FALSE - клиент не авторизовался
	*/
	bool authorized;
	
	/**
	* "connected resource"
	*
	* TRUE - client has bound a resource to the stream
	* FALSE - client hasn't bound a resource to the stream
	*/
	bool connected;
	
	/**
	* Маркер online/offline
	*
	* TRUE - Initial presense уже отправлен
	* FLASE - Initial presense ещё не отправлен
	*/
	bool available;
	
	/**
	* JID клиента
	*/
	JID client_jid;
	
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
	* Показывает, что клиент начал сессию и onOnline() уже было вызвано
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
	* Обработчик запроса компрессии
	*/
	void handleCompress(Stanza stanza);
	
	/**
	* Устаревший обработчик iq roster
	*
	* TODO необходима ревизия, скорее всего надо перенести в VirtualHost
	* или в отдельный модуль
	*/
	void handleIQRoster(Stanza stanza);
	
	/**
	* Обработчик iq-станзы
	*/
	void onIqStanza(Stanza stanza);
	
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
	void handleUnavailablePresence(Stanza stanza);
	
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
	void onPresenceStanza(Stanza stanza);
	
	/**
	* Обработчик message-станзы
	*/
	void onMessageStanza(Stanza stanza);
	
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
	* Проверить корректность запроса RosterSet
	*/
	bool checkRosterSet(Stanza stanza);
	
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
	
	/**
	* XEP-0077: In-Band Registration
	* 
	* c2s-версия: регистрация происходит как правило до полноценной
	* авторизации клиента, соответственно vhost не может адресовать
	* такого клиента, поэтому приходиться сделать две отдельные
	* версии регистрации: одну для c2s, другую для s2s
	*/
	void handleIQRegister(Stanza stanza);
};

#endif // MAWAR_XMPPCLIENT_H
