#ifndef MAWAR_XMPPSTREAM_H
#define MAWAR_XMPPSTREAM_H

#include <nanosoft/asyncxmlstream.h>
#include <nanosoft/xmlwriter.h>
#include <nanosoft/gsaslserver.h>
#include <xml_types.h>
#include <tagbuilder.h>
#include <xml_tag.h>
#include <stanza.h>
#include <presence.h>

/**
* Класс XMPP-поток
*/
class XMPPStream: public AsyncXMLStream, private nanosoft::XMLWriter
{
private:
	/**
	* Ссылка на сервер
	*/
	class XMPPServer *server;
	
	/**
	* Виртуальный хост
	*/
	class VirtualHost *vhost;
	
	/**
	* Построитель дерева тегов
	*/
	ATTagBuilder *builder;
	
	/**
	* Глубина обрабатываемого тега
	*/
	int depth;
	
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
	
protected:
	/**
	* Запись XML
	*/
	virtual void onWriteXML(const char *data, size_t len);
	
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
	XMPPStream(XMPPServer *srv, int sock);
	
	/**
	* Деструктор потока
	*/
	~XMPPStream();
	
	/**
	* JID потока
	*/
	JID jid();

	/**
	* Приоритет ресурса
	*/
	ClientPresence presence();
	
	/**
	* Событие готовности к записи
	*
	* Вызывается, когда в поток готов принять
	* данные для записи без блокировки
	*/
	virtual void onWrite();
	
	/**
	* Событие закрытие соединения
	*
	* Вызывается если peer закрыл соединение
	*/
	virtual void onShutdown();
	
	/**
	* Обработчик открытия тега
	*/
	virtual void onStartElement(const std::string &name, const attributtes_t &attributes);
	
	/**
	* Обработчик символьных данных
	*/
	virtual void onCharacterData(const std::string &cdata);
	
	/**
	* Обработчик закрытия тега
	*/
	virtual void onEndElement(const std::string &name);
	
	/**
	* Обработчик станзы
	*/
	virtual void onStanza(Stanza *stanza);
	
	/**
	* Обработчик авторизации
	*/
	virtual void onAuthStanza(Stanza *stanza);
	
	/**
	* Обработка этапа авторизации SASL
	*/
	virtual void onSASLStep(const std::string &input);
	
	/**
	* Обработчик авторизации: ответ клиента
	*/
	virtual void onResponseStanza(Stanza *stanza);
	
	/**
	* Обработчик iq-станзы
	*/
	virtual void onIqStanza(Stanza *stanza);
	
	/**
	* Обработчик message-станзы
	*/
	virtual void onMessageStanza(Stanza *stanza);
	
	/**
	* Обработчик presence-станзы
	*/
	virtual void onPresenceStanza(Stanza *stanza);
	
	void sendTag(ATXmlTag * tag);
	bool sendStanza(Stanza * stanza);
	
	/**
	* Завершить сессию
	*/
	void terminate();
};

#endif // MAWAR_XMPPSTREAM_H
