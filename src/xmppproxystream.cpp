
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
	fprintf(stderr, "connect() fault\n");
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
	fprintf(stderr, "#%d: [XMPPProxyStream: %d] event mask: %X\n", getWorkerId(), fd, mask);
	return mask;
}

/**
* Таймер-функция разблокировки потока
* @param data указатель на XMPPProxyStream который надо разблочить
*/
void XMPPProxyStream::unblock(int wid, void *data)
{
	XMPPProxyStream *stream = static_cast<XMPPProxyStream *>(data);
	fprintf(stderr, "#%d [XMPPProxyStream: %d] unlock stream\n", wid, stream->fd);
	stream->rxsec = 0;
	stream->finish(); // костыль однако
}

#include <stdlib.h>

/**
* Событие готовности к чтению
*
* Вызывается когда в потоке есть данные,
* которые можно прочитать без блокирования
*/
void XMPPProxyStream::onRead()
{
	mutex.lock();
	
	int x = random();
	fprintf(stderr, "[%d - %d] read enter\n", fd, x);
	ssize_t r = read(pair->buffer, sizeof(pair->buffer));
	fprintf(stderr, "#%d: [XMPPProxyStream #%d] read from: %d, size: %d\n", getWorkerId(), x, fd, r);
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
					fprintf(stderr, "#%d recieved %d bytes per second, block for a while: %d\n", x, rxsec, fd);
					lock(); // костыль однако
					proxy->daemon->setTimer(tm+1, unblock, this);
				}
			
		}
		
	}
	getEventsMask();
	fprintf(stderr, "[%d - %d] read leave\n\n", fd, x);
	
	mutex.unlock();
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
		fprintf(stderr, "#%d: [XMPPProxyStream] written to: %d, size: %d, remain: %d\n", getWorkerId(), fd, r, len - r);
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
		pair->mutex.lock();
			pair->ready_read = true;
			proxy->daemon->modifyObject(pair);
		pair->mutex.unlock();
	}
}

/**
* Блокировка потка от удаления
*/
void XMPPProxyStream::lock()
{
	//mutex.lock();
		int x = --finish_count;
		fprintf(stderr, "#%d: [XMPPProxyStream: %d] lock %d\n", getWorkerId(), fd, x);
	//mutex.unlock();
}

/**
* Финализация
*/
void XMPPProxyStream::finish()
{
	int x;
	mutex.lock();
		x = ++finish_count;
		fprintf(stderr, "#%d: [XMPPProxyStream: %d] finish %d\n", getWorkerId(), fd, x);
	mutex.unlock();
	if ( x <= 0 )
	{
		proxy->daemon->modifyObject(this);
	}
	if ( x == 1 )
	{
		::shutdown(pair->fd, SHUT_RDWR);
		
		if ( client )
		{
			fprintf(stdlog, "%s [proxyd] disconnect from: %s, rx: %lld, tx: %lld\n", logtime().c_str(), remoteIP.c_str(), rx, tx);
		}
	}
	if ( x == 2 )
	{
		//proxy->daemon->removeObject(this);
		delete this;
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
	fprintf(stderr, "#%d: [XMPPProxyStream: %d] peer down\n", getWorkerId(), fd);
	::shutdown(pair->fd, SHUT_RDWR);
	proxy->daemon->removeObject(this);
	finish();
	pair->finish();
}

/**
* Пир (peer) закрыл поток.
*
* Мы уже ничего не можем отправить в ответ,
* можем только корректно закрыть соединение с нашей стороны.
*/
void XMPPProxyStream::onShutdown()
{
	fprintf(stderr, "#%d: [XMPPProxyStream: %d] shutdown\n", getWorkerId(), fd);
	::shutdown(pair->fd, SHUT_RDWR);
	proxy->daemon->removeObject(this);
	terminate();
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void XMPPProxyStream::onTerminate()
{
	fprintf(stderr, "#%d: [XMPPProxyStream: %d] onTerminate\n", getWorkerId(), fd);
	::shutdown(fd, SHUT_RDWR);
	::shutdown(pair->fd, SHUT_RDWR);
}
