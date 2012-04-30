#ifndef MAWAR_EXTENSIONOUTPUT_H
#define MAWAR_EXTENSIONOUTPUT_H

#include <xmppstream.h>
#include <xmppextension.h>

class XMPPExtension;

/**
* Класс потока вывода (extension output)
*
* Поток вывода используется только для отправки станз
* модулю расширения, для приёма станз из модуля расширения
* используется другой тип потока - XMPPExtensionInput.
*
* XMPPExtensionInput - прием входящих станз от модуля
* XMPPExtensionOutput - отправка исходящих станз в модуль
*/
class XMPPExtensionOutput: public XMPPStream
{
protected:
	/**
	* Событие ошибки
	*
	* Вызывается в случае возникновения какой-либо ошибки
	*/
	virtual void onError(const char *message);
	
	/**
	* Обработчик станзы
	*/
	virtual void onStanza(Stanza stanza);
	
	/**
	* Событие: начало потока
	*/
	virtual void onStartStream(const std::string &name, const attributes_t &attributes);
	
	/**
	* Событие: конец потока
	*/
	virtual void onEndStream();
	
	/**
	* Пир (peer) закрыл поток.
	*
	* Мы уже ничего не можем отправить в ответ,
	* можем только корректно закрыть соединение с нашей стороны.
	*/
	virtual void onPeerDown();
	
	/**
	* Сигнал завершения работы
	*
	* Сервер решил закрыть соединение, здесь ещё есть время
	* корректно попрощаться с пиром (peer).
	*/
	virtual void onTerminate();
public:
	XMPPExtension *extension;
	
	/**
	* Конструктор потока
	*/
	XMPPExtensionOutput(XMPPExtension *ext, int fd_out);
	
	/**
	* Деструктор потока
	*/
	~XMPPExtensionOutput();
	
	void init();
};

#endif // MAWAR_EXTENSIONOUTPUT_H
