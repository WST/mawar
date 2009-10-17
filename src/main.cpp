
#include <iostream>
#include <nanosoft/netdaemon.h>
#include <xmppserver.h>
#include <myconsole.h>
#include <nanosoft/gsaslserver.h>
#include <xml_tag.h>
#include <attagparser.h>

using namespace std;

int main()
{
	ATXmlTag *config = parse_xml_file("config.xml");
	if ( config )
	{
		// TODO
	}
	delete config;
	
	// демон управляющий воркерами вводом-выводом
	NetDaemon daemon(10000);
	daemon.setWorkerCount(2);
	
	// XMPP-сервер
	XMPPServer server(&daemon);
	
	// подключемся к 5222 порту - подключения клиентов
	server.bind(5222);
	
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
