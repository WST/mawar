
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
* Таблица перекодировки из cp1251 в UTF-16
*/
static const int cp1251_table[256] =
{
	0,1,2,3,4,5,6,7,8,9,0xa,0xb,0xc,0xd,0xe,0xf,0x10,0x11,0x12,0x13,0x14,0x15,0x16,
	0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,
	0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,0x33,0x34,0x35,0x36,
	0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,0x40,0x41,0x42,0x43,0x44,0x45,0x46,
	0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,0x51,0x52,0x53,0x54,0x55,0x56,
	0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,0x61,0x62,0x63,0x64,0x65,0x66,
	0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,0x70,0x71,0x72,0x73,0x74,0x75,0x76,
	0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,0x402,0x403,0x201a,0x453,0x201e,
	0x2026,0x2020,0x2021,0x20ac,0x2030,0x409,0x2039,0x40a,0x40c,0x40b,0x40f,0x452,
	0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0,0x2122,0x459,0x203a,0x45a,
	0x45c,0x45b,0x45f,0xa0,0x40e,0x45e,0x408,0xa4,0x490,0xa6,0xa7,0x401,0xa9,0x404,
	0xab,0xac,0xad,0xae,0x407,0xb0,0xb1,0x406,0x456,0x491,0xb5,0xb6,0xb7,0x451,
	0x2116,0x454,0xbb,0x458,0x405,0x455,0x457,0x410,0x411,0x412,0x413,0x414,0x415,
	0x416,0x417,0x418,0x419,0x41a,0x41b,0x41c,0x41d,0x41e,0x41f,0x420,0x421,0x422,
	0x423,0x424,0x425,0x426,0x427,0x428,0x429,0x42a,0x42b,0x42c,0x42d,0x42e,0x42f,
	0x430,0x431,0x432,0x433,0x434,0x435,0x436,0x437,0x438,0x439,0x43a,0x43b,0x43c,
	0x43d,0x43e,0x43f,0x440,0x441,0x442,0x443,0x444,0x445,0x446,0x447,0x448,0x449,
	0x44a,0x44b,0x44c,0x44d,0x44e,0x44f
};

/**
* Обработчик прочитанных данных
*/
void DCBot::onRead(const char *data, size_t len)
{
	char output[CHUNK_SIZE];
	char *p = output;
	char *out_end = output + sizeof(output);
	
	const unsigned char *input = reinterpret_cast<const unsigned char*>(data);
	const unsigned char *end = input + len;
	
	while ( input < end )
	{
		int c = cp1251_table[ *input ];
		if ( c < 0x80 )
		{
			*p++ = c;
		}
		else
		{
			if ( c < 0x800 )
			{
				*p++ = (c >> 6) | 0xc0;
				*p++ = (c & 0x3f) | 0x80;
			}
		}
		
		input++;
		
		if ( out_end - p < 2 )
		{
			onReadUTF8(output, p - output);
			p = output;
		}
	}
	
	if ( p - output > 0 )
	{
		onReadUTF8(output, p - output);
	}
}

/**
* Обработчик прочитанных данных в кодировке UTF-8
*/
void DCBot::onReadUTF8(const char *data, size_t len)
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
	
	if ( strcmp(command, "GetPass") == 0 )
	{
		handleGetPassCommand(args, args_len);
		return;
	}
	
	if ( strcmp(command, "LogedIn") == 0 )
	{
		handleLogedInCommand(args, args_len);
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
	char lock[CHUNK_SIZE];
	const char *end = args + len;
	char *plock = lock;
	while ( args < end && *args != ' ' )
	{
		*plock++ = *args++;
	}
	*plock = 0;
	
	sendCommand("Supports", "NoGetINFO NoHello UserIP2");
	sendKey(lock, plock - lock);
	sendCommand("ValidateNick", "%s", nick.c_str());
}

