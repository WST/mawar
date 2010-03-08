
#include <s2sinputstream.h>
#include <virtualhost.h>
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
	setAttribute("id", id = "123456"); // Требования к id — непредсказуемость и уникальность
	setAttribute("xml:lang", "en");
	flush();
}

/**
* Событие: конец потока
*/
void S2SInputStream::onEndStream()
{
	cerr << "s2s input stream closed" << endl;
	endElement("stream:stream");
}

/**
* Обработчик станзы
*/
void S2SInputStream::onStanza(Stanza stanza)
{
	fprintf(stderr, "#%d s2s-input stanza: %s\n", getWorkerId(), stanza->name().c_str());
	if (stanza->name() == "verify" ) onDBVerifyStanza(stanza);
	else if (stanza->name() == "result") onDBResultStanza(stanza);
	else
	{
		fprintf(stderr, "#%d unexpected s2s-input stanza: %s\n", getWorkerId(), stanza->name().c_str());
		Stanza error = Stanza::streamError("not-authoized");
		sendStanza(error);
		delete error;
		terminate();
	}
}

/**
* Обработка <db:verify>
*/
void S2SInputStream::onDBVerifyStanza(Stanza stanza)
{
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
	VirtualHost *vhost = dynamic_cast<VirtualHost *>(host);
	if ( ! vhost )
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
	
	// Шаг 3. резолвим DNS записи сервера
	// NOTE для оптимизации отправляем все DNS (асинхронные) запросы сразу
	server->adns->a4(from.c_str(), on_s2s_a4, this);
	server->adns->srv(from.c_str(), "jabber", "tcp", on_s2s_srv_jabber, this);
	server->adns->srv(from.c_str(), "xmpp-server", "tcp", on_srv_xmpp_server, this);
	// TODO извлекать список DNSBL из конфига
	server->adns->a4((from + ".dnsbl.jabber.ru").c_str(), on_s2s_rbl, this);
}

/**
* Событие закрытие соединения
*
* Вызывается если peer закрыл соединение
*/
void S2SInputStream::onShutdown()
{
	cerr << "[s2s input]: peer shutdown connection" << endl;
	if ( state != terminating ) {
		AsyncXMLStream::onShutdown();
		XMLWriter::flush();
	}
	server->daemon->removeObject(this);
	shutdown(READ | WRITE);
	delete this;
	cerr << "[s2s input]: shutdown leave" << endl;
}

void S2SInputStream::terminate()
{
	cerr << "[s2s input]: terminating connection..." << endl;
	
	switch ( state )
	{
	case terminating:
		return;
	case authorized:
		break;
	}
	
	mutex.lock();
		if ( state != terminating ) {
			state = terminating;
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
void S2SInputStream::onTerminate()
{
	terminate();
}
