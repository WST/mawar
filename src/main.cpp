
#include <iostream>
#include <nanosoft/object.h>
#include <nanosoft/netdaemon.h>
#include <xmppserver.h>
#include <xep0114listener.h>
#include <s2slistener.h>
#include <serverstatus.h>
#include <myconsole.h>
#include <stanzabuffer.h>
#include <xml_tag.h>
#include <configfile.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <cstdlib>


#include <taghelper.h>
#include <nanosoft/asyncdns.h>

using namespace std;

// XMPP-сервер
nanosoft::ptr<XMPPServer> server;

void on_signal(int sig)
{
	switch ( sig )
	{
	case SIGTERM:
	case SIGINT:
		server->onSigTerm();
		break;
	case SIGHUP:
		server->onSigHup();
		break;
	default:
		// обычно мы не должны сюда попадать,
		// если попали, значит забыли добавить обработчик
		cerr << "[main] signal: " << sig << endl;
	}
}

int main()
{
	// Конфигурация
	ConfigFile *config = new ConfigFile("config.xml");
	
	fprintf(stderr, "Trying to switch to user: ");
	fprintf(stderr, config->user());
	fprintf(stderr, "\n");
	struct passwd *pw = getpwnam(config->user());
	if(pw) {
		if(setuid(pw->pw_uid) != 0 ) fprintf(stderr, "Failed to setuid!\n");
		if(setgid(pw->pw_gid) != 0) fprintf(stderr, "Failed to setgid!\n");
	}
	pid_t parpid;
	if((parpid = fork()) < 0) {
		printf("\nFailed to fork!");
		exit(99);
	}
	else if(parpid != 0) {
		exit(0); // успешно создан дочерний процесс, основной можно завершить
	}
	setsid();
	
	printf("test\n");
	// демон управляющий воркерами вводом-выводом
	NetDaemon daemon(config->c2s_sessions());
	StanzaBuffer buf(config->c2s_sessions(), 1000);
	
	// устанавливаем скорректированное число воркеров
	daemon.setWorkerCount(config->workers() - 1);
	
	// XMPP-сервер
	server = new XMPPServer(&daemon);
	server->config = config;
	
	// подключемся к c2s-порту из конфига
	server->bind(config->c2s());
	
	// не более 10 ожидающих соединений
	server->listen(10);
	
	// добавляем виртуальные хосты
	cerr << "#0: [main] loading virtual hosts..." << endl;
	for(VirtualHostConfig vhost = config->firstHost(); vhost; vhost = config->nextHost(vhost))
	{
		cerr << "#0: [main] load vhost: " << vhost.hostname() << endl;
		server->addHost(vhost.hostname(), vhost);
	}
	cerr << "#0: [main] virtual hosts loaded" << endl;
	
	// асинхронный резолвер
	nanosoft::ptr<AsyncDNS> dns = new AsyncDNS(&daemon);
	daemon.addObject(dns);
	server->adns = dns;
	
	// добавляем сервер в демона
	daemon.addObject(server);
	
	int port = config->xep0114();
	if ( port > 0 ) {
		nanosoft::ptr<XEP0114Listener> xep0114 = new XEP0114Listener(server.getObject());
		xep0114->bind(port);
		xep0114->listen(10);
		daemon.addObject(xep0114);
	}
	
	port = config->s2s();
	if ( port > 0 )
	{
		server->s2s = new S2SListener(server.getObject());
		server->s2s->bind(port);
		server->s2s->listen(10);
		daemon.addObject(server->s2s);
	}
	
	string path = config->status();
	if ( path != "" )
	{
		nanosoft::ptr<ServerStatus> status = new ServerStatus(server.getObject());
		status->bind(path.c_str());
		status->listen(1);
		daemon.addObject(status);
	}
	
	// консоль управления сервером
	//MyConsole console(&daemon, 0);
	//daemon.addObject(&console);
	
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigaction(SIGTERM, &sa, 0);
	sigaction(SIGHUP, &sa, 0);
	sigaction(SIGINT, &sa, 0);
	
	// запускаем демона
	fprintf(stderr, "#0: [main] run daemon\n");
	daemon.run();
	fprintf(stderr, "#0: [main] daemon exited\n");
	return 0;
}
