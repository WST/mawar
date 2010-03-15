#ifndef MAWAR_XMPPPROXYSTREAM_H
#define MAWAR_XMPPPROXYSTREAM_H

#include <nanosoft/asyncstream.h>
#include <nanosoft/mutex.h>

#define PROXY_BUFFER_SIZE 4096

/**
* Класс потока XMPP-прокси
*
* Читает блок данных из входного сокета и останавливает чтение
* все данные не запишутся в выходной сокет
*/
class XMPPProxyStream: public AsyncStream
{
private:
	/**
	* Mutex для сихронизации
	*/
	nanosoft::Mutex mutex;
	
	/**
	* счетчик финализации
	*/
	int finish_count;
	
	/**
	* Ссылка на противоположный сокет
	*/
	XMPPProxyStream *pair;
	
	/**
	* TRUE готов читать
	*/
	bool ready_read;
	
	/**
	* TRUE готов писать
	*/
	bool ready_write;
	
	/**
	* Размер буферизованных данных
	*/
	size_t len;
	
	/**
	* Размер уже записанных данных
	*/
	size_t written;
	
	/**
	* Буфер
	*/
	char buffer[PROXY_BUFFER_SIZE];
protected:
	/**
	* Вернуть маску ожидаемых событий
	*/
	virtual uint32_t getEventsMask();
	
	/**
	* Событие готовности к чтению
	*
	* Вызывается когда в потоке есть данные,
	* которые можно прочитать без блокирования
	*/
	virtual void onRead();
	
	/**
	* Записать кусок
	* @return TRUE всё записал, FALSE ничего не записал или только часть
	*/
	bool writeChunk();
	
	/**
	* Событие готовности к записи
	*
	* Вызывается, когда в поток готов принять
	* данные для записи без блокировки
	*/
	virtual void onWrite();
	
	/**
	* Пир (peer) закрыл поток.
	*
	* Мы уже ничего не можем отправить в ответ,
	* можем только корректно закрыть соединение с нашей стороны.
	*/
	virtual void onPeerDown();
	virtual void onShutdown();
	
	/**
	* Сигнал завершения работы
	*
	* Сервер решил закрыть соединение, здесь ещё есть время
	* корректно попрощаться с пиром (peer).
	*/
	virtual void onTerminate();
public:
	/**
	* Ссылка на прокси
	*/
	class XMPPProxy *proxy;
	
	/**
	* Конструктор потока XMPP-прокси
	*/
	XMPPProxyStream(class XMPPProxy *prx);
	
	/**
	* Конструктор потока XMPP-прокси
	*
	* @param prx ссылка на прокси
	* @param s ссылка на противоположный сокет
	* @param socket сокет
	*/
	XMPPProxyStream(class XMPPProxy *prx, XMPPProxyStream *s, int socket);
	
	/**
	* Деструктор потока XMPP-прокси
	*/
	~XMPPProxyStream();
	
	/**
	* Принять сокет
	* @param sock принимаемый сокет
	* @param ip IP сервера к которому надо приконнектисться
	* @param port порт сервера
	* @return TRUE - сокет принят, FALSE сокет отклонен
	*/
	bool accept(int sock, const char *ip, int port);
	
	/**
	* Финализация
	*/
	void finish();
};

#endif // MAWAR_XMPPPROXYSTREAM_H
