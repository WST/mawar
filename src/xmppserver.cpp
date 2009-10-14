
#include <xmppserver.h>
#include <xmppstream.h>
#include <string>

using namespace std;

/**
* Конструктор сервера
*/
XMPPServer::XMPPServer(NetDaemon *d): daemon(d),
	// TODO перенести в vhost
	GSASLServer()
{
	users = ini_open("users.ini");
}

/**
* Деструктор сервера
*/
XMPPServer::~XMPPServer()
{
	ini_close(users);
	onError("TODO: cleanup in XMPPServer::~XMPPServer");
}

/**
* Обработчик события: подключение клиента
*/
AsyncObject* XMPPServer::onAccept()
{
	int sock = accept();
	if ( sock )
	{
		XMPPStream *client = new XMPPStream(this, sock);
		daemon->addObject(client);
		return client;
	}
	return 0;
}

/**
* Вернуть пароль пользователя по логину и vhost
* TODO декостылизация...
*/
using namespace std;
#include <iostream>
std::string XMPPServer::getUserPassword(const std::string &vhost, const std::string &login)
{
	const char *pwd = ini_get(users, vhost.c_str(), login.c_str(), "");
	cout << "host: " << vhost << ", login: " << login << ", password: " << pwd << endl;	
	return pwd;
}

/**
* Обработчик авторизации пользователя
* @param username логин пользователя
* @param realm realm...
* @param password пароль пользователя
* @return TRUE - авторизован, FALSE - логин или пароль не верен
*/
bool XMPPServer::onSASLAuthorize(const std::string &username, const std::string &realm, const std::string &password)
{
	cout << "onSASLAuthorize(" << username << ", " << realm << ", " << password << endl;
	string userPassword = getUserPassword(username, realm);
	return userPassword != "" && userPassword == password;
}
