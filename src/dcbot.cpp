
#include <dcbot.h>
#include <sys/types.h>
#include <sys/socket.h>

/**
* Конструктор потока
*/
DCBot::DCBot(): AsyncStream(0)
{
	int fd = socket(PF_INET, SOCK_STREAM, 0);
	setFd(fd);
	connect(fd, (struct sockaddr *)&target, sizeof( struct sockaddr )) == 0
}

/**
* Деструктор потока
*/
DCBot::~DCBot()
{
}

/**
* Обработчик прочитанных данных
*/
void DCBot::onRead(const char *data, size_t len)
{
}

/**
* Пир (peer) закрыл поток.
*
* Мы уже ничего не можем отправить в ответ,
* можем только корректно закрыть соединение с нашей стороны.
*/
void DCBot::onPeerDown()
{
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void DCBot::onTerminate()
{
}
