
#include <xmppserveroutput.h>
#include <xmppserver.h>
#include <xmppdomain.h>
#include <virtualhost.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <attagparser.h>

/**
* Конструктор s2s-домена
*/
XMPPServerOutput::XMPPServerOutput(XMPPServer *srv, const char *host):
	XMPPDomain(srv, host), XMPPStream(srv, 0)
{
	lock();
	// Резолвим DNS записи сервера
	state = RESOLVING;
	// TODO для оптимизации отправляем все DNS (асинхронные) запросы сразу
	//server->adns->a4(host.c_str(), on_s2s_output_a4, p);
	//server->adns->srv(host.c_str(), "jabber", "tcp", on_s2s_output_jabber, p);
	XMPPDomain::server->adns->srv(host, "xmpp-server", "tcp", on_s2s_output_xmpp_server, this);
}

/**
* Деструктор потока
*/
XMPPServerOutput::~XMPPServerOutput()
{
	fprintf(stderr, "s2s-output(%s) delete\n", hostname().c_str());
}

/**
* Событие готовности к передачи данных
*
* Вызывается сразу как только было установлено соединение
* и сокет готов к передаче данных
*/
void XMPPServerOutput::onConnected()
{
	printf("s2s-output(%s): connected\n", hostname().c_str());
	
	// поздоровайся с дядей
	initXML();
	startElement("stream:stream");
	setAttribute("xmlns:stream", "http://etherx.jabber.org/streams");
	setAttribute("xmlns", "jabber:server");
	setAttribute("xmlns:db", "jabber:server:dialback");
	setAttribute("xml:lang", "en");
	flush();
	
	mutex.lock();
		state = CONNECTED;
		
		for(vhosts_t::iterator iter = vhosts.begin(); iter != vhosts.end(); ++iter)
		{
			vhost_t *vhost = iter->second;
			
			Stanza dbresult = new ATXmlTag("db:result");
			dbresult->setAttribute("to", hostname());
			dbresult->setAttribute("from", iter->first);
			dbresult += "key";
			sendStanza(dbresult);
			delete dbresult;
		}
	mutex.unlock();
}

/**
* Событие готовности к записи
*
* Вызывается, когда в поток готов принять
* данные для записи без блокировки
*/
void XMPPServerOutput::onWrite()
{
	if ( state == CONNECTING )
	{
		want_write = false;
		onConnected();
		return;
	}
	XMPPStream::onWrite();
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
		p->fd = ::socket(PF_INET, SOCK_STREAM, 0);
		printf("s2s-output(%s): connect to %s:%d, sock: %d\n", p->hostname().c_str(), dns_ntop(AF_INET, &result->dnsa4_addr[0], buf, sizeof(buf)), p->port, p->fd);
		
		target.sin_family = AF_INET;
		target.sin_port = htons(p->port);
		memcpy(&target.sin_addr, &result->dnsa4_addr[0], sizeof(result->dnsa4_addr[0]));
		memset(target.sin_zero, 0, 8);
		
		// принудительно выставить O_NONBLOCK
		int flags = fcntl(p->fd, F_GETFL, 0);
		if ( flags >= 0 )
		{
			fcntl(p->fd, F_SETFL, flags | O_NONBLOCK);
		}
		
		if ( ::connect(p->fd, (struct sockaddr *)&target, sizeof( struct sockaddr )) == 0 || errno == EINPROGRESS || errno == EALREADY )
		{
			p->want_write = true;
			p->XMPPDomain::server->daemon->addObject(p);
			return;
		}
		
		fprintf(stderr, "s2s-output(%s): connect to %s:%d fault\n", p->hostname().c_str(), dns_ntop(AF_INET, &result->dnsa4_addr[0], buf, sizeof(buf)), p->port);
	}
	else
	{
		fprintf(stderr, "s2s-output(%s): connect failed: no IP address\n", p->hostname().c_str());
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
			printf("SRV(%s) priority: %d, weight: %d, port: %d, name: %s\n",
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
			printf("SRV(%s) priority: %d, weight: %d, port: %d, name: %s\n",
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
	fprintf(stderr, "s2s-output(%s) end of stream\n", hostname().c_str());
	terminate();
}

/**
* Обработчик станзы
*/
void XMPPServerOutput::onStanza(Stanza stanza)
{
	printf("s2s-output(%s) stanza: %s\n", hostname().c_str(), stanza->name().c_str());
	if ( stanza->name() == "verify" ) onDBVerifyStanza(stanza);
	else if ( stanza->name() == "result" ) onDBResultStanza(stanza);
	else
	{
		printf("s2s-output(%s): unexpected stanza arrived: %s\n", hostname().c_str(), stanza->name().c_str());
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
	// Шаг 1. проверка: "id" должен совпадать тем, что мы давали (id s2s-input'а)
	// TODO
	if ( 0 )
	{
		Stanza stanza = Stanza::streamError("invalid-id");
		sendStanza(stanza);
		delete stanza;
		terminate();
		return;
	}
	
	// Шаг 2. проверка: "to" должен быть нашим виртуальным хостом
	std::string to = stanza->getAttribute("to");
	XMPPDomain *host = XMPPDomain::server->getHostByName(to);
	if ( ! dynamic_cast<VirtualHost*>(host) )
	{
		Stanza stanza = Stanza::streamError("host-unknown");
		sendStanza(stanza);
		delete stanza;
		terminate();
		return;
	}
	
	// Шаг 3. проверка: "from" должен совпадать с тем, что нам дали
	// TODO
	std::string from = stanza->getAttribute("from");
	if ( dynamic_cast<VirtualHost*>(XMPPDomain::server->getHostByName(from)) )
	{
		Stanza stanza = Stanza::streamError("invalid-from");
		sendStanza(stanza);
		delete stanza;
		terminate();
		return;
	}
	
	// Шаг 4. вернуть результат в s2s-input
	// TODO
}

/**
* RFC 3920 (8.3.10)
*/
void XMPPServerOutput::onDBResultStanza(Stanza stanza)
{
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
			
			printf("s2s-output(%s to %s): flush\n", hostname().c_str(), vhostname.c_str());
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
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void XMPPServerOutput::onTerminate()
{
	fprintf(stderr, "s2s-output(%s) onTerminate\n", hostname().c_str());
	
	mutex.lock();
		endElement("stream:stream");
		flush();
		XMPPDomain::server->removeDomain(this);
		XMPPDomain::server->daemon->removeObject(this);
		close();
	mutex.unlock();
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
				Stanza dbresult = new ATXmlTag("db:result");
				dbresult->setAttribute("to", hostname());
				dbresult->setAttribute("from", vhostname);
				dbresult += "key";
				sendStanza(dbresult);
				delete dbresult;
			}
		}
	mutex.unlock();
	
	if ( vhost->authorized )
	{
		sendStanza(stanza);
	}
	else
	{
		vhost->buffer.push_back(stanza->asString());
	}
	release();
}
