#ifndef MAWAR_XMPPCLIENT_H
#define MAWAR_XMPPCLIENT_H

#include <xmppstream.h>
#include <nanosoft/gsaslserver.h>
#include <xml_types.h>
#include <tagbuilder.h>
#include <xml_tag.h>
#include <stanza.h>
#include <presence.h>

/**
* Класс XMPP-поток (c2s)
*/
class XMPPClient: public XMPPStream
{
protected:
	/**
	* Виртуальный хост
	*/
	class VirtualHost *vhost;
	
	/**
	* Сеанс авторизации SASL
	*/
	GSASLSession *sasl;
	
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
	
	JID client_jid;
	
	ClientPresence client_presence;
	
	/**
	* TRUE - Initial presense уже отправлен
	* FLASE - Initial presense ещё не отправлен
	*/
	bool initialPresenceSent;
	
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
	XMPPClient(XMPPServer *srv, int sock);
	
	/**
	* Деструктор потока
	*/
	~XMPPClient();
	
	/**
	* JID потока
	*/
	JID jid() const;

	/**
	* Приоритет ресурса
	*/
	ClientPresence presence();
	
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
	* Обработчик авторизации
	*/
	virtual void onAuthStanza(Stanza stanza);
	
	/**
	* Обработка этапа авторизации SASL
	*/
	virtual void onSASLStep(const std::string &input);
	
	/**
	* Обработчик авторизации: ответ клиента
	*/
	virtual void onResponseStanza(Stanza stanza);
	
	/**
	* Обработчик iq-станзы
	*/
	virtual void onIqStanza(Stanza stanza);
	
	/**
	* Обработчик message-станзы
	*/
	virtual void onMessageStanza(Stanza stanza);
	
	/**
	* Обработчик presence-станзы
	*/
	virtual void onPresenceStanza(Stanza stanza);
	
	/**
	* Завершить сессию
	*
	* thread-safe
	*/
	void terminate();
};

#endif // MAWAR_XMPPCLIENT_H
