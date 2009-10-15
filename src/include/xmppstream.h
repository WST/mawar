#ifndef MAWAR_XMPPSTREAM_H
#define MAWAR_XMPPSTREAM_H

#include <nanosoft/asyncxmlstream.h>
#include <nanosoft/xmlwriter.h>
#include <nanosoft/gsaslserver.h>
#include <xml_types.h>
#include <tagbuilder.h>
#include <xml_tag.h>
#include <stanza.h>

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
	
	enum {init, authorized} state;
	
	/**
	* Логин авторизованного пользователя
	* TODO заменить на JID
	*/
	std::string username;
	
	/**
	* Хост
	* TODO заменить на JID
	*/
	std::string host;
	
	/**
	* Ресурс
	* TODO заменить на JID
	*/
	std::string resource;
	
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
	* TODO докостылизация в класс JID
	* Наверное лучше просто добапить публичное поле JID jid
	*/
	std::string jid();
	
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
};

#endif // MAWAR_XMPPSTREAM_H
