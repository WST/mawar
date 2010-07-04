
#include <s2slistener.h>
#include <s2sinputstream.h>
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
