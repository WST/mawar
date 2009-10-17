
#include <iostream>
#include <nanosoft/netdaemon.h>
#include <xmppserver.h>
#include <myconsole.h>
#include <nanosoft/gsaslserver.h>
#include <xml_tag.h>
#include <configfile.h>

using namespace std;

int main()
{
	// Конфигурация
	ConfigFile *config = new ConfigFile("config.xml");
	
	// демон управляющий воркерами вводом-выводом
	NetDaemon daemon(config->c2s_sessions());
	daemon.setWorkerCount(config->workers());
	
	// XMPP-сервер
	XMPPServer server(&daemon);
	
	// подключемся к c2s-порту из конфига
	server.bind(config->c2s());
	
	// не более 10 ожидающих соединений
	server.listen(10);
	
	// добавляем сервер в демона
	daemon.addObject(&server);
	
	// консоль управления сервером
	//MyConsole console(&daemon, 0);
	//daemon.addObject(&console);
	
	// запускаем демона
	return daemon.run();
}
