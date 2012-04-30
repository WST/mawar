
#include <xmppextensioninput.h>
#include <xmppextension.h>
#include <xmppserver.h>

#include <stdio.h>

/**
* Конструктор потока
*/
XMPPExtensionInput::XMPPExtensionInput(XMPPExtension *ext, int fd_in):
	XMPPStream(ext->server, fd_in), extension(ext)
{
}

/**
* Деструктор потока
*/
XMPPExtensionInput::~XMPPExtensionInput()
{
}

/**
* Событие ошибки
*
* Вызывается в случае возникновения какой-либо ошибки
*/
void XMPPExtensionInput::onError(const char *message)
{
	printf("[XMPPExtensionInput: %d] ", fd, message);
	extension->terminate();
}

/**
* Обработчик станзы
*/
void XMPPExtensionInput::onStanza(Stanza stanza)
{
	extension->server->routeStanza(stanza);
}

/**
* Событие: начало потока
*/
void XMPPExtensionInput::onStartStream(const std::string &name, const attributes_t &attributes)
{
}

/**
* Событие: конец потока
*/
void XMPPExtensionInput::onEndStream()
{
}

/**
* Пир (peer) закрыл поток.
*
* Мы уже ничего не можем отправить в ответ,
* можем только корректно закрыть соединение с нашей стороны.
*/
void XMPPExtensionInput::onPeerDown()
{
	printf("[XMPPExtensionOutput: %d] onPeerDown\n", fd);
	extension->terminate();
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void XMPPExtensionInput::onTerminate()
{
	mutex.lock();
		endElement("stream:stream");
		flush();
		server->daemon->removeObject(this);
	mutex.unlock();
}
