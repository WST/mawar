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
* Событие закрытие соединения
*
* Вызывается если peer закрыл соединение
*/
void XEP0114::onShutdown()
{
	cerr << "[XEP0114]: peer shutdown connection" << endl;
	if ( state != terminating ) {
		XMLWriter::flush();
	}
	XMPPStream::server->daemon->removeObject(this);
	if ( state == authorized ) XMPPStream::server->removeDomain(this);
	shutdown(READ | WRITE);
	delete this;
	cerr << "[XEP0114]: onShutdown leave" << endl;
}

/**
* Завершить сессию
*
* thread-safe
*/
void XEP0114::terminate()
{
	cerr << "[XEP0114]: terminating connection..." << endl;
	
	switch ( state )
	{
	case terminating:
		return;
	case authorized:
		//server->onOffline(this);
		break;
	}
	
	mutex.lock();
		if ( state != terminating ) {
			state = terminating;
			XMPPStream::server->removeDomain(this);
			endElement("stream:stream");
			flush();
			shutdown(WRITE);
		}
	mutex.unlock();
}

/**
* Сигнал завершения работы
*
* Объект должен закрыть файловый дескриптор
* и освободить все занимаемые ресурсы
*/
void XEP0114::onTerminate()
{
	terminate();
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
	cerr << "session closed" << endl;
	endElement("stream:stream");
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
