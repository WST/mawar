
#include <xmppserver.h>
#include <xmppstream.h>
#include <virtualhost.h>
#include <configfile.h>
#include <string>
#include <iostream>
#include <nanosoft/mysql.h>

using namespace std;
using namespace nanosoft;

/**
* Конструктор сервера
*/
XMPPServer::XMPPServer(NetDaemon *d): daemon(d)
{
}

/**
* Деструктор сервера
*/
XMPPServer::~XMPPServer()
{
	// delete virtual hosts
	for(vhosts_t::iterator iter = vhosts.begin(); iter != vhosts.end(); ++iter)
	{
		delete iter->second;
	}
	vhosts.clear();
}

/**
* Обработчик события: подключение клиента
*
* thread-safe
*/
AsyncObject* XMPPServer::onAccept()
{
	mutex.lock();
		int sock = accept();
		XMPPStream *client = 0;
		if ( sock )
		{
			client = new XMPPStream(this, sock);
			daemon->addObject(client);
		}
	mutex.unlock();
	return client;
}

/**
* Сигнал завершения работы
*
* Объект должен закрыть файловый дескриптор
* и освободить все занимаемые ресурсы
*/
void XMPPServer::onTerminate()
{
	onError("XMPPServer::onTerminate()...");
	daemon->removeObject(this);
	onError("XMPPServer::onTerminate() leave...");
}

/**
* Сигнал завершения
*/
void XMPPServer::onSigTerm()
{
	cerr << "\n[XMPPServer]: onSigTerm() enter\n";
	daemon->terminate();
	cerr << "[XMPPServer]: onSigTerm() leave\n";
}

/**
* Сигнал HUP
*/
void XMPPServer::onSigHup()
{
	//cerr << "[XMPPServer]: TODO XMPPServer::onSigHup\n";
}

/**
* Вернуть виртуальный хост по имени
*
* thread-safe
*
* @param name имя искомого хоста
* @return виртуальный хост или 0 если такого хоста нет
*/
VirtualHost* XMPPServer::getHostByName(const std::string &name)
{
	mutex.lock();
		vhosts_t::const_iterator iter = vhosts.find(name);
		VirtualHost *vhost = iter != vhosts.end() ? iter->second : 0;
	mutex.unlock();
	return vhost;
}

/**
* Добавить виртуальный хост
*
* thread-safe
*/
void XMPPServer::addHost(const std::string &name, VirtualHostConfig config)
{
	mutex.lock();
		vhosts[name] = new VirtualHost(this, name, config);
	mutex.unlock();
}
