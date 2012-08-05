
#include <dcbot.h>
#include <xmppserver.h>
#include <nanosoft/asyncdns.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

using namespace std;

/**
* Конструктор потока
*/
DCBot::DCBot(XMPPServer *srv): AsyncStream(0)
{
	server = srv;
	int fd = socket(PF_INET, SOCK_STREAM, 0);
	setFd(fd);
	chunk_type = UNKNOWN;
	chunk_len = 0;
}

/**
* Деструктор потока
*/
DCBot::~DCBot()
{
}

/**
* Резолвер s2s хоста, запись A (IPv4)
*/
void DCBot::on_s2s_output_a4(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data)
{
	printf("on_s2s_output_a4\n");
	DCBot *p = static_cast<DCBot *>(data);
	
	if ( result && result->dnsa4_nrr >= 1 )
	{
		struct sockaddr_in target;
		memset(&target, 0, sizeof(target));
		target.sin_family = AF_INET;
		target.sin_port = htons(p->port);
		memcpy(&target.sin_addr, &result->dnsa4_addr[0], sizeof(result->dnsa4_addr[0]));
		
		p->state = CONNECTING;
		
		if ( ::connect(p->getFd(), (struct sockaddr *)&target, sizeof( struct sockaddr )) == 0 || errno == EINPROGRESS || errno == EALREADY )
		{
			p->state = CONNECTED;
			p->server->daemon->addObject(p);
			p->onConnect();
			return;
		}
		printf("connect fault\n");
		return;
	}
	printf("resolve fault\n");
}

/**
* Обработчик готовности подключения
*/
void DCBot::onConnect()
{
}

/**
* Обработчик прочитанных данных
*/
void DCBot::onRead(const char *data, size_t len)
{
	const char *end = data + len;
	while ( data < end )
	{
		if ( chunk_type == COMMAND )
		{
			if ( *data == '|' )
			{
				chunk[chunk_len] = 0;
				parseCommand(chunk, chunk_len);
				chunk_len = 0;
				chunk_type = UNKNOWN;
			}
			else
			{
				if ( chunk_len < CHUNK_MAXLEN )
				{
					chunk[chunk_len] = *data;
					chunk_len++;
				}
				else
				{
					// TODO
				}
			}
			
			data++;
			continue;
		}
		
		if ( chunk_type == CHAT )
		{
			if ( *data == '|' )
			{
				chunk[chunk_len] = 0;
				parseChatMessage(chunk, chunk_len);
				chunk_len = 0;
				chunk_type = UNKNOWN;
			}
			else
			{
				if ( chunk_len < CHUNK_MAXLEN )
				{
					chunk[chunk_len] = *data;
					chunk_len++;
				}
				else
				{
					// TODO
				}
			}
			
			data++;
			continue;
		}
		
		if ( *data == '$' )
		{
			if ( chunk_len > 0 )
			{
				chunk[chunk_len] = 0;
				handleUnknownChunk(chunk, chunk_len);
			}
			chunk_len = 0;
			chunk_type = COMMAND;
			
			data++;
			continue;
		}
		
		if ( *data == '<' )
		{
			if ( chunk_len > 0 )
			{
				chunk[chunk_len] = 0;
				handleUnknownChunk(chunk, chunk_len);
			}
			chunk_len = 0;
			chunk_type = CHAT;
			
			data++;
			continue;
		}
		
		if ( chunk_len < CHUNK_MAXLEN )
		{
			chunk[chunk_len] = *data;
			chunk_len++;
		}
		else
		{
			// TODO
		}
		
		data++;
	}
}

/**
* Парсер комманды
*/
void DCBot::parseCommand(const char *data, size_t len)
{
#ifdef DUMP_IO
	std::string dump_cmd(data, len);
	printf("DUMP DC++ COMMAND[%d]: \033[22;31m%s\033[0m\n", getFd(), dump_cmd.c_str());
#endif
	
	char buf[CHUNK_SIZE];
	const char *end = data + len;
	const char *p = data;
	char *cmd = buf;
	while ( p < end && *p != ' ' )
	{
		*cmd++ = *p++;
	}
	*cmd = 0;
	while ( *p == ' ' ) p++;
	findCommand(buf, p, end - p);
}

/**
* Поиск комманды
*/
void DCBot::findCommand(const char *command, const char *args, size_t args_len)
{
	if ( strcmp(command, "Lock") == 0 )
	{
		handleLockCommand(args, args_len);
		return;
	}
	
	if ( strcmp(command, "Hello") == 0 )
	{
		handleHelloCommand(args, args_len);
		return;
	}
	
	if ( strcmp(command, "To:") == 0 )
	{
		handleToCommand(args, args_len);
		return;
	}
	
	handleUnknownCommand(command, args, args_len);
}

/**
* Обработчик команды Lock
*/
void DCBot::handleLockCommand(const char *args, size_t len)
{
	printf("DCBot: Lock command: %s\n", args);
	
	sendCommand("Supports", "NoGetINFO NoHello UserIP2");
	sendCommand("Key", "?");
	sendCommand("ValidateNick", "%s", nick.c_str());
}

