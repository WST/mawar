#ifndef MAWAR_XMPPPROXY_H
#define MAWAR_XMPPPROXY_H

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
class XMPPProxy: public AsyncServer
{
private:
	/**
	* IP адрес проксируемого сервера
	*/
	const char *server_ip;
	
	/**
	* Порт проксируемого сервера
	*/
	int server_port;
	
	/**
	* Число записей в белом списке
	*/
	int whitecount;
	
	/**
	* Белый список IP адресов которые не надо ограничивать по скорости
	*/
	char whitelist[100][20];
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
	* Ссылка на демона
	*/
	NetDaemon *daemon;
	
	/**
	* Конструктор сервера
	* @param d ссылка на демона
	* @param ip IP адрес проксируемого сервера
	* @param port порт проксируемого сервера
	*/
	XMPPProxy(NetDaemon *d, const char *ip, int port);
	
	/**
	* Деструктор сервера
	*/
	~XMPPProxy();
	
	/**
	* Сигнал завершения
	*/
	void onSigTerm();
	
	/**
	* Сигнал HUP
	*/
	void onSigHup();
	
	/**
	* Перезагрузить белый список из файла
	*/
	void reloadWhiteList(const char *path);
};

#endif // MAWAR_XMPPPROXY_H
