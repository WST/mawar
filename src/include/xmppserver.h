#ifndef MAWAR_XMPPSERVER_H
#define MAWAR_XMPPSERVER_H

#include <nanosoft/netdaemon.h>
#include <nanosoft/asyncserver.h>
#include <nanosoft/gsaslserver.h>
#include <nanoini.h>
#include <string>

class XMPPStream;

/**
* Класс XMPP сервера
*/
class XMPPServer: public AsyncServer,
	// TODO пенерести в vhost
	public GSASLServer
{
private:
	/**
	* список пользователей
	* TODO декостылизация...
	*/
	ini_p users;
	
protected:
	/**
	* Обработчик авторизации пользователя
	* @param username логин пользователя
	* @param realm realm...
	* @param password пароль пользователя
	* @return TRUE - авторизован, FALSE - логин или пароль не верен
	*/
	virtual bool onSASLAuthorize(const std::string &username, const std::string &realm, const std::string &password);
	
public:
	/**
	* TODO декостылизация...
	*/
	typedef std::vector<std::string> users_t;

	/**
	* TODO декостылизация...
	* (добавил WST)
	*/
	typedef std::map<std::string, XMPPStream *> reslist_t;
	typedef std::map<std::string, reslist_t> sessions_t;
	
	/**
	* Ссылка на демона
	*/
	NetDaemon *daemon;
	
	/**
	* Конструктор сервера
	*/
	XMPPServer(NetDaemon *d);
	
	/**
	* Деструктор сервера
	*/
	~XMPPServer();
	
	/**
	* Обработчик события: подключение клиента
	*/
	AsyncObject* onAccept();
	
	/**
	* Событие появления нового онлайнера
	* @param stream поток
	*/
	void onOnline(XMPPStream *stream);
	
	/**
	* Событие ухода пользователя в офлайн
	*/
	void onOffline(XMPPStream *stream);
	
	/**
	* Вернуть пароль пользователя по логину и vhost
	* TODO декостылизация... хм... а может и так пойдет :-)
	*/
	std::string getUserPassword(const std::string &vhost, const std::string &login);
	
	/**
	* Вернуть список всех пользователей сервера
	* TODO временый костыль для временного ростера
	*/
	users_t getUserList();
	sessions_t onliners;
};

#endif // MAWAR_XMPPSERVER_H
