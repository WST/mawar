#ifndef MAWAR_XMPPSERVER_H
#define MAWAR_XMPPSERVER_H

#include <nanosoft/netdaemon.h>
#include <nanosoft/asyncserver.h>
#include <nanosoft/gsaslserver.h>
#include <nanoini.h>
#include <string>

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
	* Вернуть пароль пользователя по логину и vhost
	* TODO декостылизация... хм... а может и так пойдет :-)
	*/
	std::string getUserPassword(const std::string &vhost, const std::string &login);
	
	/**
	* Вернуть список всех пользователей сервера
	* TODO временый костыль для временного ростера
	*/
	users_t getUserList();
};

#endif // MAWAR_XMPPSERVER_H
