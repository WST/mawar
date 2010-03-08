
#include <s2sinputstream.h>
#include <stanza.h>

/**
* Конструктор потока
*/
S2SInputStream::S2SInputStream(XMPPServer *srv, int sock): XMPPStream(srv, sock)
{
}

/**
* Деструктор потока
*/
S2SInputStream::~S2SInputStream()
{
}

/**
* Событие: начало потока
*/
void S2SInputStream::onStartStream(const std::string &name, const attributes_t &attributes)
{
}

/**
* Событие: конец потока
*/
void S2SInputStream::onEndStream()
{
}

/**
* Обработчик станзы
*/
void S2SInputStream::onStanza(Stanza stanza)
{
}

/**
* Событие закрытие соединения
*
* Вызывается если peer закрыл соединение
*/
void S2SInputStream::onShutdown()
{
}

/**
* Сигнал завершения работы
*
* Объект должен закрыть файловый дескриптор
* и освободить все занимаемые ресурсы
*/
void S2SInputStream::onTerminate()
{
}
