
#include <xmppserveroutput.h>
#include <xmppserverinput.h>
#include <s2slistener.h>
#include <xmppserver.h>
#include <xmppdomain.h>
#include <virtualhost.h>
#include <functions.h>
#include <logs.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <attagparser.h>
#include <string>

using namespace std;

/**
* Конструктор s2s-домена
*/
XMPPServerOutput::XMPPServerOutput(XMPPServer *srv, const char *host):
	XMPPDomain(srv, host), XMPPStream(srv, 0)
{
	sprintf(key, "%06X-%06X-%06X-%06X", random() % 0x1000000, random() % 0x1000000, random() % 0x1000000, random() % 0x1000000);
	lock();
	
	// Резолвим DNS записи сервера
	state = RESOLVING;
	XMPPDomain::server->adns->srv(host, "xmpp-server", "tcp", on_s2s_output_xmpp_server, this);
}

/**
* Деструктор потока
*/
XMPPServerOutput::~XMPPServerOutput()
{
	printf("%s s2s-output(%s) delete\n", logtime().c_str(), hostname().c_str());
	
	for(vhosts_t::iterator iter = vhosts.begin(); iter != vhosts.end(); ++iter)
	{
		delete iter->second;
	}
	vhosts.clear();
}

/**
* Обработчик готовности подключения
*/
void XMPPServerOutput::onConnect()
{
	mutex.lock();
	printf("%s s2s-output(%s): connected\n", logtime().c_str(), hostname().c_str());
	
	// поздоровайся с дядей
	initXML();
	startElement("stream:stream");
	setAttribute("xmlns:stream", "http://etherx.jabber.org/streams");
	setAttribute("xmlns", "jabber:server");
	setAttribute("xmlns:db", "jabber:server:dialback");
	setAttribute("xml:lang", "en");
	flush();
	
	for(vhosts_t::iterator iter = vhosts.begin(); iter != vhosts.end(); ++iter)
	{
		vhost_t *vhost = iter->second;
		
		sendDBRequest(iter->first, hostname());
		
		printf("%s s2s-output(%s): flush connected to %s\n", logtime().c_str(), hostname().c_str(), iter->first.c_str());
		for(buffer_t::iterator bi = vhost->connbuffer.begin(); bi != vhost->connbuffer.end(); bi++)
		{
			Stanza stanza = parse_xml_string(*bi);
			sendStanza(stanza);
			delete stanza;
		}
	}
	
	mutex.unlock();
}

/**
* Резолвер s2s хоста, запись A (IPv4)
*/
void XMPPServerOutput::on_s2s_output_a4(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data)
{
	char buf[40];
	struct sockaddr_in target;
	
	XMPPServerOutput *p = static_cast<XMPPServerOutput *>(data);
	
	if ( result && result->dnsa4_nrr >= 1 )
	{
		
		p->state = CONNECTING;
		p->setFd( ::socket(PF_INET, SOCK_STREAM, 0) );
		printf("%s s2s-output(%s): connect to %s:%d, sock: %d\n", logtime().c_str(), p->hostname().c_str(), dns_ntop(AF_INET, &result->dnsa4_addr[0], buf, sizeof(buf)), p->port, p->getFd());
		
		target.sin_family = AF_INET;
		target.sin_port = htons(p->port);
		memcpy(&target.sin_addr, &result->dnsa4_addr[0], sizeof(result->dnsa4_addr[0]));
		memset(target.sin_zero, 0, 8);
		
		// принудительно выставить O_NONBLOCK
		int flags = fcntl(p->getFd(), F_GETFL, 0);
		if ( flags >= 0 )
		{
			fcntl(p->getFd(), F_SETFL, flags | O_NONBLOCK);
		}
		
		if ( ::connect(p->getFd(), (struct sockaddr *)&target, sizeof( struct sockaddr )) == 0 || errno == EINPROGRESS || errno == EALREADY )
		{
			p->state = CONNECTED;
			p->XMPPDomain::server->daemon->addObject(p);
			p->onConnect();
			return;
		}
		
		printf("%s s2s-output(%s): connect to %s:%d fault\n", logtime().c_str(), p->hostname().c_str(), dns_ntop(AF_INET, &result->dnsa4_addr[0], buf, sizeof(buf)), p->port);
	}
	else
	{
		printf("%s s2s-output(%s): connect failed: no IP address\n", logtime().c_str(), p->hostname().c_str());
	}
	
	p->XMPPDomain::server->removeDomain(p);
	p->XMPPDomain::server->daemon->removeObject(p);
	p->release();
}

