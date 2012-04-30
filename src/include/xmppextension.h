#ifndef MAWAR_XMPPEXTENSION_H
#define MAWAR_XMPPEXTENSION_H

#include <nanosoft/asyncxmlstream.h>
#include <nanosoft/xmlwriter.h>
#include <nanosoft/mutex.h>
#include <nanosoft/object.h>
#include <xml_types.h>
#include <tagbuilder.h>
#include <xml_tag.h>
#include <stanza.h>

class XMPPServer;
class XMPPExtensionInput;
class XMPPExtensionOutput;

/**
* Класс XMPP-расширение
*/
class XMPPExtension: public nanosoft::Object
{
protected:
	int ctime;
	int active;
	nanosoft::ptr<XMPPExtensionInput> ext_in;
	nanosoft::ptr<XMPPExtensionOutput> ext_out;
	std::string urn;
	std::string fname;
	std::string fpath;
public:
	/**
	* Ссылка на сервер
	*/
	XMPPServer *server;
	
	/**
	* Конструктор модуля-расширения
	*/
	XMPPExtension(XMPPServer *srv, const char *urn, const char *fname);
	
	/**
	* Деструктор модуля-расширения
	*/
	virtual ~XMPPExtension();
	
	/**
	* Открыть модуль-расширение
	*/
	bool open();
	
	/**
	* Закрыть модуль-расширение
	*/
	void terminate();
	
	/**
	* Обработать станзу
	* @param stanza станза
	*/
	void handleStanza(Stanza stanza);
};

#endif // MAWAR_XMPPEXTENSION_H