/**
* Выдать команду $Key
*/
void DCBot::sendKey(const char *lock, size_t len)
{
	char key[CHUNK_SIZE];
	char newkey[CHUNK_SIZE];
	
	int i;
	for(i = 1; i < len; ++i)
		key[i] = lock[i] ^ lock[i-1];
	
	key[0] = lock[0] ^ lock[len-1] ^ lock[len-2] ^ 5;
	
	for(i = 0; i < len; ++i)
		key[i] = ((key[i]<<4) & 0xF0) | ((key[i]>>4) & 0x0F);
	
	char *newkey_p = newkey;
	for(i = 0; i < len; ++i)
	{
		switch(key[i])
		{
			case 0:
			case 5:
			case 36:
			case 96:
			case 124:
			case 126:
				sprintf(newkey_p, "/%%DCN%03d%%/", key[i]);
				newkey_p += 10;
				break;
			default:
				*newkey_p = key[i];
				++newkey_p;
		}
	}
	*newkey_p = '\0';
	const char *p = newkey;
	while ( p < newkey_p ) printf("key[%d]: %02X\n", (p - newkey), (*p++) &0xFF);
	
	int r = snprintf(key, CHUNK_MAXLEN, "$Key %s", newkey);
	key[r++] = '|';
	put(key, r);
	//sendCommand("Key", "%s", newkey); 
}

/**
* Обработчик команды Hello
*/
void DCBot::handleHelloCommand(const char *args, size_t len)
{
	sendCommand("Version", "1,0091");
	
	string descr = "test";
	string tag = "<++ V:x,M:x,H:x/y/z,S:x,O:x>";
	
	sendCommand("MyINFO", "$ALL %s %s%s$ $'\x01'$$0$", nick.c_str(), descr.c_str(), tag.c_str());
	sendCommand("GetNickList");
}

/**
* Обработчик команды $GetPass
*/
void DCBot::handleGetPassCommand(const char *args, size_t len)
{
	sendCommand("MyPass", "test43");
}

/**
* Обработчик команды $LogedIn
*/
void DCBot::handleLogedInCommand(const char *args, size_t len)
{
	sendCommand("SET", "2 motd %s", "test");
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
	sendPrivateMessage(from, "hello! товарищ");
}

/**
* Обработчик неизвестной комманды
*/
void DCBot::handleUnknownCommand(const char *command, const char *args, size_t args_len)
{
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
	return putCP1251(cmd_buf, len+1);
}

/**
* Отправить команду установки MOTD (Message Of The Day)
*/
bool DCBot::sendMotd(const char *text)
{
	return sendCommand("SET", "2 motd %s", text);
}

/**
* Отправить личное сообщение
*/
bool DCBot::sendPrivateMessage(const char *to, const char *message)
{
	return sendCommand("To:", "%s From: %s $<%s> %s", to, nick.c_str(), nick.c_str(), message);
}

/**
* Конвертировать из UTF-8 в CP1251 и записать в сокет
*/
bool DCBot::putCP1251(const char *data, size_t len)
{
	char output[CHUNK_SIZE];
	const unsigned char *inp = reinterpret_cast<const unsigned char*>(data);
	const unsigned char *end_in = inp + len;
    char *outp = output;
	
	while ( inp < end_in )
	{
		if ( (*inp & 0x80) == 0 )
		{
			*outp++ = *inp++;
		}
		else
		{
			int count = 0;
			unsigned char mask = 0x80;
			unsigned char x = *inp++;
			
			while ( mask > 0 )
			{
				if ( (x & mask) == 0 ) 
				{
					break;
				}
				mask = mask >> 1;
				count++;
			}
			mask--;
			
			int c = x & mask;
			for(int i = 1; i < count && inp < end_in; i++)
			{
				c = (c << 6) | (*inp & 0x3F);
				inp++;
			}
			
			*outp = '?';
			for(int i = 0; i < 256; i++)
			{
				if ( cp1251_table[i] == c )
				{
					*outp = i;
					break;
				}	
				
			}
			outp++;
		}
		
	}
	
	return put(output, outp-output);
}
