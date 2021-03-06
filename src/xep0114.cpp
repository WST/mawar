#include <xep0114.h>
#include <xmppstream.h>
#include <xmppserver.h>
#include <virtualhost.h>
#include <db.h>
#include <tagbuilder.h>
#include <nanosoft/base64.h>
#include <gsasl.h>
#include <functions.h>
#include <configfile.h>

// for debug only
#include <string>
#include <iostream>
#include <stdio.h>

using namespace std;
using namespace nanosoft;

/**
* Конструктор потока
*/
XEP0114::XEP0114(XMPPServer *srv, int sock):
	XMPPStream(srv, sock), XMPPDomain(srv, ""),
	state(init), id(getUniqueId())
{
	lock();
}

/**
* Деструктор потока
*/
XEP0114::~XEP0114()
{
	printf("XEP0114::~XEP0114()\n");
	XMPPDomain::server->removeDomain(this);
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void XEP0114::onTerminate()
{
	fprintf(stderr, "XEP0114[%d]: onTerminate\n", getFd());
	
	mutex.lock();
		endElement("stream:stream");
		flush();
		XMPPDomain::server->daemon->removeObject(this);
	mutex.unlock();
	
	release();
}

/**
* Обработчик станз
*/
void XEP0114::onStanza(Stanza stanza)
{
	fprintf(stdout, "XEP0114[%d]: onStanza: %s\n", getFd(), stanza->name().c_str());
	if ( state == init )
	{
		Stanza reply;
		
		if ( config && stanza->name() == "handshake" )
		{
			string hash = sha1( id + config["secret"]->getCharacterData() );
			if ( hash == stanza->getCharacterData() )
			{
				state = authorized;
				reply = new XmlTag("handshake");
				sendStanza(reply);
				delete reply;
				XMPPStream::server->addDomain(this);
				return;
			}
		}
		
		reply = new XmlTag("not-authorized");
		sendStanza(reply);
		delete reply;
		terminate();
		return;
	} else {
		// TODO проверка from
		XMPPStream::server->routeStanza(stanza.to().hostname(), stanza);
	}
}

/**
* Событие: начало потока
*/
void XEP0114::onStartStream(const std::string &name, const attributes_t &attributes)
{
	fprintf(stderr, "XEP0114[%d]: new stream to %s\n", getFd(), attributes.find("to")->second.c_str());
	
	attributes_t::const_iterator to = attributes.find("to");
	XMPPDomain::name = to != attributes.end() ? to->second : string();
	
	initXML();
	startElement("stream:stream");
	setAttribute("xmlns", "jabber:component:accept");
	setAttribute("xmlns:stream", "http://etherx.jabber.org/streams");
	setAttribute("from", XMPPDomain::name);
	setAttribute("id", id);
	flush();
	
	XMPPDomain *domain = XMPPStream::server->getHostByName(XMPPDomain::name);
	if ( domain )
	{
		// конфликт
		Stanza error = Stanza::streamError("conflict");
		sendStanza(error);
		delete error;
		terminate();
		return;
	}
	
	ConfigFile *srvconf = XMPPDomain::server->config;
	
	for(TagHelper item = srvconf->firstExternal(); item; item = srvconf->nextExternal(item))
	{
		if ( item->getAttribute("name") == XMPPDomain::name )
		{
			config = item;
			return;
		}
	}
	
	Stanza error = Stanza::streamError("host-unknown");
	sendStanza(error);
	delete error;
	terminate();
	return;
}

/**
* Событие: конец потока
*/
void XEP0114::onEndStream()
{
	fprintf(stderr, "XEP0114[%d]: end of stream\n", getFd());
	terminate();
}

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
bool XEP0114::routeStanza(Stanza stanza)
{
	return sendStanza(stanza);
}
