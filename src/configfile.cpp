
#include <configfile.h>
#include <cstdlib>
#include <attagparser.h>
#include <config.h>

using namespace std;

ConfigFile::ConfigFile(const std::string &filename): config_filename(filename), config_tag(0) {
	reload();
}

void ConfigFile::reload() {
	delete config_tag;
	config_tag = parse_xml_file(config_filename);
	
	if(!config_tag) {
		// ошибка!
		exit(100);
	}
	
	if(config_tag->name() != "maward") {
		// Ошибка: корневой элемент должен быть maward
	}
	
	limits = config_tag->getChild("limits");
	listen = config_tag->getChild("listen");
	system = config_tag->getChild("system");
}

const char *ConfigFile::user() {
	if(system) {
		return system->getChildValue("user", CONFIG_USERNAME).c_str();
	}
	return CONFIG_USERNAME;
}

unsigned short int ConfigFile::workers() {
	if(limits) {
		return atoi(limits->getChildValue("workers", CONFIG_WORKERS).c_str());
	}
	return atoi(CONFIG_WORKERS);
}

unsigned long int ConfigFile::c2s_sessions() {
	if(limits) {
		return atoi(limits->getChildValue("c2s_sessions", CONFIG_C2S_SESSIONS).c_str());
	}
	return atoi(CONFIG_C2S_SESSIONS);
}

size_t ConfigFile::getBufferSize()
{
	if(limits) {
		return atoi(limits->getChildValue("output-buffer", CONFIG_OUTPUT_BUFFER).c_str());
	}
	return atoi(CONFIG_C2S_SESSIONS);
}

unsigned int ConfigFile::c2s() {
	if(listen) {
		return atoi(listen->getChildValue("c2s", CONFIG_C2S).c_str());
	}
	return atoi(CONFIG_C2S);
}

unsigned int ConfigFile::s2s() {
	if(listen) {
		return atoi(listen->getChildValue("s2s", CONFIG_S2S).c_str());
	}
	return atoi(CONFIG_S2S);
}

string ConfigFile::status()
{
	if( listen ) {
		return listen->getChildValue("status", CONFIG_STATUS);
	}
	return CONFIG_STATUS;
}

/**
* Порт для XEP-0114
* @return -1 если ненужно открывать порт
*/
int ConfigFile::xep0114()
{
	if(listen) {
		return atoi(listen->getChildValue("xep0114", CONFIG_XEP0114).c_str());
	}
	return atoi(CONFIG_XEP0114);
}

/**
* Вернуть первый виртуальный хост
*/
VirtualHostConfig ConfigFile::firstHost()
{
	return config_tag->find("hosts/host");
}

/**
* Вернуть следующий виртуальный хост
*/
VirtualHostConfig ConfigFile::nextHost(VirtualHostConfig from)
{
	return config_tag->findNext("hosts/host", from);
}
