#ifndef MAWAR_VIRTUALHOST_H
#define MAWAR_VIRTUALHOST_H

#include <xmppserver.h>
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
class VirtualHost: public GSASLServer
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
		virtual void handleIq(Stanza stanza); // Обработать iq, адресованный данному виртуальному узлу или пользователю на нём
		virtual void handleMessage(Stanza stanza); // Обработать message
		virtual void handlePresence(Stanza stanza); // Обработать presence
		const std::string &hostname(); // Вернуть имя хоста
		
		/**
		* Найти поток по JID (thread-safe)
		*
		* @note возможно в нем отпадет необходимость по завершении routeStanza()
		*/
		XMPPStream *getStreamByJid(const JID &jid);
		
		/**
		* Событие: Пользователь появился в online (thread-safe)
		* @param stream поток
		*/
		virtual void onOnline(XMPPStream *stream);
		
		/**
		* Событие: Пользователь ушел в offline (thread-safe)
		* @param stream поток
		*/
		virtual void onOffline(XMPPStream *stream);
		void saveOfflineMessage(Stanza stanza);
		
		/**
		* Вернуть пароль пользователя по логину
		* @param realm домен
		* @param login логин пользователя
		* @return пароль пользователя или "" если нет такого пользователя
		*/
		std::string getUserPassword(const std::string &realm, const std::string &login);
		
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
		/* TODO virtual */ bool routeStanza(const JID &to, Stanza stanza);
		
	private:
		void handleVHostIq(Stanza stanza); // Обработать IQ, адресованный данному виртуальному узлу
		bool sendRoster(Stanza stanza); // Отправить ростер в ответ на станзу stanza
		void addRosterItem(Stanza stanza, std::string jid, std::string name, std::string group);
		typedef std::map<std::string, XMPPStream *> reslist_t;
		typedef std::map<std::string, reslist_t> sessions_t;
		XMPPServer *server; // Ссылка на сервер которому принадлежит виртуальный хост
		std::string name; // Имя виртуального узла
		sessions_t onliners; // Онлайнеры
		std::map<std::string, unsigned long int> id_users;
};

#endif // MAWAR_VIRTUALHOST_H
