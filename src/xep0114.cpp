#include <xep0114.h>
#include <xmppstream.h>
#include <xmppserver.h>
#include <virtualhost.h>
#include <db.h>
#include <tagbuilder.h>
#include <nanosoft/base64.h>

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
	state(init)
{
}

/**
* Деструктор потока
*/
XEP0114::~XEP0114()
{
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void XEP0114::onTerminate()
{
	fprintf(stderr, "#%d: [XEP0114: %d] onTerminate\n", getWorkerId(), fd);
	
	mutex.lock();
		endElement("stream:stream");
		flush();
		shutdown(WRITE);
	mutex.unlock();
}

/**
* Обработчик станз
*/
void XEP0114::onStanza(Stanza stanza)
{
	fprintf(stderr, "#%d stanza: %s\n", getWorkerId(), stanza->name().c_str());
	if ( state == init ) {
		if ( stanza->name() == "handshake" ) {
			// TODO авторизация
			state = authorized;
			
			XMPPStream::server->addDomain(this);
			startElement("handshake");
			endElement("handshake");
			flush();
			
			return;
		} else {
			Stanza error = Stanza::streamError("not-authorized");
			sendStanza(error);
			delete error;
			flush();
			terminate();
			return;
		}
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
	fprintf(stderr, "#%d new stream to %s\n", getWorkerId(), attributes.find("to")->second.c_str());
	initXML();
	
	attributes_t::const_iterator to = attributes.find("to");
	XMPPDomain::name = to != attributes.end() ? to->second : string();
	
	startElement("stream:stream");
	setAttribute("xmlns", "jabber:component:accept");
	setAttribute("xmlns:stream", "http://etherx.jabber.org/streams");
	setAttribute("from", XMPPDomain::name);
	setAttribute("id", "123456"); // Требования к id — непредсказуемость и уникальность
	
	
	XMPPDomain *domain = XMPPStream::server->getHostByName(XMPPDomain::name);
	if ( domain ) {
		// конфликт
		Stanza error = Stanza::streamError("conflict");
		sendStanza(error);
		delete error;
		flush();
		terminate();
		return;
	}
	
	flush();
}

/**
* Событие: конец потока
*/
void XEP0114::onEndStream()
{
	fprintf(stderr, "#%d: [XEP0114: %d] end of stream\n", getWorkerId(), fd);
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
	cerr << "TODO XEP0114::routeStanza()\n";
	sendStanza(stanza);
}
