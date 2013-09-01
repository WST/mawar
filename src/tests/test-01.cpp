
#include <stanza.h>
#include <jid.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

/**
* With JID
*/
void VirtualHost_sendRoster_1(Stanza stanza) {
	Stanza iq = new XmlTag("iq");
	iq->setAttribute("from", stanza.from().full());
	iq->setAttribute("to", stanza.to().full());
	if(stanza->hasAttribute("id")) iq->setAttribute("id", stanza.id());
	iq->setAttribute("type", "result");
	TagHelper query = iq["query"];
	query->setDefaultNameSpaceAttribute("jabber:iq:roster");
	
	// ...
	//printf("stanza1: %s\n", iq->asString().c_str());
	delete iq;
}

/**
* Without JID
*/
void VirtualHost_sendRoster_2(Stanza stanza) {
	Stanza iq = new XmlTag("iq");
	iq->setAttribute("from", stanza->getAttribute("from", ""));
	iq->setAttribute("to", stanza->getAttribute("to", ""));
	if(stanza->hasAttribute("id")) iq->setAttribute("id", stanza.id());
	iq->setAttribute("type", "result");
	TagHelper query = iq["query"];
	query->setDefaultNameSpaceAttribute("jabber:iq:roster");
	
	// ...
	//printf("stanza2: %s\n", iq->asString().c_str());
	delete iq;
}

double microtime()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (tv.tv_usec / 1000000.) + tv.tv_sec;
}

int main(int argc, char **argv)
{
	printf("test-01: With or Without JID?\n");
	int count = argc > 1 ? atoi(argv[1]) : 100000;
	printf("count: %d\n", count);
	
	Stanza test = new XmlTag("stanza");
	test->setAttribute("from", "foo@example.com/FOO");
	test->setAttribute("to", "bar@example.com/BAR");
	
	VirtualHost_sendRoster_1(test);
	VirtualHost_sendRoster_2(test);
	
	double start, time1, time2;
	
	start = microtime();
	for(int i = 0; i < count; i++)
	{
		VirtualHost_sendRoster_1(test);
	}
	time1 = microtime() - start;
	printf("time1: %f seconds (with JID)\n", time1);
	
	start = microtime();
	for(int i = 0; i < count; i++)
	{
		VirtualHost_sendRoster_2(test);
	}
	time2 = microtime() - start;
	printf("time2: %f seconds (without JID)\n", time2);
	
	double diff = (time1 / time2) * 100.;
	
	printf("diff: %0.2f%\n", diff);
	
	return 0;
}
