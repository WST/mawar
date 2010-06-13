#ifndef MAWAR_VIRTUALHOST_H
#define MAWAR_VIRTUALHOST_H

#include <xmppserver.h>
#include <xmppclient.h>
#include <xmppdomain.h>
#include <configfile.h>
#include <string>
#include <stanza.h>
#include <jid.h>
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
		* Конструктор
		* @param srv ссылка на сервер
		* @param aName имя хоста
		* @param config конфигурация хоста
		*/
		VirtualHost(XMPPServer *srv, const std::string &aName, VirtualHostConfig config);
		~VirtualHost();
		
		virtual void handleMessage(Stanza stanza); // Обработать message
		
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
		* Регистрация пользователей
		*/
		void handleRegisterIq(XMPPClient *client, Stanza stanza);
	private:
		void handleVHostIq(Stanza stanza); // Обработать IQ, адресованный данному виртуальному узлу
		void handleVcardRequest(Stanza stanza); // Обработать запрос vCard
		
		typedef std::map<std::string, XMPPClient *> reslist_t;
		typedef std::map<std::string, reslist_t> sessions_t;
		sessions_t onliners; // Онлайнеры
		std::map<std::string, unsigned long int> id_users;
		
		bool registration_allowed; // разрешена ли регистрация
		unsigned long int onliners_number; // число подключённых пользователей
};

#endif // MAWAR_VIRTUALHOST_H
