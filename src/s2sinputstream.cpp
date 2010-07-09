
#include <s2sinputstream.h>
#include <xmppserveroutput.h>
#include <virtualhost.h>
#include <functions.h>
#include <nanosoft/asyncdns.h>
#include <iostream>

using namespace std;

/**
* Конструктор потока
*/
S2SInputStream::S2SInputStream(XMPPServer *srv, int sock): XMPPStream(srv, sock)
{
}

/**
* Деструктор потока
*/
S2SInputStream::~S2SInputStream()
{
}

/**
* Событие: начало потока
*/
void S2SInputStream::onStartStream(const std::string &name, const attributes_t &attributes)
{
	printf("s2s-input: new stream, sock: %d\n", fd);
	initXML();
	startElement("stream:stream");
	setAttribute("xmlns:stream", "http://etherx.jabber.org/streams");
	setAttribute("xmlns", "jabber:server");
	setAttribute("xmlns:db", "jabber:server:dialback");
	setAttribute("id", id = getUniqueId());
	setAttribute("xml:lang", "en");
	flush();
}

/**
* Событие: конец потока
*/
void S2SInputStream::onEndStream()
{
	fprintf(stderr, "s2s-input(%s): end of stream\n", remote_host.c_str());
	terminate();
}

/**
* Обработчик станзы
*/
void S2SInputStream::onStanza(Stanza stanza)
{
	printf("s2s-input(%s from %s) stanza: %s\n", stanza.to().hostname().c_str(), stanza.from().hostname().c_str(), stanza->name().c_str());
	if ( stanza->name() == "verify" ) onDBVerifyStanza(stanza);
	else if ( stanza->name() == "result" ) onDBResultStanza(stanza);
	else if ( state != authorized )
	{
		fprintf(stderr, "#%d unexpected s2s-input stanza: %s\n", getWorkerId(), stanza->name().c_str());
		Stanza error = Stanza::streamError("not-authoized");
		sendStanza(error);
		delete error;
		terminate();
	}
	else if ( stanza.from().hostname() != remote_host )
	{
		fprintf(stderr, "#%d [s2s-input: %s] invalid from: %s\n", getWorkerId(), remote_host.c_str(), stanza->getAttribute("from").c_str());
		Stanza error = Stanza::streamError("improper-addressing");
		sendStanza(error);
		delete error;
		terminate();
	}
	else
	{
		// доставить станзу по назначению
		XMPPDomain *vhost = server->getHostByName(stanza.to().hostname());
		if ( ! vhost )
		{
			fprintf(stderr, "#%d [s2s-input: %s] invalid to: %s\n", getWorkerId(), remote_host.c_str(), stanza->getAttribute("to").c_str());
			Stanza error = Stanza::streamError("improper-addressing");
			sendStanza(error);
			delete error;
			terminate();
		}
		vhost->routeStanza(stanza);
	}
}

/**
* RFC 3920 (8.3.8)
*/
void S2SInputStream::onDBVerifyStanza(Stanza stanza)
{
	// Шаг 1. проверка: "to" должен быть нашим виртуальным хостом
	string to = stanza->getAttribute("to");
	XMPPDomain *host = server->getHostByName(to);
	if ( ! dynamic_cast<VirtualHost*>(host) )
	{
		Stanza stanza = Stanza::streamError("host-unknown");
		sendStanza(stanza);
		delete stanza;
		terminate();
		return;
	}
	
	// Шаг 2. проверка: "from"
	// TODO
	string from = stanza->getAttribute("from");
	if ( dynamic_cast<VirtualHost*>(server->getHostByName(from)) )
	{
		Stanza stanza = Stanza::streamError("invalid-from");
		sendStanza(stanza);
		delete stanza;
		terminate();
		return;
	}
	
	// Шаг 3. проверка ключа
	// TODO
	if ( stanza->getCharacterData() == "key" )
	{
		Stanza result = new ATXmlTag("db:verify");
		result->setAttribute("to", from);
		result->setAttribute("from", to);
		result->setAttribute("type", "valid");
		sendStanza(result);
		delete result;
	}
	else
	{
		Stanza result = new ATXmlTag("db:verify");
		result->setAttribute("to", from);
		result->setAttribute("from", to);
		result->setAttribute("type", "invalid");
		sendStanza(result);
		delete result;
	}
}

/**
* RFC 3920 (8.3.4)
*/
void S2SInputStream::onDBResultStanza(Stanza stanza)
{
	// Шаг 1. проверка: "to" должен быть нашим виртуальным хостом
	string to = stanza->getAttribute("to");
	XMPPDomain *host = server->getHostByName(to);
	if ( ! dynamic_cast<VirtualHost*>(host) )
	{
		Stanza stanza = Stanza::streamError("host-unknown");
		sendStanza(stanza);
		delete stanza;
		terminate();
		return;
	}
	
	// Шаг 2. проверка "from"
	//
	// RFC 3920 не запрещает делать повторные коннекты (8.3.4).
	//
	// До завершения авторизации нужно поддерживать старое соединение,
	// пока не авторизуется новое. Но можно блокировать повторные
	// коннекты с ошибкой <not-authorized />, что мы и делаем.
	//
	// NOTE: В любом случае, логично блокировать попытки представиться
	// нашим хостом - мы сами к себе никогда не коннектимся.
	// Так что, если будете открывать повторные коннекты, то не забудьте
	// блокировать попытки коннекта к самим себе.
	// (c) shade
	string from = stanza->getAttribute("from");
	host = server->getHostByName(from);
	if ( dynamic_cast<VirtualHost*>(host) )
	{
		Stanza stanza = Stanza::streamError("not-authorized");
		sendStanza(stanza);
		delete stanza;
		terminate();
		return;
	}
	remote_host = from;
	
	// Шаг 3. шлем запрос на проверку ключа
	Stanza verify = new ATXmlTag("db:verify");
	verify->setAttribute("from", to);
	verify->setAttribute("to", from);
	verify->setAttribute("id", id);
	verify += stanza->getCharacterData();
	server->routeStanza(verify);
	delete verify;
	
	// Шаг X. костыль - ответить сразу "authorized"
	state = authorized;
	Stanza result = new ATXmlTag("db:result");
	result->setAttribute("to", from);
	result->setAttribute("from", to);
	result->setAttribute("type", "valid");
	sendStanza(result);
	delete result;
}

/**
* Пир (peer) закрыл поток.
*
* Мы уже ничего не можем отправить в ответ,
* можем только корректно закрыть соединение с нашей стороны.
*/
void S2SInputStream::onPeerDown()
{
	printf("[S2SInputStream: %d] onPeerDown\n", fd);
	terminate();
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void S2SInputStream::onTerminate()
{
	printf("s2s-input(%s): onTerminate\n", remote_host.c_str());
	
	// if ( state == authorized ) server->onOffline(this);
	
	mutex.lock();
		endElement("stream:stream");
		flush();
		server->daemon->addObject(this);
		close();
	mutex.unlock();
}
