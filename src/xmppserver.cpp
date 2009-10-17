
#include <xmppserver.h>
#include <xmppstream.h>
#include <string>
#include <iostream>

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

/**
* TODO временый костыль для временного ростера
*/
struct online_balon_t
{
	XMPPServer::users_t users;
};

/**
* TODO временый костыль для временного ростера
*/
static int users_callback(ini_p ini, const char *section, const char *key, const char *value, void *balon)
{
	static_cast< XMPPServer::users_t* >(balon)->push_back(string(key) + "@" + section);
	return 0;
}

/**
* TODO временый костыль для временного ростера
*/
static int users_vhosts_callback(ini_p ini, const char *section, void *balon)
{
	ini_section_map(ini, section, users_callback, balon);
	return 0;
}

/**
* Вернуть список всех онлайнеров
* TODO временый костыль для временного ростера
*/
XMPPServer::users_t XMPPServer::getUserList()
{
	XMPPServer::users_t list;
	ini_map(users, users_vhosts_callback, &list);
	return list;
}

/**
* Событие появления нового онлайнера
* @param stream поток
*/
void XMPPServer::onOnline(XMPPStream *stream)
{
	std::map<std::string, XMPPStream*> res;
	res[stream->jid().resource()] = stream;
	onliners[stream->jid().bare()] = res;
	cout << stream->jid().full() << " is online :-)\n";
}

/**
* Событие ухода пользователя в офлайн
*/
void XMPPServer::onOffline(XMPPStream *stream)
{
	onliners[stream->jid().bare()].erase(stream->jid().resource());
	if(onliners[stream->jid().bare()].empty()) {
		onliners.erase(stream->jid().bare());
	}
	cout << stream->jid().full() << " is offline :-(\n";
}
