
#include <s2slistener.h>
#include <s2sinputstream.h>
#include <xmppserver.h>

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
}

/**
* Обработчик события: подключение s2s
*
* thread-safe
*/
AsyncObject* S2SListener::onAccept()
{
	XMPPStream *client;
	
	int sock = accept();
	if ( sock )
	{
		client = new S2SInputStream(server, sock);
		server->daemon->addObject(client);
	}
	
	return client;
}

/**
* Сигнал завершения работы
*
* Объект должен закрыть файловый дескриптор
* и освободить все занимаемые ресурсы
*/
void S2SListener::onTerminate()
{
	onError("S2SListener::onTerminate()...");
	server->daemon->removeObject(this);
	onError("S2SListener::onTerminate() leave...");
}

/**
* Сигнал завершения
*/
void S2SListener::onSigTerm()
{
}

/**
* Сигнал HUP
*/
void S2SListener::onSigHup()
{
}
