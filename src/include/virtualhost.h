#ifndef MAWAR_VIRTUALHOST_H
#define MAWAR_VIRTUALHOST_H

#include <xmppserver.h>
#include <xmppclient.h>
#include <xmppdomain.h>
#include <xmppextension.h>
#include <configfile.h>
#include <string>
#include <stanza.h>
#include <jid.h>
#include <nanosoft/object.h>
#include <nanosoft/gsaslserver.h>
#include <nanosoft/mutex.h>
#include <db.h>

/**
* Класс виртуального узла
*/
class VirtualHost: public XMPPDomain, public GSASLServer
{
	public:
		/**
		* База данных
		*/
		DB db;
		
		/**
		* Mutex для thread-safe доступа к общим данным
		*/
		nanosoft::Mutex mutex;
		
		/**
		* RFC 6120, 7.7.2.2. Resource Binding, Conflict
		*
		* Возможные варианты поведения при конфликте ресурса во время bind
		*/
		enum bind_conflict_t {
			/**
			* Сервер генерирует клиенту новый ресурс
			*/
			bind_override,
			
			/**
			* Отказать в привязке ресурса новому клиенту
			*/
			bind_reject_new,
			
			/**
			* Выкинуть старого клиента
			*/
			bind_remove_old
		} state;
		
		/**
		* Конструктор
		* @param srv ссылка на сервер
		* @param aName имя хоста
		* @param config конфигурация хоста
		*/
		VirtualHost(XMPPServer *srv, const std::string &aName, ATXmlTag *config);
		~VirtualHost();
		
		virtual void handleMessage(Stanza stanza); // Обработать message
		
		/**
		* RFC 6120, 10.3.  No 'to' Address
		* 
		* If the stanza possesses no 'to' attribute, the server MUST handle
		* it directly on behalf of the entity that sent it, where the meaning
		* of "handle it directly" depends on whether the stanza is message,
		* presence, or IQ. Because all stanzas received from other servers
		* MUST possess a 'to' attribute, this rule applies only to stanzas
		* received from a local entity (typically a client) that is connected
		* to the server. 
		*/
		void handleDirectly(Stanza stanza, XMPPClient *client = 0);
		
		/**
		* RFC 6120, 10.3.1  No 'to' Address, Message
		* 
		* If the server receives a message stanza with no 'to' attribute,
		* it MUST treat the message as if the 'to' address were the bare
		* JID <localpart@domainpart> of the sending entity. 
		*/
		void handleDirectlyMessage(Stanza stanza, XMPPClient *client = 0);
		
		/**
		* RFC 6120, 10.3.2  No 'to' Address, Presence
		* 
		* If the server receives a presence stanza with no 'to' attribute,
		* it MUST broadcast it to the entities that are subscribed to the
		* sending entity's presence, if applicable ([XMPP‑IM] defines the
		* semantics of such broadcasting for presence applications). 
		*/
		void handleDirectlyPresence(Stanza stanza, XMPPClient *client = 0);
		
		/**
		* RFC 6120, 10.3.3  No 'to' Address, IQ
		* 
		* If the server receives an IQ stanza with no 'to' attribute, it MUST
		* process the stanza on behalf of the account from which received
		* the stanza
		*/
		void handleDirectlyIQ(Stanza stanza, XMPPClient *client = 0);
		
		/**
		* RFC 6120, 7. Resource Binding
		*/
		void handleIQBind(Stanza stanza, XMPPClient *client);
		
		/**
		* XEP-0012: Last Activity
		*/
		void handleIQLast(Stanza stanza);
		
		/**
		* XEP-0030: Service Discovery #info
		*/
		void handleIQServiceDiscoveryInfo(Stanza stanza);
		
		/**
		* XEP-0030: Service Discovery #items
		*/
		void handleIQServiceDiscoveryItems(Stanza stanza);
		
		/**
		* XEP-0039: Statistics Gathering
		*/
		void handleIQStats(Stanza stanza);
		
		/**
		* XEP-0049: Private XML Storage
		*/
		void handleIQPrivateStorage(Stanza stanza);
		
		/**
		* XEP-0050: Ad-Hoc Commands
		*/
		void handleIQAdHocCommands(Stanza stanza);
		
		/**
		* XEP-0054: vcard-temp
		*/
		void handleIQVCardTemp(Stanza stanza);
		
		/**
		* XEP-0092: Software Version
		*/
		void handleIQVersion(Stanza stanza);
		
		/**
		* XEP-0199: XMPP Ping
		*/
		void handleIQPing(Stanza stanza);
		
		/**
		* Обработка запрещенной IQ-станзы (недостаточно прав)
		*/
		void handleIQForbidden(Stanza stanza);
		
		/**
		* Обработка неизвестной IQ-станзы
		*/
		void handleIQUnknown(Stanza stanza);
		
		/**
		* Enable/disable user registration command
		*
		* XEP-0050: Ad-Hoc Commands
		*/
		void handleIQAdHocEnableRegistration(Stanza stanza);
		
		/**
		* Обслуживание обычного презенса
		*/
		void serveCommonPresence(Stanza stanza);
		
