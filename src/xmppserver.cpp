
#include <xmppserver.h>
#include <xmppclient.h>
#include <xmppdomain.h>
#include <xmppserveroutput.h>
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
	fprintf(stderr, "XMPPServer[%d] deleting\n", getFd());
	
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
	fprintf(stderr, "XMPPServer[%d]: onTerminate\n", getFd());
	daemon->removeObject(this);
}

/**
* Сигнал завершения
*/
void XMPPServer::onSigTerm()
{
	fprintf(stderr, "XMPPServer[%d]: SIGTERM\n", getFd());
	daemon->terminate();
}

/**
* Сигнал HUP
*/
void XMPPServer::onSigHup()
{
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
* Проверить является ли указанный хост нашим
*/
bool XMPPServer::isOurHost(const std::string &hostname)
{
	XMPPDomain *domain = getHostByName(hostname);
	return domain && ! dynamic_cast<XMPPServerOutput*>(domain);
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
		printf("removeDomain(%s) = %lu\n", domain->hostname().c_str(), domains.erase(domain->hostname()));
	mutex.unlock();
}

/**
* Добавить виртуальный хост
*
* thread-safe
*/
void XMPPServer::addHost(const std::string &name, XmlTag *config)
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
	if ( host == "" )
	{
		printf("routeStanza(unknown host): %s\n", stanza->asString().c_str());
		return false;
	}
	mutex.lock();
		domains_t::const_iterator iter = domains.find(host);
		XMPPDomain *domain = iter != domains.end() ? iter->second : 0;
		if ( ! domain )
		{
			domain = new XMPPServerOutput(this, host.c_str());
			domains[domain->hostname()] = domain;
		}
	mutex.unlock();
	
	return domain->routeStanza(stanza);
}

/**
* Роутер исходящих станз (thread-safe)
*
* Упрощенный врапер для routeStanza(host, stanza), хост извлекается
* из атрибута to
*/
bool XMPPServer::routeStanza(Stanza stanza)
{
	return routeStanza(stanza.to().hostname(), stanza);
}

