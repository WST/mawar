#ifndef MAWAR_XMPPSERVER_H
#define MAWAR_XMPPSERVER_H

#include <nanosoft/netdaemon.h>
#include <nanosoft/asyncserver.h>
#include <nanosoft/mutex.h>
#include <configfile.h>
#include <string>

class XMPPStream;
class VirtualHost;

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
	* Сигнал завершения работы
	*
	* Объект должен закрыть файловый дескриптор
	* и освободить все занимаемые ресурсы
	*/
	virtual void onTerminate();
	
public:
	/**
	* Список виртуальных хостов
	*/
	typedef std::map<std::string, VirtualHost*> vhosts_t;
	
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
	*
	* thread-safe
	*/
	AsyncObject* onAccept();
	
	/**
	* Сигнал завершения
	*/
	void onSigTerm();
	
	/**
	* Сигнал HUP
	*/
	void onSigHup();
	
	/**
	* Вернуть виртуальный хост по имени
	*
	* thread-safe
	*
	* @param name имя искомого хоста
	* @return виртуальный хост или 0 если такого хоста нет
	*/
	VirtualHost* getHostByName(const std::string &name);
	
	/**
	* Добавить виртуальный хост
	*
	* thread-safe
	*/
	void addHost(const std::string &name, VirtualHostConfig config);
	
private:
	/**
	* Список виртуальных хостов
	*/
	vhosts_t vhosts;
};

#endif // MAWAR_XMPPSERVER_H