/**
* Резолвер s2s хоста, запись SRV (_jabber._tcp)
*/
void XMPPServerOutput::on_s2s_output_jabber(struct dns_ctx *ctx, struct dns_rr_srv *result, void *data)
{
	XMPPServerOutput *p = static_cast<XMPPServerOutput *>(data);
	
	if ( result && result->dnssrv_nrr > 0 )
	{
		for(int i = 0; i < result->dnssrv_nrr; i++)
		{
			char buf[40];
			printf("%s SRV(%s) priority: %d, weight: %d, port: %d, name: %s\n",
			logtime().c_str(),
			p->hostname().c_str(),
			result->dnssrv_srv[i].priority,
			result->dnssrv_srv[i].weight,
			result->dnssrv_srv[i].port,
			result->dnssrv_srv[i].name);
		}
		
		p->port = result->dnssrv_srv[0].port;
		p->XMPPDomain::server->adns->a4(result->dnssrv_srv[0].name, on_s2s_output_a4, p);
		
		return;
	}
	
	p->port = 5269;
	p->XMPPDomain::server->adns->a4(p->hostname().c_str(), on_s2s_output_a4, p);
}

/**
* Резолвер s2s хоста, запись SRV (_xmpp-server._tcp)
*/
void XMPPServerOutput::on_s2s_output_xmpp_server(struct dns_ctx *ctx, struct dns_rr_srv *result, void *data)
{
	XMPPServerOutput *p = static_cast<XMPPServerOutput *>(data);
	
	if ( result && result->dnssrv_nrr > 0 )
	{
		for(int i = 0; i < result->dnssrv_nrr; i++)
		{
			char buf[40];
			printf("%s SRV(%s) priority: %d, weight: %d, port: %d, name: %s\n",
			logtime().c_str(),
			p->hostname().c_str(),
			result->dnssrv_srv[i].priority,
			result->dnssrv_srv[i].weight,
			result->dnssrv_srv[i].port,
			result->dnssrv_srv[i].name);
		}
		
		p->port = result->dnssrv_srv[0].port;
		p->XMPPDomain::server->adns->a4(result->dnssrv_srv[0].name, on_s2s_output_a4, p);
		
		return;
	}
	
	p->XMPPDomain::server->adns->srv(p->hostname().c_str(), "jabber", "tcp", on_s2s_output_jabber, p);
}

/**
* 2.1.1 Originating Server Generates Outbound Request for Authorization by Receiving Server
* 
* XEP-0220: Server Dialback
*/
void XMPPServerOutput::sendDBRequest(const std::string &from, const std::string &to)
{
	Stanza dbresult = new ATXmlTag("db:result");
	dbresult->setAttribute("from", from);
	dbresult->setAttribute("to", to);
	dbresult += sha1(from + ":" + to + ":" + key);
	sendStanza(dbresult);
	delete dbresult;
}

/**
* Событие: начало потока
*/
void XMPPServerOutput::onStartStream(const std::string &name, const attributes_t &attributes)
{
}

/**
* Событие: конец потока
*/
void XMPPServerOutput::onEndStream()
{
	printf("% s2s-output(%s): end of stream\n", logtime().c_str(), hostname().c_str());
	terminate();
}

/**
* Обработчик станзы
*/
void XMPPServerOutput::onStanza(Stanza stanza)
{
	// Шаг 1. проверка: "to" должен быть нашим хостом
	string to = stanza.to().hostname();
	if ( ! XMPPStream::server->isOurHost(to) )
	{
		Stanza stanza = Stanza::streamError("improper-addressing");
		sendStanza(stanza);
		delete stanza;
		terminate();
		return;
	}
	
	// Шаг 2. проверка: "from" должен быть НЕ нашим хостом
	string from = stanza.from().hostname();
	if ( XMPPStream::server->isOurHost(from) )
	{
		Stanza stanza = Stanza::streamError("improper-addressing");
		sendStanza(stanza);
		delete stanza;
		terminate();
		return;
	}
	
	if ( stanza->name() == "verify" ) onDBVerifyStanza(stanza);
	else if ( stanza->name() == "result" ) onDBResultStanza(stanza);
	else
	{
		printf("%s s2s-output(%s): unexpected stanza arrived: %s\n", logtime().c_str(), hostname().c_str(), stanza->name().c_str());
		Stanza error = Stanza::streamError("not-authoized");
		sendStanza(error);
		delete error;
		terminate();
	}
}

