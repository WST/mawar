
#include <s2soutputstream.h>
#include <s2slistener.h>
#include <virtualhost.h>
#include <iostream>

using namespace std;

/**
* Конструктор потока
*/
S2SOutputStream::S2SOutputStream(XMPPServer *srv, int sock, const std::string &tohost, const std::string &fromhost): XMPPStream(srv, sock), XMPPDomain(srv, tohost)
{
	state = init;
	remote_host = tohost;
	local_host = fromhost;
}

/**
* Деструктор потока
*/
S2SOutputStream::~S2SOutputStream()
{
}

/**
* Открыть поток
*/
void S2SOutputStream::startStream()
{
	fprintf(stderr, "#%d new s2s output stream\n", getWorkerId());
	initXML();
	startElement("stream:stream");
	setAttribute("xmlns:stream", "http://etherx.jabber.org/streams");
	setAttribute("xmlns", "jabber:server");
	setAttribute("xmlns:db", "jabber:server:dialback");
	setAttribute("xml:lang", "en");
	flush();
}

/**
* Событие: начало потока
*/
void S2SOutputStream::onStartStream(const std::string &name, const attributes_t &attributes)
{
	state = verify;
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
	fprintf(stderr, "#%d s2s-output stanza: %s\n", getWorkerId(), stanza->name().c_str());
	if ( stanza->name() == "verify" ) onDBVerifyStanza(stanza);
	else if ( stanza->name() == "result" ) onDBResultStanza(stanza);
	else
	{
		fprintf(stderr, "#%d unexpected s2s-output stanza arrived: %s\n", getWorkerId(), stanza->name().c_str());
		Stanza error = Stanza::streamError("not-authoized");
		sendStanza(error);
		delete error;
		terminate();
	}
}

/**
* Обработка <db:verify>
*/
void S2SOutputStream::onDBVerifyStanza(Stanza stanza)
{
	Stanza verify = new ATXmlTag("db:verify");
	verify->setAttribute("from", local_host);
	verify->setAttribute("to", remote_host);
	verify->setAttribute("type", "valid");
	verify->setAttribute("id", stanza->getAttribute("id"));
	sendStanza(verify);
	delete verify;
}

/**
* Обработка <db:result>
*/
void S2SOutputStream::onDBResultStanza(Stanza stanza)
{
	if ( stanza->getAttribute("type") != "valid" )
	{
		terminate();
	}
	else
	{
		state = authorized;
		XMPPStream::server->addDomain(this);
		XMPPStream::server->s2s->flush(this);
	}
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
	XMPPStream::server->daemon->removeObject(this);
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

/**
* Роутер исходящих станз (thread-safe)
*
* Роутер передает станзу нужному потоку.
*
* @note Данная функция отвечает только за маршрутизацию, она не сохраняет офлайновые сообщения:
*   если адресат online, то пересылает ему станзу,
*   если offline, то вернет FALSE и вызывающая сторона должна сама сохранить офлайновое сообщение.
*
* @note Данный метод вызывается из глобального маршрутизатора станз XMPPServer::routeStanza()
*   вызывать его напрямую из других мест не рекомендуется - используйте XMPPServer::routeStanza()
*
* @note Данный метод в будущем станет виртуальным и будет перенесен в специальный
*   базовый класс, от которого будут наследовать VirtualHost (виртуальные узлы)
*   и, к примеру, MUC. Виртуальые узлы и MUC имеют общие черты, оба адресуются
*   доменом, оба принимают входящие станзы, но обрабатывают их по разному,
*   VirtualHost доставляет сообщения своим пользователям, а MUC доставляет
*   сообщения участникам комнат.
*
* @param to адресат которому надо направить станзу
* @param stanza станза
* @return TRUE - станза была отправлена, FALSE - станзу отправить не удалось
*/
bool S2SOutputStream::routeStanza(Stanza stanza)
{
	sendStanza(stanza);
}
