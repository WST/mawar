#ifndef MAWAR_XMPPSERVER_H
#define MAWAR_XMPPSERVER_H

#include <nanosoft/netdaemon.h>
#include <nanosoft/asyncserver.h>
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
	* Сигнал завершения работы
	*
	* Объект должен закрыть файловый дескриптор
	* и освободить все занимаемые ресурсы
	*/
	virtual void onTerminate();
	
public:
	/**
	* TODO декостылизация...
	*/
	typedef std::vector<std::string> users_t;
	
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
	* Сигнал завершения
	*/
	void onSigTerm();
	
	/**
	* Сигнал HUP
	*/
	void onSigHup();
	
	/**
	* Вернуть список всех пользователей сервера
	* TODO временый костыль для временного ростера
	*/
	users_t getUserList();
	
	/**
	* Вернуть виртуальный хост по имени
	* @param name имя искомого хоста
	* @return виртуальный хост или 0 если такого хоста нет
	*/
	VirtualHost* getHostByName(const std::string &name);
	
	/**
	* Добавить виртуальный хост
	*/
	void addHost(const std::string &name, VirtualHostConfig config);
	
private:
	/**
	* Список виртуальных хостов
	*/
	vhosts_t vhosts;
};

#endif // MAWAR_XMPPSERVER_H
