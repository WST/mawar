
#include <s2slistener.h>
#include <xmppserverinput.h>
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
	fprintf(stderr, "#%d: [S2SListener: %d] deleting\n", getWorkerId(), getFd());
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
		XMPPServerInput *client = new XMPPServerInput(server, sock);
		mutex.lock();
			inputs[client->getId()] = client;
		mutex.unlock();
		server->daemon->addObject(client);
	}
}

/**
* Найти s2s-input по ID
*
* thread-safe
*/
XMPPServerInput * S2SListener::getInput(const std::string &id)
{
	mutex.lock();
		inputs_t::const_iterator iter = inputs.find(id);
		XMPPServerInput *input = iter != inputs.end() ? iter->second : 0;
	mutex.unlock();
	
	return input;
}

/**
* Удалить s2s-input
*
* thread-safe
*/
void S2SListener::removeInput(XMPPServerInput *input)
{
	mutex.lock();
		inputs.erase(input->getId());
	mutex.unlock();
}

/**
* Сигнал завершения работы
*
* Объект должен закрыть файловый дескриптор
* и освободить все занимаемые ресурсы
*/
void S2SListener::onTerminate()
{
	fprintf(stderr, "#%d: [S2SListener: %d] onTerminate\n", getWorkerId(), getFd());
	server->daemon->removeObject(this);
}
