
#include <s2soutputstream.h>
#include <virtualhost.h>
#include <iostream>

using namespace std;

/**
* Конструктор потока
*/
S2SOutputStream::S2SOutputStream(XMPPServer *srv, int sock): XMPPStream(server, sock)
{
}

/**
* Деструктор потока
*/
S2SOutputStream::~S2SOutputStream()
{
}

/**
* Событие: начало потока
*/
void S2SOutputStream::onStartStream(const std::string &name, const attributes_t &attributes)
{
	
}

/**
* Событие: конец потока
*/
void S2SOutputStream::onEndStream()
{
	cerr << "s2s output stream closed" << endl;
	endElement("stream:stream");
}

/**
* Обработчик станзы
*/
void S2SOutputStream::onStanza(Stanza stanza)
{
}

/**
* Событие закрытие соединения
*
* Вызывается если peer закрыл соединение
*/
void S2SOutputStream::onShutdown()
{
	cerr << "[s2s output]: peer shutdown connection" << endl;
	if ( state != terminating ) {
		AsyncXMLStream::onShutdown();
		XMLWriter::flush();
	}
	server->daemon->removeObject(this);
	shutdown(READ | WRITE);
	delete this;
	cerr << "[s2s output]: shutdown leave" << endl;
}

/**
* Сигнал завершения работы
*
* Объект должен закрыть файловый дескриптор
* и освободить все занимаемые ресурсы
*/
void S2SOutputStream::onTerminate()
{
	cerr << "[s2s output]: terminating connection..." << endl;
	
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
