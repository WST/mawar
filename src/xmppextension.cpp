
#include <xmppserver.h>
#include <xmppextension.h>
#include <xmppextensioninput.h>
#include <xmppextensionoutput.h>
#include <time.h>

#include <unistd.h>
#include <stdio.h>



// for debug only
#include <string>
#include <iostream>
#include <stdio.h>

using namespace std;
using namespace nanosoft;

/**
* Конструктор модуля-расширения
*/
XMPPExtension::XMPPExtension(XMPPServer *srv, const char *urn, const char *fname):
	server(srv)
{
	this->urn = urn;
	this->fname = fname;
	this->fpath = string(srv->config->extension_dir()) + "/" + fname;
	open();
}

/**
* Деструктор модуля-расширения
*/
XMPPExtension::~XMPPExtension()
{
}

/**
* Открыть модуль-расширение
*/
bool XMPPExtension::open()
{
	printf("MPPExtension::open(%s)\n", fpath.c_str());
	int pipe_stdin[2];
	int pipe_stdout[2];
	
	pipe(pipe_stdin);
	pipe(pipe_stdout);
	
	pid_t pid = fork();
	if ( pid < 0 )
	{
		close(pipe_stdin[0]);
		close(pipe_stdin[1]);
		close(pipe_stdout[0]);
		close(pipe_stdout[1]);
		return 0;
	}
	if ( pid == 0 )
	{
		printf("forked child\n");
		/* dup pipe read/write to stdin/stdout */
		dup2(pipe_stdin[0], STDIN_FILENO);
		dup2(pipe_stdout[1], STDOUT_FILENO);
		
		/* close unnecessary pipe descriptors for a clean environment */
		close(pipe_stdin[0]);
		close(pipe_stdin[1]);
		close(pipe_stdout[0]);
		close(pipe_stdout[1]);
		
		execlp(fpath.c_str(), fpath.c_str(), NULL);
		printf("MPPExtension::open(%s) failed\n", fpath.c_str());
		_exit(255);
	}
	else
	{
		printf("forked parent\n");
		/* Close unused pipe ends. This is especially important for the
		* pipefrom[1] write descriptor, otherwise readFromPipe will never 
		* get an EOF. */
		close( pipe_stdin[0] );
		close( pipe_stdout[1] );
		
		ctime = time(0);
		ext_in = new XMPPExtensionInput(this, pipe_stdout[0]);
		ext_out = new XMPPExtensionOutput(this, pipe_stdin[1]);
		
		server->daemon->addObject(ext_in);
		server->daemon->addObject(ext_out);
		
		ext_out->init();
	}
}

/**
* Закрыть модуль-расширение
*/
void XMPPExtension::terminate()
{
	ext_in->terminate();
	ext_out->terminate();
	
	ext_in = 0;
	ext_out = 0;
	
	int tm = time(0);
	if ( server->daemon->getDaemonActive() )
	{
		if ( (tm - ctime) > 600 )
		{
			open();
		}
		else
		{
			// TODO set timer
		}
	}
}

/**
* Обработать станзу
* @param stanza станза
*/
void XMPPExtension::handleStanza(Stanza stanza)
{
	ext_out->sendStanza(stanza);
}
