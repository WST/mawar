
#include <iostream>
#include <nanosoft/netdaemon.h>
#include <xmppserver.h>
#include <myconsole.h>
#include <xml_tag.h>
#include <configfile.h>
#include <signal.h>
#include <string.h>
#include <nanosoft/mysql.h>

#include <taghelper.h>

using namespace std;

/**
* Пример использования некоторых фишек TagHelper
*
* Расскоментируй вызов в main() чтобы поигаться
*/
void test_TagHelper()
{
	// создаем тег foo
	TagHelper tag = new ATXmlTag("foo");
	
	// устанавливаем в нем атрибут bar=test
	tag->setAttribute("bar", "test");
	
	// записываем в него текст (если есть потомки, то они удаляются)
	tag = "Hello world";
	
	// добавить тег проще пареной репы :-)
	tag["message"] = "Hello world";
	
	// добавить стразу два вложенных тега <iq><bind> тоже просто:
	tag["iq/bind"] = ":-)";
	
	// добавить ещё текста? :-)
	tag += "bla bla bla";
	
	// добавить тег?
	TagHelper bar = tag += new ATXmlTag("bar");
	bar->setAttribute("name", "bar");
	
	// здесь мы не копируем объект, мы берем ссылку/указатель
	TagHelper iq = tag["iq"];
	iq["session"]->setAttribute("id", "12345");
	
	cout << "iq: " << iq->asString() << endl;
	
	cout << "foo: " << tag->asString() << endl;
	

	// и даже удаляется :-]
	delete tag;
}

// XMPP-сервер
XMPPServer *server;

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
	
	//test_TagHelper();
	
	// демон управляющий воркерами вводом-выводом
	NetDaemon daemon(config->c2s_sessions());
	daemon.setWorkerCount(config->workers());
	
	// XMPP-сервер
	server = new XMPPServer(&daemon);
	
	// подключемся к c2s-порту из конфига
	server->bind(config->c2s());
	
	// не более 10 ожидающих соединений
	server->listen(10);
	
	// добавляем виртуальные хосты
	cerr << "[main] loading virtual hosts..." << endl;
	for(VirtualHostConfig vhost = config->firstHost(); vhost; vhost = config->nextHost(vhost))
	{
		cerr << "vhost: " << vhost.hostname() << endl;
		server->addHost(vhost.hostname(), vhost);
	}
	cerr << "[main] virtual hosts loaded" << endl;
	
	// добавляем сервер в демона
	daemon.addObject(server);
	
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
	cerr << "[main] running daemon..." << endl;
	daemon.run();
	cerr << "[main] daemon exited" << endl;
	
	delete server;
	
	return 0;
}
