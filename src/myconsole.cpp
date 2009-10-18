
#include <myconsole.h>
#include <string>
#include <iostream>

using namespace std;

std::string trim(const std::string &s) {
	const char *left = s.c_str();
	const char *right = left + s.length() - 1;
	while(isspace(*left) && left < right) left++;
	while(isspace(*right) && left < right ) right--;
	return std::string(left, right - left + 1);
}

/**
* Конструктор потока
*/
MyConsole::MyConsole(NetDaemon *d, int sock): AsyncStream(sock), daemon(d)
{
}

/**
* Деструктор потока
*/
MyConsole::~MyConsole()
{
}

/**
* Событие готовности к чтению
*
* Вызывается когда в потоке есть данные,
* которые можно прочитать без блокирования
*/
void MyConsole::onRead()
{
	char buf[4096];
	int r = read(buf, sizeof(buf));
	string cmd = trim(string(buf, r));
	if ( cmd == "quit" )
	{
		daemon->terminate();
	}
}

/**
* Событие готовности к записи
*
* Вызывается, когда в поток готов принять
* данные для записи без блокировки
*/
void MyConsole::onWrite()
{
}

/**
* Событие закрытие соединения
*
* Вызывается если peer закрыл соединение
*/
void MyConsole::onShutdown()
{
	cout << "stdin closed =)" << endl;
}

