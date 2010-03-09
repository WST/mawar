
#include <xep0114listener.h>
#include <xmppserver.h>
#include <xmppstream.h>
#include <xep0114.h>

/**
* Конструктор
*/
XEP0114Listener::XEP0114Listener(XMPPServer *srv): server(srv)
{
}

/**
* Деструктор
*/
XEP0114Listener::~XEP0114Listener()
{
}

#include <xmppclient.h>
/**
* Обработчик события: подключение клиента
*
* thread-safe
*/
AsyncObject* XEP0114Listener::onAccept()
{
	mutex.lock();
		int sock = accept();
		XMPPStream *client = 0;
		if ( sock )
		{
			client = new XEP0114(server, sock);
			server->daemon->addObject(client);
		}
	mutex.unlock();
	return client;
}

/**
* Сигнал завершения работы
*
* Объект должен закрыть файловый дескриптор
* и освободить все занимаемые ресурсы
*/
void XEP0114Listener::onTerminate()
{
	fprintf(stderr, "#%d: [XEP0114Listener: %d] onTerminate\n", getWorkerId(), fd);
	server->daemon->removeObject(this);
}
