
#include <xmppproxy.h>
#include <nanosoft/object.h>
#include <nanosoft/netdaemon.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <cstdlib>
#include <logs.h>
#include <nanosoft/error.h>

using namespace std;

// XMPP-proxy
nanosoft::ptr<XMPPProxy> proxy_c2s;
nanosoft::ptr<XMPPProxy> proxy_s2s;

#define WHITELIST_PATH "whitelist.txt"

void on_signal(int sig)
{
	switch ( sig )
	{
	case SIGTERM:
	case SIGINT:
		proxy_c2s->onSigTerm();
		break;
	case SIGHUP:
		proxy_c2s->onSigHup();
		break;
	case SIGUSR1:
		proxy_c2s->reloadWhiteList(WHITELIST_PATH);
		break;
	default:
		// обычно мы не должны сюда попадать,
		// если попали, значит забыли добавить обработчик
		fprintf(stderr, "[main] signal: %d\n", sig);
	}
}

const char *user = "nobody";

int main()
{
	/*fprintf(stderr, "Trying to switch to user: ");
	fprintf(stderr, user);
	fprintf(stderr, "\n");
	struct passwd *pw = getpwnam(user);
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
	*/
	
	open_stdlog("proxyd.log");
	fprintf(stdlog, "%s [proxyd] started\n", logtime().c_str(), nanosoft::stderror());
	
	// демон управляющий воркерами вводом-выводом
	NetDaemon daemon(1000, 128);
	
	// устанавливаем скорректированное число воркеров
	daemon.setWorkerCount(3 - 1);
	
	// XMPP-прокси
	//proxy = new XMPPProxy(&daemon, "173.212.227.170", 5222);
	proxy_c2s = new XMPPProxy(&daemon, "127.0.0.1", 5242);
	//proxy_s2s = new XMPPProxy(&daemon, "127.0.0.1", 5243);
	
	proxy_c2s->reloadWhiteList(WHITELIST_PATH);
	
	proxy_c2s->bind(5222);
	//proxy_s2s->bind(5269);
	
	proxy_c2s->listen(10);
	//proxy_s2s->listen(10);
	
	// добавляем сервер в демона
	daemon.addObject(proxy_c2s);
	//daemon.addObject(proxy_s2s);
	
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigaction(SIGTERM, &sa, 0);
	sigaction(SIGHUP, &sa, 0);
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGUSR1, &sa, 0);
	
	// запускаем демона
	fprintf(stderr, "#0: [main] run daemon\n");
	daemon.run();
	fprintf(stderr, "#0: [main] daemon exited\n");
	
	fprintf(stderr, "#0: [main] exit\n");
	fprintf(stdlog, "%s [proxyd] exited\n", logtime().c_str());
	return 0;
}
