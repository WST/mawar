#ifndef MAWAR_XEP0114_H
#define MAWAR_XEP0114_H

#include <xmppstream.h>
#include <xmppdomain.h>
#include <stanza.h>
#include <presence.h>

/**
* Класс внешнего компонента (XEP-0114)
*/
class XEP0114: public XMPPStream, public XMPPDomain
{
protected:
	enum state_t {
		/**
		* Начальное состояние - инициализация и авторизация
		*/
		init,
		
		/**
		* Авторизован - авторизовался и готов работать
		*/
		authorized,
		
		/**
		* Завершение - сервер завершает свою работу,
		* соединение готовиться к закрытию, нужно
		* корректно попрощаться с клиентом
		*/
		terminating
	} state;
	
	/**
	* Событие: начало потока
	*/
	virtual void onStartStream(const std::string &name, const attributes_t &attributes);
	
	/**
	* Событие: конец потока
	*/
	virtual void onEndStream();
	
	/**
	* Сигнал завершения работы
	*
	* Объект должен закрыть файловый дескриптор
	* и освободить все занимаемые ресурсы
	*/
	virtual void onTerminate();
	
public:
	/**
	* Конструктор потока
	*/
	XEP0114(XMPPServer *srv, int sock);
	
	/**
	* Деструктор потока
	*/
	~XEP0114();
	
	/**
	* Событие закрытие соединения
	*
	* Вызывается если peer закрыл соединение
	*/
	virtual void onShutdown();
	
	/**
	* Обработчик станзы
	*/
	virtual void onStanza(Stanza stanza);
	
	/**
	* Роутер станз (need thread-safe)
	*
	* Данная функция отвечает только за маршрутизацию станз в данном домене
	*
	* @note Данный метод вызывается из глобального маршрутизатора станз XMPPServer::routeStanza()
	*   вызывать его напрямую из других мест не рекомендуется - используйте XMPPServer::routeStanza()
	*
	* @param stanza станза
	* @return TRUE - станза была отправлена, FALSE - станзу отправить не удалось
	*/
	virtual bool routeStanza(Stanza stanza);
	
	void terminate();
};

#endif // MAWAR_XEP0114_H