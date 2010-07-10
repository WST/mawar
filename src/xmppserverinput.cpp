
#include <xmppserverinput.h>
#include <xmppserveroutput.h>
#include <s2slistener.h>
#include <virtualhost.h>
#include <functions.h>
#include <stdio.h>

using namespace std;

/**
* Конструктор потока
*/
XMPPServerInput::XMPPServerInput(XMPPServer *srv, int sock): XMPPStream(srv, sock)
{
	lock();
}

/**
* Деструктор потока
*/
XMPPServerInput::~XMPPServerInput()
{
}

/**
* Событие: начало потока
*/
void XMPPServerInput::onStartStream(const std::string &name, const attributes_t &attributes)
{
	printf("s2s-input(%d): new stream\n", fd);
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
void XMPPServerInput::onEndStream()
{
	fprintf(stderr, "s2s-input(%s): end of stream\n", remote_host.c_str());
	terminate();
}

/**
* Обработчик станзы
*/
void XMPPServerInput::onStanza(Stanza stanza)
{
	printf("s2s-input(%s from %s): stanza: %s\n", stanza.to().hostname().c_str(), stanza.from().hostname().c_str(), stanza->name().c_str());
	if ( stanza->name() == "verify" ) onDBVerifyStanza(stanza);
	else if ( stanza->name() == "result" ) onDBResultStanza(stanza);
	else
	{
		// Шаг 1. проверка: "to" должен быть нашим виртуальным хостом
		string to = stanza->getAttribute("to");
		XMPPDomain *host = server->getHostByName(to);
		if ( ! dynamic_cast<VirtualHost*>(host) )
		{
			Stanza stanza = Stanza::streamError("improper-addressing");
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
			Stanza stanza = Stanza::streamError("improper-addressing");
			sendStanza(stanza);
			delete stanza;
			terminate();
			return;
		}
		
		vhostkey_t key(from, to);
		
		mutex.lock();
			vhosts_t::const_iterator iter = vhosts.find(key);
			vhost_t *vhost = iter != vhosts.end() ? iter->second : 0;
		mutex.unlock();
		
		if ( vhost && vhost->authorized ) server->routeStanza(stanza);
		else
		{
			Stanza error = Stanza::streamError("not-authoized");
			sendStanza(error);
			delete error;
			terminate();
		}
	}
}

/**
* RFC 3920 (8.3.8)
*/
void XMPPServerInput::onDBVerifyStanza(Stanza stanza)
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
	std::string hash = sha1(stanza->getAttribute("id") + "key");
	if ( stanza->getCharacterData() == hash )
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
void XMPPServerInput::onDBResultStanza(Stanza stanza)
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
	vhostkey_t key(from, to);
	
	mutex.lock();
		vhosts_t::const_iterator iter = vhosts.find(key);
		vhost_t *vhost = iter != vhosts.end() ? iter->second : 0;
		
		if ( ! vhost )
		{
			vhost = new vhost_t;
			vhost->authorized = false;
			vhosts[key] = vhost;
		}
		
	mutex.unlock();
	
	Stanza verify = new ATXmlTag("db:verify");
	verify->setAttribute("from", to);
	verify->setAttribute("to", from);
	verify->setAttribute("id", id);
	verify += stanza->getCharacterData();
	server->routeStanza(verify);
	delete verify;
}

/**
* Авторизовать поток
*/
void XMPPServerInput::authorize(const std::string &from, const std::string &to, bool authorized)
{
	vhostkey_t key(from, to);
	mutex.lock();
		vhosts_t::const_iterator iter = vhosts.find(key);
		vhost_t *vhost = iter != vhosts.end() ? iter->second : 0;
		if ( vhost ) vhost->authorized = authorized;
	mutex.unlock();
}

/**
* Пир (peer) закрыл поток.
*
* Мы уже ничего не можем отправить в ответ,
* можем только корректно закрыть соединение с нашей стороны.
*/
void XMPPServerInput::onPeerDown()
{
	printf("s2s-input(%d): onPeerDown\n", fd);
	terminate();
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void XMPPServerInput::onTerminate()
{
	printf("s2s-input(%s): onTerminate\n", remote_host.c_str());
	
	mutex.lock();
		endElement("stream:stream");
		flush();
		server->s2s->removeInput(this);
		server->daemon->removeObject(this);
	mutex.unlock();
	
	release();
}
