
#include <xmppextensionoutput.h>
#include <xmppextension.h>
#include <xmppserver.h>
#include <sys/epoll.h>

#include <stdio.h>

/**
* Конструктор потока
*/
XMPPExtensionOutput::XMPPExtensionOutput(XMPPExtension *ext, int fd_out):
	XMPPStream(ext->server, fd_out), extension(ext)
{
}

/**
* Деструктор потока
*/
XMPPExtensionOutput::~XMPPExtensionOutput()
{
}

void XMPPExtensionOutput::init()
{
	printf("XMPPExtensionOutput(%d)\n", getFd());
	initXML();
	startElement("stream:stream");
	setAttribute("xmlns:stream", "http://etherx.jabber.org/streams");
	setAttribute("xmlns", "jabber:server");
	setAttribute("xmlns:db", "jabber:server:dialback");
	setAttribute("xml:lang", "en");
	flush();
}

/**
* Событие ошибки
*
* Вызывается в случае возникновения какой-либо ошибки
*/
void XMPPExtensionOutput::onError(const char *message)
{
	printf("[XMPPExtensionOutput: %d] error %s\n", getFd(), message);
	extension->terminate();
}

/**
* Обработчик станзы
*/
void XMPPExtensionOutput::onStanza(Stanza stanza)
{
}

/**
* Событие: начало потока
*/
void XMPPExtensionOutput::onStartStream(const std::string &name, const attributes_t &attributes)
{
}

/**
* Событие: конец потока
*/
void XMPPExtensionOutput::onEndStream()
{
}

/**
* Пир (peer) закрыл поток.
*
* Мы уже ничего не можем отправить в ответ,
* можем только корректно закрыть соединение с нашей стороны.
*/
void XMPPExtensionOutput::onPeerDown()
{
	printf("[XMPPExtensionOutput: %d] onPeerDown\n", getFd());
	extension->terminate();
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void XMPPExtensionOutput::onTerminate()
{
	mutex.lock();
		endElement("stream:stream");
		flush();
		server->daemon->removeObject(this);
	mutex.unlock();
}
