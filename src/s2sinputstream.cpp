
#include <s2sinputstream.h>
#include <virtualhost.h>
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

/**
* Сигнал завершения работы
*
* Объект должен закрыть файловый дескриптор
* и освободить все занимаемые ресурсы
*/
void S2SInputStream::onTerminate()
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
