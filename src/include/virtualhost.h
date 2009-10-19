#ifndef MAWAR_VIRTUALHOST_H
#define MAWAR_VIRTUALHOST_H

#include <xmppserver.h>
#include <configfile.h>
#include <string>
#include <stanza.h>

/**
* Класс виртуального узла
*/
class VirtualHost
{
	public:
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
		XMPPStream *getStreamByJid(JID jid);
		virtual void onOnline(XMPPStream *stream);
		virtual void onOffline(XMPPStream *stream);
	
	private:
		void handleVHostIq(Stanza stanza); // Обработать IQ, адресованный данному виртуальному узлу
		typedef std::map<std::string, XMPPStream *> reslist_t;
		typedef std::map<std::string, reslist_t> sessions_t;
		XMPPServer *server; // Ссылка на сервер которому принадлежит виртуальный хост
		std::string name; // Имя виртуального узла
		sessions_t onliners; // Онлайнеры
};

#endif // MAWAR_VIRTUALHOST_H
