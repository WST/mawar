
#include <s2slistener.h>
#include <s2sinputstream.h>
#include <s2soutputstream.h>
#include <attagparser.h>
#include <xmppserver.h>
#include <iostream>

using namespace std;
using namespace nanosoft;

/**
* Конструктор сервера
*/
S2SListener::S2SListener(XMPPServer *srv)
{
	server = srv;
}

/**
* Деструктор сервера
*/
S2SListener::~S2SListener()
{
	fprintf(stderr, "#%d: [S2SListener: %d] deleting\n", getWorkerId(), fd);
}

/**
* Обработчик события: подключение s2s
*
* thread-safe
*/
void S2SListener::onAccept()
{
	int sock = accept();
	if ( sock )
	{
		XMPPStream *client = new S2SInputStream(server, sock);
		server->daemon->addObject(client);
	}
}

/**
* Сигнал завершения работы
*
* Объект должен закрыть файловый дескриптор
* и освободить все занимаемые ресурсы
*/
void S2SListener::onTerminate()
{
	fprintf(stderr, "#%d: [S2SListener: %d] onTerminate\n", getWorkerId(), fd);
	server->daemon->removeObject(this);
}

#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <netdb.h>
#include <fcntl.h>


/**
* Резолвер s2s хоста, запись A (IPv4)
*/
void S2SListener::on_s2s_output_a4(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data)
{
	char buf[40];
	struct sockaddr_in target;
	
	printf("on_s2s_a4\n");
	if ( result && result->dnsa4_nrr >= 1 )
	{
		int sock = ::socket(PF_INET, SOCK_STREAM, 0);
		for(int i = 0; i < result->dnsa4_nrr; i++)
		{
			printf("  try addr: %s\n", dns_ntop(AF_INET, &result->dnsa4_addr[i], buf, sizeof(buf)));
			target.sin_family = AF_INET;
			target.sin_port = htons(5269);
			memcpy(&target.sin_addr, &result->dnsa4_addr[i], sizeof(result->dnsa4_addr[i]));
			memset(target.sin_zero, '\0', 8);
			if ( ::connect(sock, (struct sockaddr *)&target, sizeof( struct sockaddr )) == 0 )
			{
				fprintf(stderr, "  connected: %s\n", buf);
				pending_t *p = static_cast<pending_t*>(data);
				S2SOutputStream *stream = new S2SOutputStream(p->server, sock, p->to, p->from);
				fcntl(sock, F_SETFL, O_NONBLOCK);
				fprintf(stderr, "before addObject\n");
				p->server->daemon->addObject(stream);
				fprintf(stderr, "after addObject\n");
				stream->startStream();
				Stanza dbresult = new ATXmlTag("db:result");
				dbresult->setAttribute("to", p->to);
				dbresult->setAttribute("from", p->from);
				dbresult += "key";
				stream->sendStanza(dbresult);
				delete dbresult;
				return;
			}
			else
			{
				fprintf(stderr, "connect() fault\n");
			}
		}
		::close(sock);
	}
	printf("\n");
}

/**
* Резолвер s2s хоста, запись SRV (_jabber._tcp)
*/
static void on_s2s_output_jabber(struct dns_ctx *ctx, struct dns_rr_srv *result, void *data)
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
static void on_s2s_output_xmpp_server(struct dns_ctx *ctx, struct dns_rr_srv *result, void *data)
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
static void on_s2s_output_rbl(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data)
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
* Роутер исходящих s2s-станз (thread-safe)
* @param host домент в который надо направить станзу
* @param stanza станза
* @return TRUE - станза была отправлена, FALSE - станзу отправить не удалось
*/
bool S2SListener::routeStanza(const std::string &host, Stanza stanza)
{
	pending_t *p;
	
	mutex.lock();
		pendings_t::iterator iter = pendings.find(host);
		if ( iter == pendings.end() )
		{
			cerr << "new s2s-output to: " << host << endl;
			
			p = new pending_t;
			p->server = server;
			p->to = stanza.to().hostname();
			p->from = stanza.from().hostname();
			p->buffer.push_back(stanza->asString());
			pendings[host] = p;
			
			// Резолвим DNS записи сервера
			// NOTE для оптимизации отправляем все DNS (асинхронные) запросы сразу
			server->adns->a4(host.c_str(), on_s2s_output_a4, p);
			server->adns->srv(host.c_str(), "jabber", "tcp", on_s2s_output_jabber, p);
			server->adns->srv(host.c_str(), "xmpp-server", "tcp", on_s2s_output_xmpp_server, p);
			// TODO извлекать список DNSBL из конфига
			server->adns->a4((host + ".dnsbl.jabber.ru").c_str(), on_s2s_output_rbl, p);
		}
		else
		{
			p = iter->second;
			p->buffer.push_back(stanza);
		}
	mutex.unlock();
	return true;
}

/**
* Отправить буферизованные станзы
*/
void S2SListener::flush(S2SOutputStream *stream)
{
	pending_t *p;
	printf("flush: %s\n", stream->remoteHost().c_str());
	mutex.lock();
		pendings_t::iterator iter = pendings.find(stream->remoteHost());
		if ( iter != pendings.end() )
		{
			p = iter->second;
			//pendings.erase(iter);
		} else p = 0;
	mutex.unlock();
	printf("flush: %s send\n", stream->remoteHost().c_str());
	if ( p )
	{
		for(buffer_t::iterator bi = p->buffer.begin(); bi != p->buffer.end(); bi++)
		{
			Stanza stanza = parse_xml_string(*bi);
			stream->sendStanza(stanza);
			delete stanza;
		}
		delete p;
	}
	printf("flush: %s leave\n", stream->remoteHost().c_str());
}
