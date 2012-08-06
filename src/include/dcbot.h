#ifndef MAWAR_DCBOT_H
#define MAWAR_DCBOT_H

#include <nanosoft/asyncstream.h>
#include <nanosoft/object.h>
#include <string>

class XMPPServer;

#define CHUNK_SIZE 4096
#define CHUNK_MAXLEN (CHUNK_SIZE-1)

/**
* Класс XMPP-поток
*/
class DCBot: public AsyncStream
{
protected:
	/**
	* Порт к которому нужно подключиться
	*/
	int port;
	
	/**
	* Состояние s2s-домена
	*/
	enum {
		/**
		* Резолвим DNS
		*/
		RESOLVING,
		
		/**
		* Подключаемся к серверу
		*/
		CONNECTING,
		
		/**
		* Подключены
		*/
		CONNECTED
	} state;
	
	/**
	* Тип текущего блока
	*/
	enum {
		/**
		* Команда
		*/
		COMMAND,
		
		/**
		* Сообщение чата
		*/
		CHAT,
		
		/**
		* Неизвестно
		*/
		UNKNOWN
	} chunk_type;
	
	/**
	* Буфер блока
	*/
	char chunk[CHUNK_SIZE];
	
	/**
	* Размер блока
	*/
	size_t chunk_len;
	
	/**
	* Резолвер s2s хоста, запись A (IPv4)
	*/
	static void on_s2s_output_a4(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data);
	
	/**
	* Обработчик готовности подключения
	*/
	void onConnect();
	
	/**
	* Обработчик прочитанных данных (в кодировке cp1251)
	*/
	virtual void onRead(const char *data, size_t len);
	
	/**
	* Обработчик прочитанных данных в кодировке UTF-8
	*/
	void onReadUTF8(const char *data, size_t len);
	
	/**
	* Парсер комманды
	*/
	void parseCommand(const char *data, size_t len);
	
	/**
	* Поиск комманды
	*/
	void findCommand(const char *command, const char *args, size_t args_len);
	
	/**
	* Обработчик команды $Lock
	*/
	void handleLockCommand(const char *args, size_t len);
	
	/**
	* Выдать команду $Key
	*/
	void sendKey(const char *lock, size_t lock_len);
	
	/**
	* Обработчик команды $Hello
	*/
	void handleHelloCommand(const char *args, size_t len);
	
	/**
	* Обработчик команды $GetPass
	*/
	void handleGetPassCommand(const char *args, size_t len);
	
	/**
	* Обработчик команды $LogedIn
	*/
	void handleLogedInCommand(const char *args, size_t len);
	
	/**
	* Обработчик комманды $To
	*/
	void handleToCommand(const char *args, size_t len);
	
	/**
	* Обработчик личного сообщения
	*/
	void handlePrivateMessage(const char *from, const char *message, size_t message_len);
	
	/**
	* Обработчик неизвестной комманды
	*/
	void handleUnknownCommand(const char *command, const char *args, size_t args_len);
	
	/**
	* Парсер сообщения из чата
	*/
	void parseChatMessage(const char *data, size_t len);
	
	/**
	* Обработчик получения сообщения из чата
	*/
	void handleChatMessage(const char *login, const char *message, size_t message_len);
	
	/**
	* Обработчик получения блока неизвестного типа
	*/
	void handleUnknownChunk(const char *data, size_t len);
	
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
	/**
	* Ссылка на асинхронный резолвер DNS
	*/
	nanosoft::ptr<class XMPPServer> server;
	
	/**
	* Ник под которым надо зайти на хаб
	*/
	std::string nick;
	
	/**
	* Конструктор потока
	*/
	DCBot(XMPPServer *srv);
	
	/**
	* Деструктор потока
	*/
	~DCBot();
	
	/**
	* Установить соединение
	*/
	void connect(const char *host, int host_port);
	
	/**
	* Послать комманду
	*/
	bool sendCommand(const char *command, const char *fmt = 0, ...);
	
	/**
	* Отправить команду установки MOTD (Message Of The Day)
	*/
	bool sendMotd(const char *text);
	
	/**
	* Отправить личное сообщение
	*/
	bool sendPrivateMessage(const char *to, const char *message);
	
	/**
	* Конвертировать из UTF-8 в CP1251 и записать в сокет
	*/
	bool putCP1251(const char *data, size_t len);
};

#endif // MAWAR_DCBOT_H
