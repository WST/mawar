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
		virtual void handlePresence(Stanza stanza); // Обработать presence
		virtual void handleSubscribed(Stanza stanza); // обработать presence[type=subscribed]
		
		/**
		* Presence Broadcast (RFC 3921, 5.1.2)
		*/
		void broadcastPresence(Stanza stanza);
		
		/**
		* Initial Presence (RFC 3921, 5.1.1)
		*/
		void initialPresence(Stanza stanza);
		
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
		* Событие: Пользователь ушел в offline (thread-safe)
		* @param client поток
		*/
		virtual void onOffline(XMPPClient *client);
		void saveOfflineMessage(Stanza stanza);
		
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
		* Обработка ростера
		*/
		virtual void handleRosterIq(XMPPClient *client, Stanza stanza);
	private:
		void handleVHostIq(Stanza stanza); // Обработать IQ, адресованный данному виртуальному узлу
		bool sendRoster(Stanza stanza); // Отправить ростер в ответ на станзу stanza
		
		/**
		* Добавить/обновить контакт в ростере
		* @param client клиент чей ростер обновляем
		* @param stanza станза управления ростером
		* @param item элемент описывающий изменения в контакте
		*/
		void setRosterItem(XMPPClient *client, Stanza stanza, TagHelper item);
		
		/**
		* Удалить контакт из ростера
		* @param client клиент чей ростер обновляем
		* @param stanza станза управления ростером
		* @param item элемент описывающий изменения в контакте
		*/
		void removeRosterItem(XMPPClient *client, Stanza stanza, TagHelper item);
		
		typedef std::map<std::string, XMPPClient *> reslist_t;
		typedef std::map<std::string, reslist_t> sessions_t;
		sessions_t onliners; // Онлайнеры
		std::map<std::string, unsigned long int> id_users;
};

#endif // MAWAR_VIRTUALHOST_H
