
#include <xmppproxystream.h>
#include <xmppproxy.h>
#include <logs.h>
#include <sys/socket.h>
#include <arpa/inet.h>
//#include <netinet/in.h>
//#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>

using namespace std;

/**
* Конструктор
*/
XMPPProxyStream::XMPPProxyStream(XMPPProxy *prx):
	proxy(prx), AsyncStream(0),
	ready_read(false), ready_write(false),
	pair(0), finish_count(0)
{
	tx = rx = rxsec = rxsec_limit = rxsec_switch = 0;
	client = true;
}

/**
* Конструктор потока XMPP-прокси
*
* @param prx ссылка на прокси
* @param s ссылка на противоположный сокет
* @param socket сокет
*/
XMPPProxyStream::XMPPProxyStream(class XMPPProxy *prx, XMPPProxyStream *s, int socket):
	proxy(prx), AsyncStream(socket),
	ready_read(true), ready_write(false),
	pair(s), finish_count(0)
{
	tx = rx = rxsec = rxsec_limit = rxsec_switch = 0;
	client = false;
}

/**
* Деструктор
*/
XMPPProxyStream::~XMPPProxyStream()
{
	if ( client )
	{
		fprintf(stdlog, "%s [proxyd] disconnect from: %s, rx: %lld, tx: %lld\n", logtime().c_str(), remoteIP.c_str(), rx, tx);
	}
}

/**
* Принять сокет
* @param sock принимаемый сокет
* @param ip IP сервера к которому надо приконнектисться
* @param port порт сервера
* @return TRUE - сокет принят, FALSE сокет отклонен
*/
bool XMPPProxyStream::accept(int sock, const char *ip, int port)
{
	struct sockaddr_in target;
	
	// Клиент который к нам приконнектился
	fd = sock;
	
	// Сервер к которому мы коннектимся
	int server_socket = ::socket(PF_INET, SOCK_STREAM, 0);
	if ( server_socket <= 0 ) return false;
	
	target.sin_family = AF_INET;
	target.sin_port = htons(port);
	inet_pton(AF_INET, ip, &(target.sin_addr));
	memset(target.sin_zero, '\0', 8);
	
	if ( ::connect(server_socket, (struct sockaddr *)&target, sizeof( struct sockaddr )) == 0 )
	{
		fcntl(server_socket, F_SETFL, O_NONBLOCK);
		this->pair = new XMPPProxyStream(proxy, this, server_socket);
		ready_read = true;
		proxy->daemon->addObject(this);
		proxy->daemon->addObject(this->pair);
		return true;
	}
	::close(server_socket);
	
	// shit!
	fprintf(stdlog, "%s [proxyd] connect(%s:%d) for %s failed\n", logtime().c_str(), ip, port, remoteIP.c_str());
	::shutdown(fd, SHUT_RDWR);
	return false;
}

/**
* Вернуть маску ожидаемых событий
*/
uint32_t XMPPProxyStream::getEventsMask()
{
	uint32_t mask = EPOLLRDHUP | EPOLLONESHOT | EPOLLHUP | EPOLLERR;
	if ( ready_read && ((rxsec_limit == 0) || (rxsec <= rxsec_limit)) ) mask |= EPOLLIN;
	if ( ready_write ) mask |= EPOLLOUT;
	return mask;
}

/**
* Таймер-функция разблокировки потока
* @param data указатель на XMPPProxyStream который надо разблочить
*/
void XMPPProxyStream::unblock(int wid, void *data)
{
	XMPPProxyStream *stream = static_cast<XMPPProxyStream *>(data);
	printf("[XMPPProxyStream: %d] unlock stream\n", stream->fd);
	stream->rxsec = 0;
	stream->proxy->daemon->modifyObject(stream);
	stream->release();
}

/**
* Событие готовности к чтению
*
* Вызывается когда в потоке есть данные,
* которые можно прочитать без блокирования
*/
void XMPPProxyStream::onRead()
{
	if ( mutex.lock() )
	{
		ssize_t r = read(pair->buffer, sizeof(pair->buffer));
		if ( r > 0 )
		{
			rx += r;
			pair->len = r;
			pair->written = 0;
			
			//if ( ! pair->writeChunk() )
			{
				ready_read = false;
				pair->ready_write = true;
				proxy->daemon->modifyObject(pair);
			}
			
			// проверка ограничений
			
			if ( rxsec_limit > 0 )
			{
				
					time_t tm = time(0);
					int mask = tm & 1;
					if ( rxsec_switch != mask )
					{
						rxsec = 0;
						rxsec_switch = mask;
					}
					rxsec += r;
					if ( rxsec > rxsec_limit )
					{
						printf("[XMPPProxyStream: %d] lock stream, recieved %d bytes per second\n", fd, rxsec);
						this->lock();
						proxy->daemon->setTimer(tm+1, unblock, this);
					}
			}
		}
		mutex.unlock();
	}
}

/**
* Записать кусок
* @return TRUE всё записал, FALSE ничего не записал или только часть
*/
bool XMPPProxyStream::writeChunk()
{
	ssize_t r = write(buffer + written, len - written);
	if ( r > 0 )
	{
		len -= r;
		written += r;
		tx += r;
		return len == 0;
	}
	return false;
}

/**
* Событие готовности к записи
*
* Вызывается, когда в поток готов принять
* данные для записи без блокировки
*/
void XMPPProxyStream::onWrite()
{
	if ( writeChunk() )
	{
		ready_write = false;
		if ( pair->mutex.lock() )
		{
			pair->ready_read = true;
			proxy->daemon->modifyObject(pair);
			
			pair->mutex.unlock();
		}
	}
}

/**
* Пир (peer) закрыл поток.
*
* Мы уже ничего не можем отправить в ответ,
* можем только корректно закрыть соединение с нашей стороны.
*/
void XMPPProxyStream::onPeerDown()
{
	printf("#%d: [XMPPProxyStream: %d] peer down\n", getWorkerId(), fd);
	::shutdown(pair->fd, SHUT_WR);
	pair = 0;
	proxy->daemon->removeObject(this);
}

/**
* Пир (peer) закрыл поток.
*
* Мы уже ничего не можем отправить в ответ,
* можем только корректно закрыть соединение с нашей стороны.
*/
void XMPPProxyStream::onShutdown()
{
	printf("#%d: [XMPPProxyStream: %d] shutdown\n", getWorkerId(), fd);
	::shutdown(pair->fd, SHUT_RDWR);
	proxy->daemon->removeObject(this);
	pair = 0;
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void XMPPProxyStream::onTerminate()
{
	printf("#%d: [XMPPProxyStream: %d] onTerminate\n", getWorkerId(), fd);
	::shutdown(fd, SHUT_RDWR);
	::shutdown(pair->fd, SHUT_RDWR);
}
