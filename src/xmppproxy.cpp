
#include <xmppproxy.h>
#include <xmppproxystream.h>
#include <logs.h>
#include <sys/socket.h>
#include <arpa/inet.h>

using namespace std;
using namespace nanosoft;

/**
* Конструктор сервера
* @param d ссылка на демона
* @param ip IP адрес проксируемого сервера
* @param port порт проксируемого сервера
*/
XMPPProxy::XMPPProxy(NetDaemon *d, const char *ip, int port):
	daemon(d), server_ip(ip), server_port(port)
{
	whitecount = 0;
}

/**
* Деструктор прокси сервера
*/
XMPPProxy::~XMPPProxy()
{
	fprintf(stderr, "#%d: [XMPPProxy: %d] deleting\n", getWorkerId(), fd);
}

/**
* Принять входящее соединение
*/
void XMPPProxy::onAccept()
{
	struct sockaddr_in target;
	socklen_t sl = sizeof( struct sockaddr );
	int sock = ::accept(fd, (struct sockaddr *)&target, &sl);
	
	if ( sock > 0 )
	{
		XMPPProxyStream *client = new XMPPProxyStream(this);
		
		inet_ntop(target.sin_family, &(target.sin_addr), client->remoteIP.ptr(), client->remoteIP.bufsz());
		
		client->rxsec_limit = 10240;
		for(int i = 0; i < whitecount; i++)
		{
			if ( client->remoteIP == whitelist[i] )
			{
				client->rxsec_limit = 0;
				break;
			}
		}
		
		fprintf(stdlog, "%s [proxyd] connect from: %s limit: %d\n", logtime().c_str(), client->remoteIP.c_str(), client->rxsec_limit);
		
		if ( ! client->accept(sock, server_ip, server_port) )
		{
			// shit!
			delete client;
			return;
		}
		
	}
}

/**
* Сигнал завершения работы
*/
void XMPPProxy::onTerminate()
{
	fprintf(stderr, "#%d: [XMPPProxy: %d] onTerminate\n", getWorkerId(), fd);
	daemon->removeObject(this);
}

/**
* Сигнал завершения
*/
void XMPPProxy::onSigTerm()
{
	fprintf(stderr, "#%d: [XMPPProxy: %d] SIGTERM\n", getWorkerId(), fd);
	daemon->terminate();
}

/**
* Сигнал HUP
*/
void XMPPProxy::onSigHup()
{
	//fprintf(stderr, "#%d: [XMPPServer: %d] SIGUP not implemented yet\n", getWorkerId(), fd);
}

/**
* Перезагрузить белый список из файла
*/
void XMPPProxy::reloadWhiteList(const char *path)
{
	FILE *f = fopen(path, "r");
	if ( f )
	{
		whitecount = 0;
		do
		{
			if ( fscanf(f, "%s", whitelist[whitecount].ptr()) > 0 )
			{
				fprintf(stderr, "whitelist: '%s'\n", whitelist[whitecount].c_str());
				whitecount++;
			}
		} while ( ! feof(f) );
		fclose(f);
	}
}
