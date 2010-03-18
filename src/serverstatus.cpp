
#include <serverstatus.h>
#include <xmppserver.h>
#include <stdio.h>
#include <sys/socket.h>

/**
* Конструктор сервера статистики
*/
ServerStatus::ServerStatus(class XMPPServer *srv): server(srv)
{
}

/**
* Деструктор сервера статистики
*/
ServerStatus::~ServerStatus()
{
}

/**
* Выдать статистику в указанный файл
*
* @param f файл (сокет) в который надо записать статистику
*/
void ServerStatus::handleStatus(FILE *status)
{
	fprintf(status, "maward status\n");
	fprintf(status, "workers: %d\n", server->daemon->getWorkerCount()+1);
	fprintf(status, "sockets: %d\n", server->daemon->getObjectCount());
}

/**
* Обработчик события: подключение s2s
*
* thread-safe
*/
void ServerStatus::onAccept()
{
	int sock = accept();
	if ( sock )
	{
		// сразу говорим, что читать ничего не будем, только писать
		shutdown(sock, SHUT_RD);
		FILE *f = fdopen(sock, "w");
		handleStatus(f);
		fclose(f);
	}
}

/**
* Сигнал завершения работы
*/
void ServerStatus::onTerminate()
{
	server->daemon->removeObject(this);
}
