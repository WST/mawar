#ifndef MAWAR_XMPPSTREAM_H
#define MAWAR_XMPPSTREAM_H

#include <nanosoft/asyncxmlstream.h>
#include <nanosoft/xmlwriter.h>
#include <nanosoft/mutex.h>
#include <xml-types.h>
#include <tagbuilder.h>
#include <xml-tag.h>
#include <stanza.h>

/**
* Класс XMPP-поток
*/
class XMPPStream: public AsyncXMLStream, protected nanosoft::XMLWriter
{
protected:
	/**
	* Mutex для сихронизации
	*/
	nanosoft::Mutex mutex;
	
	/**
	* Ссылка на сервер
	*/
	class XMPPServer *server;
	
	/**
	* Версия XMPP заявленая пиром
	*/
	int ver_major;
	
	/**
	* Версия XMPP заявленая пиром
	*/
	int ver_minor;
	
	/**
	* Построитель дерева тегов
	*/
	TagBuilder builder;
	
	/**
	* Глубина обрабатываемого тега
	*/
	int depth;
	
	/**
	* Чисто тегов выделенно перед обработкой станзы
	*/
	static unsigned tags_created_before_stanza;
	
	/**
	* Число тегов уничтожено перед обработкой станзы
	*/
	static unsigned tags_destroyed_before_stanza;
	
	/**
	* Число тегов выделеных для обработки станзы
	*/
	static unsigned tags_created_for_stanza;
	
	/**
	* Число тегов удаленных при обработки станзы
	*/
	static unsigned tags_destroyed_for_stanza;
	
	/**
	* Максимальное число тегов выделенных для обработки 1 станзы
	*/
	static unsigned tags_max_created_for_stanza;
	
	/**
	* Число утеряных тегов при обработке станзы
	*/
	static unsigned tags_leak_for_stanza;
	
	/**
	* Всего утеряных тегов
	*/
	static unsigned tags_leak;
	
	/**
	* Число обработанных станз
	*/
	static unsigned stanza_count;
	
	/**
	* Вернуть маску ожидаемых событий
	*/
	virtual uint32_t getEventsMask();
	
	/**
	* Запись XML
	*/
	virtual void onWriteXML(const char *data, size_t len);
	
	/**
	* Открытие потока
	*/
	void startStream(const std::string &name, const attributtes_t &attributes);
	
	/**
	* Событие: начало потока
	*/
	virtual void onStartStream(const std::string &name, const attributes_t &attributes) = 0;
	
	/**
	* Событие: конец потока
	*/
	virtual void onEndStream() = 0;
	
	/**
	* Обработчик ошибок парсера
	*/
	virtual void onParseError(const char *message);
	
	/**
	* Вызывается после получения, но до обработки станзы
	*/
	void onBeforeStanza();
	
	/**
	* Вызывается после завершение обработки станзы
	*/
	void onAfterStanza();
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
	* Число обработанных станз
	*/
	static unsigned getStanzaCount() { return stanza_count; }
	
	/**
	* Вернуть число утеряных тегов
	*/
	static unsigned getTagsLeak() { return tags_leak; }
	
	/**
	* Вернуть максимальное число тегов созданных для обработки 1 станзы
	*/
	static unsigned getMaxTagsPerStanza() { return tags_max_created_for_stanza; }
	
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
	virtual void onStanza(Stanza stanza) = 0;
	
	/**
	* Отправить станзу в поток (thread-safe)
	* @param stanza станза
	* @return TRUE - станза отправлена (или буферизована для отправки), FALSE что-то не получилось
	*/
	bool sendStanza(Stanza stanza);
};

#endif // MAWAR_XMPPSTREAM_H
