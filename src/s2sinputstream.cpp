
#include <s2sinputstream.h>
#include <s2soutputstream.h>
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
	fprintf(stderr, "#%d new s2s stream\n", getWorkerId());
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
	fprintf(stderr, "#%d: [S2SInputStream: %d] end of stream\n", getWorkerId(), fd);
	terminate();
}

/**
* Обработчик станзы
*/
void S2SInputStream::onStanza(Stanza stanza)
{
	fprintf(stderr, "#%d s2s-input stanza: %s\n", getWorkerId(), stanza->name().c_str());
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
* Обработка <db:verify>
*/
void S2SInputStream::onDBVerifyStanza(Stanza stanza)
{
	Stanza verify = new ATXmlTag("db:verify");
	verify->setAttribute("from", stanza->getAttribute("to"));
	verify->setAttribute("to", stanza->getAttribute("from"));
	verify->setAttribute("type", "valid");
	verify->setAttribute("id", stanza->getAttribute("id"));
	sendStanza(verify);
	delete verify;
}

/**
* Резолвер s2s хоста, запись A (IPv4)
*/
static void on_s2s_a4(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data)
{
  printf("on_s2s_a4\n");
  if ( result )
  for(int i = 0; i < result->dnsa4_nrr; i++)
  {
    char buf[40];
    printf("  addr: %s\n", dns_ntop(AF_INET, &result->dnsa4_addr[i], buf, sizeof(buf)));
  }
  printf("\n");
}

/**
* Резолвер s2s хоста, запись SRV (_jabber._tcp)
*/
static void on_s2s_srv_jabber(struct dns_ctx *ctx, struct dns_rr_srv *result, void *data)
{
  printf("on_s2s_srv_jabber\n");
  if ( result )
  for(int i = 0; i < result->dnssrv_nrr; i++)
  {
    char buf[40];
    printf("  SRV priority: %d, weight: %d, port: %d, name: %s\n",
      result->dnssrv_srv[i].priority,
      result->dnssrv_srv[i].weight,
      result->dnssrv_srv[i].port,
      result->dnssrv_srv[i].name);
  }
  printf("\n");
}

/**
* Резолвер s2s хоста, запись SRV (_xmpp-server._tcp)
*/
static void on_srv_xmpp_server(struct dns_ctx *ctx, struct dns_rr_srv *result, void *data)
{
  printf("on_srv_xmpp_server\n");
  if ( result )
  for(int i = 0; i < result->dnssrv_nrr; i++)
  {
    char buf[40];
    printf("  SRV priority: %d, weight: %d, port: %d, name: %s\n",
      result->dnssrv_srv[i].priority,
      result->dnssrv_srv[i].weight,
      result->dnssrv_srv[i].port,
      result->dnssrv_srv[i].name);
  }
  printf("\n");
}

/**
* Резолвер s2s хоста, запись RBL
*/
static void on_s2s_rbl(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data)
{
  printf("on_s2s_rbl\n");
  if ( result ) {
  for(int i = 0; i < result->dnsa4_nrr; i++)
  {
    char buf[40];
    printf("  addr: %s\n", dns_ntop(AF_INET, &result->dnsa4_addr[i], buf, sizeof(buf)));
  }
  }
  printf("\n");
}

/**
* Обработка <db:result>
*/
void S2SInputStream::onDBResultStanza(Stanza stanza)
{
	string to = stanza->getAttribute("to");
	string from = stanza->getAttribute("from");
	cerr << "[s2s-input] db:result to: " << to << ", from: " << from << endl;
	
	// Шаг 1. проверка: "to" должен быть нашим виртуальным хостом
	XMPPDomain *host = server->getHostByName(to);
	if ( ! host || dynamic_cast<S2SOutputStream*>(host) )
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
	// Так что, если будете открывать повторные коннекты, то забудьте
	// блокировать попытки коннекта к самим себе.
	if ( server->getHostByName(from) )
	{
		Stanza stanza = Stanza::streamError("not-authorized");
		sendStanza(stanza);
		delete stanza;
		terminate();
		return;
	}
	remote_host = from;
	
	// Шаг 3. резолвим DNS записи сервера
	// NOTE для оптимизации отправляем все DNS (асинхронные) запросы сразу
	server->adns->a4(from.c_str(), on_s2s_a4, this);
	server->adns->srv(from.c_str(), "jabber", "tcp", on_s2s_srv_jabber, this);
	server->adns->srv(from.c_str(), "xmpp-server", "tcp", on_srv_xmpp_server, this);
	// TODO извлекать список DNSBL из конфига
	server->adns->a4((from + ".dnsbl.jabber.ru").c_str(), on_s2s_rbl, this);
	
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
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void S2SInputStream::onTerminate()
{
	fprintf(stderr, "#%d: [S2SInputStream: %d] onTerminate\n", getWorkerId(), fd);
	
	// if ( state == authorized ) server->onOffline(this);
	
	mutex.lock();
		endElement("stream:stream");
		flush();
		shutdown(WRITE);
	mutex.unlock();
}
