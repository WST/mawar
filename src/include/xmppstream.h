#ifndef MAWAR_XMPPSTREAM_H
#define MAWAR_XMPPSTREAM_H

#include <nanosoft/asyncxmlstream.h>
#include <nanosoft/xmlwriter.h>

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
	* Глубина обрабатываемого тега
	*/
	int depth;
	
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
};

#endif // MAWAR_XMPPSTREAM_H