/**
* Обработчик команды Hello
*/
void DCBot::handleHelloCommand(const char *args, size_t len)
{
	printf("DCBot: Hello command: %s\n", args);
	
	sendCommand("Version", "1,0091");
	
	string descr = "test";
	string tag = "<++ V:x,M:x,H:x/y/z,S:x,O:x>";
	
	sendCommand("MyINFO", "$ALL %s %s%s$ $'\x01'$$0$", nick.c_str(), descr.c_str(), tag.c_str());
	sendCommand("GetNickList");
}

/**
* Обработчик комманды $To
*/
void DCBot::handleToCommand(const char *args, size_t len)
{
	const char *from = strstr(args, "From:");
	const char *message = strchr(args, '$');
	const char *end = args + len;
	char login[CHUNK_SIZE];
	login[0] = 0;
	if ( from )
	{
		from += 5;
		while ( *from == ' ' ) from++;
		
		char *p = login;
		while ( from < end && *from != ' ' && *from != '$' )
		{
			*p++ = *from++;
		}
		*p = 0;
	}
	
	if ( message )
	{
		message++;
		handlePrivateMessage(login, message, end - message);
	}
}

/**
* Обработчик личного сообщения
*/
void DCBot::handlePrivateMessage(const char *from, const char *message, size_t message_len)
{
	printf("Private message from '%s': %s\n", from, message);
}

/**
* Обработчик неизвестной комманды
*/
void DCBot::handleUnknownCommand(const char *command, const char *args, size_t args_len)
{
	printf("DCBot: unknown command: %s %s\n", command, args);
}

/**
* Парсер сообщения из чата
*/
void DCBot::parseChatMessage(const char *data, size_t len)
{
#ifdef DUMP_IO
	std::string message(data, len);
	printf("DUMP DC++ CHAT MESSAGE[%d]: \033[22;31m%s\033[0m\n", getFd(), message.c_str());
#endif
	
	char buf[CHUNK_SIZE];
	const char *end = data + len;
	const char *p = data;
	char *login = buf;
	while ( p < end && *p != '>' )
	{
		*login++ = *p++;
	}
	*login = 0;
	
	if ( *p == '>' ) p++;
	while ( *p == ' ' ) p++;
	
	handleChatMessage(buf, p, end - p);
}

/**
* Обработчик получения сообщения из чата
*/
void DCBot::handleChatMessage(const char *login, const char *message, size_t message_len)
{
	printf("DC++ chat message from '%s': %s\n", login, message);
}

/**
* Обработчик получения блока неизвестного типа
*/
void DCBot::handleUnknownChunk(const char *data, size_t len)
{
#ifdef DUMP_IO
	std::string message(data, len);
	printf("DUMP DC++ UNKNOWN MESSAGE[%d]: \033[22;31m%s\033[0m, len: %d\n", getFd(), message.c_str(), len);
#endif
}

/**
* Пир (peer) закрыл поток.
*
* Мы уже ничего не можем отправить в ответ,
* можем только корректно закрыть соединение с нашей стороны.
*/
void DCBot::onPeerDown()
{
	terminate();
}

/**
* Сигнал завершения работы
*
* Сервер решил закрыть соединение, здесь ещё есть время
* корректно попрощаться с пиром (peer).
*/
void DCBot::onTerminate()
{
	server->daemon->removeObject(this);
}

/**
* Установить соединение
*/
void DCBot::connect(const char *host, int host_port)
{
	port = host_port;
	struct sockaddr_in target;
	memset(&target, 0, sizeof(target));
	if ( inet_pton(AF_INET, host, &(target.sin_addr)) == 1 )
	{
		target.sin_family = AF_INET;
		target.sin_port = htons(port);
		state = CONNECTING;
		
		if ( ::connect(getFd(), (struct sockaddr *)&target, sizeof( struct sockaddr )) == 0 || errno == EINPROGRESS || errno == EALREADY )
		{
			state = CONNECTED;
			server->daemon->addObject(this);
			onConnect();
		}
	}
	
	// Резолвим DNS записи сервера
	state = RESOLVING;
	server->adns->a4(host, on_s2s_output_a4, this);
}

/**
* Послать комманду
*/
bool DCBot::sendCommand(const char *command, const char *fmt, ...)
{
	char cmd_buf[CHUNK_SIZE];
	char args_buf[CHUNK_SIZE];
	int len;
	
	// если fmt пуст, то отправить только имя комманды
	if ( fmt == 0 || *fmt == 0 )
	{
		args_buf[0] = 0;
	}
	else
	{
		va_list args;
		va_start(args, fmt);
		args_buf[0] = ' ';
		len = vsnprintf(args_buf+1, sizeof(args_buf)-2, fmt, args);
		va_end(args);
		if ( len > 0 ) args_buf[len+1] = 0;
		else args_buf[0] = 0;
	}
	
	len = snprintf(cmd_buf, sizeof(cmd_buf)-2, "$%s%s", command, args_buf);
	cmd_buf[len] = '|';
	put(cmd_buf, len+1);
}
