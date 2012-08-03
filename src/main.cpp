
// системные вызовы
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pwd.h>

// стандартная библиотека C
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// C++ STL
#include <iostream>

// наши классы
#include <nanosoft/object.h>
#include <nanosoft/netdaemon.h>
#include <logs.h>
#include <xml_tag.h>
#include <configfile.h>
#include <xmppserver.h>
#include <xep0114listener.h>
#include <s2slistener.h>
#include <serverstatus.h>
#include <functions.h>
#include <nanosoft/switchlogserver.h>
#include <dcbot.h>

#define PATH_PID (PATH_VAR "/run/maward.pid")
#define PATH_STATUS (PATH_VAR "/run/maward.status")

#define PATH_ACCESS_LOG (PATH_VAR "/log/maward-access.log")
#define PATH_ERROR_LOG (PATH_VAR "/log/maward-error.log")

#define PATH_CONFIG (PATH_ETC "/mawar/main.xml")
#define PATH_C2S_IPCONFIG (PATH_ETC "/mawar/c2s-ipconfig")
#define PATH_S2S_IPCONIFG (PATH_ETC "/mawar/s2s-ipconfig")

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

/**
* Зачистка: удалиь pid, unix-сокеты и пр.
*/
void cleanup()
{
	unlink(PATH_PID);
	unlink(PATH_STATUS);
}

int main(int argc, const char **argv)
{
	// Конфигурация
	ConfigFile *config = new ConfigFile(PATH_CONFIG);
	
	// открыть лог файлы до смены пользователя
	open_access_log(PATH_ACCESS_LOG);
	open_error_log(PATH_ERROR_LOG);
	FILE *fpid = fopen(PATH_PID, "w");
	
	// установить лимиты до смены пользователя
	if ( getuid() == 0 )
	{
		struct rlimit rl;
		rl.rlim_cur = config->filesLimit();
		rl.rlim_max = config->filesLimit();
		if ( setrlimit(RLIMIT_NOFILE, &rl) == -1 )
		{
			fprintf(stderr, "setrlimit fault: %s\n", strerror(errno));
		}
	}
	else
	{
		struct rlimit rl;
		if ( getrlimit(RLIMIT_NOFILE, &rl) == -1 )
		{
			fprintf(stderr, "getrlimit fault: %s\n", strerror(errno));
		}
		else
		{
			rl.rlim_cur = config->filesLimit();
			if ( config->filesLimit() > rl.rlim_max )
			{
				fprintf(stderr, "only root can increase over hard limit (RLIMIT_NOFILE)\ntry to increase up to hard limit (%d)\n", rl.rlim_max);
				rl.rlim_cur = rl.rlim_max;
			}
			if ( setrlimit(RLIMIT_NOFILE, &rl) == -1 )
			{
				fprintf(stderr, "setrlimit fault: %s\n", strerror(errno));
			}
		}
	}
	
	// если запущены под root, то сменить пользователя
	if ( getuid() == 0 && config->user() != "" )
	{
		fprintf(stderr, "Trying to switch to user: ");
		fprintf(stderr, config->user());
		fprintf(stderr, "\n");
		struct passwd *pw = getpwnam(config->user());
		if(pw)
		{
			if(setgid(pw->pw_gid) != 0) fprintf(stderr, "Failed to setgid!\n");
			if(setuid(pw->pw_uid) != 0 ) fprintf(stderr, "Failed to setuid!\n");
		}
		else
		{
			fprintf(stderr, "user %s not found\n", config->user());
		}
	}
	
	if ( argc > 1 && strcmp(argv[1], "-d") == 0 )
	{
		printf("try fork\n");
		pid_t parpid;
		if((parpid = fork()) < 0) {
			mawarError("Failed to fork!", 99);
		}
		else if(parpid != 0) {
			exit(0); // успешно создан дочерний процесс, основной можно завершить
		}
		setsid();
	}
	
	// после форка записать pid
	if ( fpid )
	{
		fprintf(fpid, "%d", getpid());
		fclose(fpid);
		fpid = 0;
	}
	
	// демон управляющий воркерами вводом-выводом
	struct rlimit rl;
	if ( getrlimit(RLIMIT_NOFILE, &rl) == -1 )
	{
		fprintf(stderr, "getrlimit fault: %s\n", strerror(errno));
		return 1;
	}
	printf("files limit: %d\n", rl.rlim_cur);
	NetDaemon daemon(rl.rlim_cur, config->getOutputBuffers());
	
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
	printf("[main] loading virtual hosts\n");
	for(ATXmlTag *vhost = config->firstHost(); vhost; vhost = config->nextHost(vhost)) {
		printf("[main] load vhost: %s\n", vhost->getAttribute("name").c_str());
		server->addHost(vhost->getAttribute("name"), vhost);
	}
	printf("[main] virtual hosts loaded\n");
	
	// добавляем виртуальные хосты
	printf("[main] loading xmpp groups\n");
	for(ATXmlTag *vhost = config->firstGroupsHost(); vhost; vhost = config->nextGroupsHost(vhost)) {
		printf("[main] load xmpp groups: %s\n", vhost->getAttribute("name").c_str());
		server->addXMPPGroups(vhost->getAttribute("name"), vhost);
	}
	printf("[main] xmpp groups loaded\n");
	
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
	
	for(TagHelper swlog_config = config->getSwitchLogFirst(); swlog_config; swlog_config = config->getSwitchLogNext(swlog_config))
	{
		string addr = swlog_config->getAttribute("bind-address", "0.0.0.0");
		string port = swlog_config->getAttribute("bind-port", "514");
		printf("new switch log config found: %s:%s\n", addr.c_str(), port.c_str());
		
		nanosoft::ptr<SwitchLogServer> swlog = new SwitchLogServer();
		
		// Подключаемся к БД
		TagHelper storage = swlog_config["storage"];
		string server = storage["server"];
		if(server.substr(0, 5) == "unix:" ) {
			if ( ! swlog->db.connectUnix(server.substr(5), storage["database"], storage["username"], storage["password"]) )
			{
				fprintf(stderr, "[SwitchLogServer] cannot connect to database\n");
			}
		} else {
			if ( ! swlog->db.connect(server, storage["database"], storage["username"], storage["password"]) )
			{
				fprintf(stderr, "[SwitchLogServer] cannot connect to database\n");
			}
		}
		
		swlog->bind(
			addr.c_str(),
			port.c_str()
		);
		
		daemon.addObject(swlog);
	}
	
	// консоль управления сервером
	//MyConsole console(&daemon, 0);
	//daemon.addObject(&console);
	
	DCBot *ptr = new DCBot();
	
	
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigaction(SIGTERM, &sa, 0);
	sigaction(SIGHUP, &sa, 0);
	sigaction(SIGINT, &sa, 0);
	
	// запускаем демона
	fprintf(stderr, "[main] run daemon\n");
	daemon.run();
	fprintf(stderr, "[main] daemon exited\n");
	
	cleanup();
	
	return 0;
}

