#ifndef MAWAR_S2SLISTENER_H
#define MAWAR_S2SLISTENER_H

#include <nanosoft/asyncserver.h>
#include <nanosoft/mutex.h>
#include <nanosoft/asyncdns.h>
#include <stanza.h>
#include <string>
#include <map>

class XMPPServerInput;

/**
* Класс сокета слушающего S2S
*
* И по совместительсвую ИО менеджера s2s :-D
*/
class S2SListener: public AsyncServer
{
protected:
	/**
	* Mutex для thread-safe доступа к общим данным
	*/
	nanosoft::Mutex mutex;
	
	/**
	* Список входящиз s2s соединений (s2s-input)
	*/
	typedef std::map<std::string, XMPPServerInput *> inputs_t;
	
	/**
	* Список авторизуемых виртуальных хостов
	*/
	inputs_t inputs;
	
	/**
	* Обработчик события: подключение s2s
	*
	* thread-safe
	*/
	virtual void onAccept();
	
	/**
	* Сигнал завершения работы
	*
	* Объект должен закрыть файловый дескриптор
	* и освободить все занимаемые ресурсы
	*/
	virtual void onTerminate();
	
public:
	/**
	* Ссылка на объект сервера
	*/
	class XMPPServer *server;
	
	/**
	* Конструктор сервера
	*/
	S2SListener(class XMPPServer *srv);
	
	/**
	* Деструктор сервера
	*/
	~S2SListener();
	
	/**
	* Найти s2s-input по ID
	*
	* thread-safe
	*/
	XMPPServerInput *getInput(const std::string &id);
	
	/**
	* Удалить s2s-input
	*
	* thread-safe
	*/
	void removeInput(XMPPServerInput *input);
};

#endif // MAWAR_S2SLISTENER_H
