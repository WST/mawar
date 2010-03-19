
#include <xmppproxy.h>
#include <xmppproxystream.h>

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
	int sock = accept();
	if ( sock )
	{
		XMPPProxyStream *client = new XMPPProxyStream(this);
		if ( ! client->accept(sock, server_ip, server_port) )
		{
			// shit!
			delete client;
			return;
		}
		client->rxsec_limit = 10240;
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
