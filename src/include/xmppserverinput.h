#ifndef MAWAR_SERVERINPUT_H
#define MAWAR_SERVERINPUT_H

#include <xmppstream.h>
#include <utility>
#include <map>
#include <nanosoft/mutex.h>

/**
* Класс потока ввода (s2s input)
*
* Поток ввода авторизуется только для приёма станз
* от удалённого хоста, для отправки станз удалённому хосту
* используется другой тип потока - XMPPServerOutput (s2s output).
*
* Данный класс управляет подключением к внешнему домену
* и списком авторизованных виртуальных хостов.
* Для каждого авторизованного виртуального хоста
* заводиться свой псевдо-поток.
*
* XMPPServerOutput - клиентский сокет, исходящие станзы
* XMPPServerInput - серверный сокет, входящие станзы
*/
class XMPPServerInput: public XMPPStream
{
protected:
	/**
	* Mutex для thread-safe доступа к общим данным
	*/
	nanosoft::Mutex mutex;
	
	/**
	* Удаленный хост от которого мы принимаем станзы
	*/
	std::string remote_host;
	
	/**
	* ID сеанса
	*/
	std::string id;
	
	/**
	* Описание виртуального хоста
	*/
	struct vhost_t
	{
		/**
		* Виртуальный хост автризован для передачи данных
		*/
		bool authorized;
	};
	
	/**
	* Ключь s2s-input коннекта (from, to)
	*/
	typedef std::pair<std::string, std::string> vhostkey_t;
	
	/**
	* Список авторизуемых виртуальных хостов
	*/
	typedef std::map<vhostkey_t, vhost_t *> vhosts_t;
	
	/**
	* Список авторизуемых виртуальных хостов
	*/
	vhosts_t vhosts;
	
	/**
	* Событие: начало потока
	*/
	virtual void onStartStream(const std::string &name, const attributes_t &attributes);
	
	/**
	* Событие: конец потока
	*/
	virtual void onEndStream();
	
	/**
	* Пир (peer) закрыл поток.
	*
	* Мы уже ничего не можем отправить в ответ,
	* можем только корректно закрыть соединение с нашей стороны.
	*/
	virtual void onPeerDown();
	
	/**
	* Сигнал завершения работы
	*
	* Сервер решил закрыть соединение, здесь ещё есть время
	* корректно попрощаться с пиром (peer).
	*/
	virtual void onTerminate();
	
public:
	/**
	* Конструктор потока
	*/
	XMPPServerInput(XMPPServer *srv, int sock);
	
	/**
	* Деструктор потока
	*/
	~XMPPServerInput();
	
	/**
	* Удаленный хост от которого мы принимаем станзы
	*/
	std::string remoteHost() const { return remote_host; }
	
	/**
	* Вернуть ID s2s-input потока
	*/
	std::string getId() const { return id; }
	
	/**
	* Обработчик станзы
	*/
	virtual void onStanza(Stanza stanza);
	
	/**
	* Обработка <db:verify>
	*/
	void onDBVerifyStanza(Stanza stanza);
	
	/**
	* Обработка <db:result>
	*/
	void onDBResultStanza(Stanza stanza);
};

#endif // MAWAR_SERVERINPUT_H
