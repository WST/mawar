#ifndef MAWAR_XMPPSERVER_H
#define MAWAR_XMPPSERVER_H

#include <nanosoft/netdaemon.h>
#include <nanosoft/asyncserver.h>
#include <nanosoft/mutex.h>
#include <configfile.h>
#include <jid.h>
#include <stanza.h>
#include <string>

class XMPPStream;
class XMPPDomain;

/**
* Класс XMPP сервера
*/
class XMPPServer: public AsyncServer
{
protected:
	/**
	* Mutex для thread-safe доступа к общим данным
	*/
	nanosoft::Mutex mutex;
	
	/**
	* Принять входящее соединение
	*/
	virtual void onAccept();
	
	/**
	* Сигнал завершения работы
	*/
	virtual void onTerminate();
public:
	/**
	* Список доменов
	*/
	typedef std::map<std::string, XMPPDomain*> domains_t;
	
	/**
	* Ссылка на демона
	*/
	NetDaemon *daemon;
	
	/**
	* Ссылка на асинхронный резолвер DNS
	*/
	nanosoft::ptr<class AsyncDNS> adns;
	
	/**
	* Ссылка на менеджер s2s-соединений
	*/
	nanosoft::ptr<class S2SListener> s2s;
	
	/**
	* Конфигурация сервера
	*/
	ConfigFile *config;
	
	/**
	* Конструктор сервера
	*/
	XMPPServer(NetDaemon *d);
	
	/**
	* Деструктор сервера
	*/
	~XMPPServer();
	
	/**
	* Сигнал завершения
	*/
	void onSigTerm();
	
	/**
	* Сигнал HUP
	*/
	void onSigHup();
	
	/**
	* Вернуть домен по имени
	*
	* thread-safe
	*
	* @param name имя искомого хоста
	* @return домен или 0 если такого хоста нет
	*/
	XMPPDomain* getHostByName(const std::string &name);
	
	/**
	* Проверить является ли указанный хост нашим
	*/
	bool isOurHost(const std::string &hostname);
	
	/**
	* Добавить домен (thread-safe)
	*/
	void addDomain(XMPPDomain *domain);
	
	/**
	* Удалить домен (thread-safe)
	*/
	void removeDomain(XMPPDomain *domain);
	
	/**
	* Добавить виртуальный хост
	*
	* thread-safe
	*/
	void addHost(const std::string &name, XmlTag *config);
	
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
	bool routeStanza(const std::string &host, Stanza stanza);
	
	/**
	* Роутер исходящих станз (thread-safe)
	*
	* Упрощенный врапер для routeStanza(host, stanza), хост извлекается
	* из атрибута to
	*/
	bool routeStanza(Stanza stanza);
private:
	/**
	* Список доменов
	*/
	domains_t domains;
};

#endif // MAWAR_XMPPSERVER_H