		/**
		* Отправить presence unavailable со всех ресурсов пользователя
		* @param from логин пользователя от имени которого надо отправить
		* @param to jid (bare) которому надо отправить
		*/
		void broadcastUnavailable(const std::string &from, const std::string &to);
		
		/**
		* Обслуживание Presence Probes
		*
		* RFC 3921 (5.1.3) Presence Probes
		*/
		void servePresenceProbes(Stanza stanza);
		
		/**
		* RFC 3921 (8.2) Presence Subscribe
		*/
		void servePresenceSubscribe(Stanza stanza);
		
		/**
		* RFC 3921 (8.2) Presence Subscribed
		*/
		void servePresenceSubscribed(Stanza stanza);
		
		/**
		* RFC 3921 (8.4) Presence Unsubscribe
		*/
		void servePresenceUnsubscribe(Stanza stanza);
		
		/**
		* RFC 3921 (8.2.1) Presence Unsubscribed
		*/
		void servePresenceUnsubscribed(Stanza stanza);
		
		/**
		* Обслуживаение Presence Subscriptions
		*
		* RFC 3921 (5.1.6) Presence Subscriptions
		*/
		void servePresenceSubscriptions(Stanza stanza);
		
		/**
		* Серверная часть обработки станзы presence
		*
		* Вся клиентская часть находиться в классе XMPPClinet.
		* Сюда попадают только станзы из сети (s2s)
		* или из других виртуальных хостов.
		*/
		void servePresence(Stanza stanza);
		
		/**
		* Отправить станзу всем ресурсам указаного пользователя
		*/
		void broadcast(Stanza stanza, const std::string &login);
		
		/**
		* Найти клиента по JID (thread-safe)
		*
		* @note возможно в нем отпадет необходимость по завершении routeStanza()
		*/
		XMPPClient *getClientByJid(const JID &jid);
		
		/**
		* Найти клиента по JID (thread-unsafe)
		*
		* @note возможно в нем отпадет необходимость по завершении routeStanza()
		*/
		XMPPClient *getClientByJid0(const JID &jid);
		
		/**
		* Генерировать случайный ресурс для абонента
		*/
		std::string genResource(const char *username);
		
		/**
		* Привязать ресурс
		*/
		bool bindResource(const char *resource, XMPPClient *client);
		
		/**
		* Отвязать ресурс
		*/
		void unbindResource(const char *resource, XMPPClient *client);
		
		/**
		* Событие: Пользователь появился в online (thread-safe)
		* @param client поток
		*/
		virtual void onOnline(XMPPClient *client);
		
		/**
		* Отправить Presence Unavailable всем, кому был отправлен Directed Presence
		* @param client клиент, который отправлял Directed Presence
		*/
		void unavailableDirectedPresence(XMPPClient *client);
		
		/**
		* Событие: Пользователь ушел в offline (thread-safe)
		* @param client поток
		*/
		virtual void onOffline(XMPPClient *client);
		void saveOfflineMessage(Stanza stanza);
		void sendOfflineMessages(XMPPClient *client);
		
		/**
		* Вернуть пароль пользователя по логину
		* @param realm домен
		* @param login логин пользователя
		* @return пароль пользователя или "" если нет такого пользователя
		*/
		std::string getUserPassword(const std::string &realm, const std::string &login);
		
		/**
		* Вернуть ID пользователя
		* @param login логин пользователя
		* @return ID пользователя
		*/
		int getUserId(const std::string &login);
		
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
		
		/**
		* Roster Push
		*
		* Отправить станзу всем активным ресурсам пользователя
		* @NOTE если ресурс не запрашивал ростер, то отправлять не нужно
		* @param username логин пользователя которому надо отправить
		* @param stanza станза которую надо отправить
		*/
		void rosterPush(const std::string &username, Stanza stanza);
		
		/**
		* XEP-0077: In-Band Registration
		*
		* Регистрация пользователей
		*/
		void handleRegisterIq(XMPPClient *client, Stanza stanza);
		
		bool isAdmin(std::string barejid);
		
		bool userExists(std::string username);
		
		/**
		* Добавить расширение
		*/
		void addExtension(const char *urn, const char *fname);
		
		/**
		* Удалить расширение
		*/
		void removeExtension(const char *urn);
	
	private:
		void handleVHostIq(Stanza stanza); // Обработать IQ, адресованный данному виртуальному узлу
		
		typedef std::map<std::string, XMPPClient *> reslist_t;
		typedef std::map<std::string, reslist_t> sessions_t;
		sessions_t onliners; // Онлайнеры
		
		typedef std::map<std::string, nanosoft::ptr<XMPPExtension> > extlist_t;
		extlist_t ext;
		
		bool registration_allowed; // разрешена ли регистрация
		
		bind_conflict_t bind_conflict; // политика разрешения конфликта ресурсов
		unsigned long int onliners_number; // число подключённых пользователей
		unsigned long int vcard_queries; // число запросов vcard
		unsigned long int stats_queries; // число запросов статистики
		unsigned long int xmpp_ping_queries; // число обслуженных XMPP-пингов
		unsigned long int xmpp_error_queries; // число неопозанных запросов
		unsigned long int version_requests; // число запросов верси сервера
		unsigned long int start_time; // момент запуска виртуального узл
		
		ATXmlTag *config;
};

#endif // MAWAR_VIRTUALHOST_H
