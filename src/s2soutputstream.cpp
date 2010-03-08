
#include <s2soutputstream.h>

/**
* Конструктор потока
*/
S2SOutputStream::S2SOutputStream(XMPPServer *srv, int sock): XMPPStream(server, sock)
{
}

/**
* Деструктор потока
*/
S2SOutputStream::~S2SOutputStream()
{
}

/**
* Событие: начало потока
*/
void S2SOutputStream::onStartStream(const std::string &name, const attributes_t &attributes)
{
}

/**
* Событие: конец потока
*/
void S2SOutputStream::onEndStream()
{
}

/**
* Обработчик станзы
*/
void S2SOutputStream::onStanza(Stanza stanza)
{
}

/**
* Событие закрытие соединения
*
* Вызывается если peer закрыл соединение
*/
void S2SOutputStream::onShutdown()
{
}

/**
* Сигнал завершения работы
*
* Объект должен закрыть файловый дескриптор
* и освободить все занимаемые ресурсы
*/
void S2SOutputStream::onTerminate()
{
}