/**
* RFC 3920 (8.3.9) - анализируем результат проверки ключа
*/
void XMPPServerOutput::onDBVerifyStanza(Stanza stanza)
{
	printf("%s s2s-output(%s): db:verify to %s from %s type %s\n", logtime().c_str(), hostname().c_str(), stanza.to().hostname().c_str(), stanza.from().hostname().c_str(), stanza->getAttribute("type").c_str());
	
	// Шаг 1. проверка: "id" должен совпадать тем, что мы давали (id s2s-input'а)
	// TODO
	std::string id = stanza->getAttribute("id");
	XMPPServerInput *input = XMPPDomain::server->s2s->getInput(id);
	if ( ! input )
	{
		Stanza stanza = Stanza::streamError("invalid-id");
		sendStanza(stanza);
		delete stanza;
		terminate();
		return;
	}
	
	std::string to = stanza.to().hostname();
	std::string from = stanza.from().hostname();
	
	if ( stanza->getAttribute("type") == "valid" || stanza->getAttribute("type") == "invalid" )
	{
		// Шаг 4. вернуть результат в s2s-input
		input->authorize(from, to, stanza->getAttribute("type") == "valid");
		Stanza result = new ATXmlTag("db:result");
		result->setAttribute("to", from);
		result->setAttribute("from", to);
		result->setAttribute("type", stanza->getAttribute("type"));
		input->sendStanza(result);
		delete result;
	}
}

/**
* RFC 3920 (8.3.10)
*/
void XMPPServerOutput::onDBResultStanza(Stanza stanza)
{
	printf("%s s2s-output(%s): db:result to %s from %s type %s\n", logtime().c_str(), hostname().c_str(), stanza.to().hostname().c_str(), stanza.from().hostname().c_str(), stanza->getAttribute("type").c_str());
	if ( stanza->getAttribute("type") != "valid" )
	{
		terminate();
	}
	else
	{
		std::string vhostname = stanza.to().hostname();
		vhosts_t::const_iterator iter = vhosts.find(vhostname);
		vhost_t *vhost = iter != vhosts.end() ? iter->second : 0;
		if ( vhost )
		{
			vhost->authorized = true;
			
			printf("%s s2s-output(%s): flush authorized to %s\n", logtime().c_str(), hostname().c_str(), vhostname.c_str());
			for(buffer_t::iterator bi = vhost->buffer.begin(); bi != vhost->buffer.end(); bi++)
			{
				Stanza stanza = parse_xml_string(*bi);
				sendStanza(stanza);
				delete stanza;
			}
			vhost->buffer.clear();
		}
	}
}

/**
* Пир (peer) закрыл поток.
*
* Мы уже ничего не можем отправить в ответ,
* можем только корректно закрыть соединение с нашей стороны.
*/
void XMPPServerOutput::onPeerDown()
{
	printf("%s s2s-output(%s): peer down\n", logtime().c_str(), hostname().c_str());
	terminate();
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void XMPPServerOutput::onTerminate()
{
	printf("%s s2s-output(%s): terminate\n", logtime().c_str(), hostname().c_str());
	
	mutex.lock();
		endElement("stream:stream");
		flush();
		XMPPDomain::server->removeDomain(this);
		XMPPDomain::server->daemon->removeObject(this);
	mutex.unlock();
	
	release();
}

/**
* Роутер исходящих станз (thread-safe)
*
* Роутер передает станзу нужному потоку.
*
* @note Данная функция отвечает только за маршрутизацию, она не сохраняет офлайновые сообщения:
*   если адресат online, то пересылает ему станзу,
*   если offline, то вернет FALSE и вызывающая сторона должна сама сохранить офлайновое сообщение.
*
* @note Данный метод вызывается из глобального маршрутизатора станз XMPPServer::routeStanza()
*   вызывать его напрямую из других мест не рекомендуется - используйте XMPPServer::routeStanza()
*
* @note Данный метод в будущем станет виртуальным и будет перенесен в специальный
*   базовый класс, от которого будут наследовать VirtualHost (виртуальные узлы)
*   и, к примеру, MUC. Виртуальые узлы и MUC имеют общие черты, оба адресуются
*   доменом, оба принимают входящие станзы, но обрабатывают их по разному,
*   VirtualHost доставляет сообщения своим пользователям, а MUC доставляет
*   сообщения участникам комнат.
*
* @param to адресат которому надо направить станзу
* @param stanza станза
* @return TRUE - станза была отправлена, FALSE - станзу отправить не удалось
*/
bool XMPPServerOutput::routeStanza(Stanza stanza)
{
	lock();
	mutex.lock();
		std::string vhostname = stanza.from().hostname();
		vhosts_t::const_iterator iter = vhosts.find(vhostname);
		vhost_t *vhost = iter != vhosts.end() ? iter->second : 0;
		if ( ! vhost )
		{
			vhost = new vhost_t;
			vhost->authorized = false;
			vhosts[vhostname] = vhost;
			
			if ( state == CONNECTED )
			{
				sendDBRequest(vhostname, hostname());
			}
		}
	mutex.unlock();
	
	if ( vhost->authorized )
	{
		sendStanza(stanza);
	}
	else if ( stanza->name() == "db:verify" )
	{
		mutex.lock();
			if ( state == CONNECTED ) sendStanza(stanza);
			else vhost->connbuffer.push_back(stanza->asString());
		mutex.unlock();
	}
	else
	{
		vhost->buffer.push_back(stanza->asString());
	}
	
	release();
}
