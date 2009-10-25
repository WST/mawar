
#include <stanza.h>
#include <jid.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <nanosoft/fstream.h>
#include <nanosoft/xmlwriter.h>
#include <nanosoft/error.h>

using namespace nanosoft;

/**
* send(tag->asString())
*/
void XMPPStream_sendTag_1(fstream *f, ATXmlTag * tag) {
	std::string xml = tag->asString();
	int r = f->write(xml.c_str(), xml.length());
	if ( r != xml.length() ) error("write fault :-(");
}

/**
* XMLWriter
*/
class XMLStream: public fstream, public XMLWriter
{
public:
	XMLStream(const char *fileName, int flags): fstream(fileName, flags) {
	}
	void sendTag(ATXmlTag * tag);
	void onWriteXML(const char *buf, size_t len) {
		int r = write(buf, len);
		if ( r != len ) error("write fault :-(");
	}
};

void XMLStream::sendTag(ATXmlTag * tag) {
	startElement(tag->name());
	attributes_t attributes = tag->getAttributes();
	for(attributes_t::iterator it = attributes.begin(); it != attributes.end(); it++) {
		setAttribute(it->first, it->second);
	}
	nodes_list_t nodes = tag->getChildNodes();
	for(nodes_list_t::iterator it = nodes.begin(); it != nodes.end(); it++) {
		if((*it)->type == TTag) {
			sendTag((*it)->tag);
		} else {
			characterData((*it)->cdata);
		}
	}
	endElement(tag->name());
	// send(tag->asString()); — так будет куда проще…
}

double microtime()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (tv.tv_usec / 1000000.) + tv.tv_sec;
}

int main(int argc, char **argv)
{
	printf("test-02: // send(tag->asString()); — так будет куда проще…\n");
	int count = argc > 1 ? atoi(argv[1]) : 100000;
	printf("count: %d\n", count);
	
	Stanza test = new ATXmlTag("message");
	test->setAttribute("from", "foo@example.com/FOO");
	test->setAttribute("to", "bar@example.com/BAR");
	test["body"] = "Hello world";
	
	
	double start, time1, time2;
	
	fstream *f = new fstream("test1.xml", fstream::rw);
	start = microtime();
	for(int i = 0; i < count; i++)
	{
		XMPPStream_sendTag_1(f, test);
	}
	time1 = microtime() - start;
	printf("time1: %f seconds (tag->asString())\n", time1);
	delete f;
	
	XMLStream *x = new XMLStream("test2.xml", fstream::rw);
	start = microtime();
	for(int i = 0; i < count; i++)
	{
		x->sendTag(test);
		x->flush();
	}
	time2 = microtime() - start;
	printf("time2: %f seconds (XMLWriter)\n", time2);
	delete x;
	
	double diff = (time2 / time1) * 100.;
	
	printf("diff: %0.2f%\n", diff);
	
	return 0;
}
