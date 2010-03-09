
#include <xmppserver.h>
#include <xmppclient.h>
#include <xmppdomain.h>
#include <virtualhost.h>
#include <s2slistener.h>
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
	fprintf(stderr, "#%d: [XMPPServer: %d] deleting\n", getWorkerId(), fd);
	
	// delete domains
	for(domains_t::iterator iter = domains.begin(); iter != domains.end(); ++iter)
	{
		delete iter->second;
	}
	domains.clear();
}

/**
* Принять входящее соединение
*/
void XMPPServer::onAccept()
{
	int sock = accept();
	if ( sock )
	{
		XMPPStream *client = new XMPPClient(this, sock);
		daemon->addObject(client);
	}
}

/**
* Сигнал завершения работы
*/
void XMPPServer::onTerminate()
{
	fprintf(stderr, "#%d: [XMPPServer: %d] onTerminate\n", getWorkerId(), fd);
	daemon->removeObject(this);
}

/**
* Сигнал завершения
*/
void XMPPServer::onSigTerm()
{
	fprintf(stderr, "#%d: [XMPPServer: %d] SIGTERM\n", getWorkerId(), fd);
	daemon->terminate();
}

/**
* Сигнал HUP
*/
void XMPPServer::onSigHup()
{
	//fprintf(stderr, "#%d: [XMPPServer: %d] SIGUP not implemented yet\n", getWorkerId(), fd);
}

/**
* Вернуть виртуальный хост по имени
*
* thread-safe
*
* @param name имя искомого хоста
* @return виртуальный хост или 0 если такого хоста нет
*/
XMPPDomain* XMPPServer::getHostByName(const std::string &name)
{
	mutex.lock();
		domains_t::const_iterator iter = domains.find(name);
		XMPPDomain *domain = iter != domains.end() ? iter->second : 0;
	mutex.unlock();
	return domain;
}

/**
* Добавить домен (thread-safe)
*/
void XMPPServer::addDomain(XMPPDomain *domain)
{
	mutex.lock();
		domains[domain->hostname()] = domain;
	mutex.unlock();
}

/**
* Удалить домен (thread-safe)
*/
void XMPPServer::removeDomain(XMPPDomain *domain)
{
	mutex.lock();
		domains.erase(domain->hostname());
	mutex.unlock();
}

/**
* Добавить виртуальный хост
*
* thread-safe
*/
void XMPPServer::addHost(const std::string &name, VirtualHostConfig config)
{
	addDomain(new VirtualHost(this, name, config));
}

/**
* Роутер исходящих станз (thread-safe)
*
* Роутер передает станзу нужному потоку.
* Если поток локальный, то просто перекидывает сообщение в него.
* Если внешний (s2s), то пытается установить соединение с сервером и передать ему станзу
*
* @note Данная функция отвечает только за маршрутизацию, она не сохраняет офлайновые сообщения:
*   если адресат локальный, но в offline, то routeStanza() вернет FALSE и вызывающая
*   сторона должна сама сохранить офлайновое сообщение.
*
* @param host домент в который надо направить станзу
* @param stanza станза
* @return TRUE - станза была отправлена, FALSE - станзу отправить не удалось
*/
bool XMPPServer::routeStanza(const std::string &host, Stanza stanza)
{
	XMPPDomain *domain = getHostByName(host);
	if ( domain )
	{ // известный домен
		return domain->routeStanza(stanza);
	}
	else
	{ // неизвестный домен
		return s2s->routeStanza(host, stanza);
	}
}
