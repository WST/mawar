
#include <configfile.h>
#include <cstdlib>
#include <attagparser.h>

#define CONFIG_WORKERS "3"
#define CONFIG_C2S_SESSIONS "1000"
#define CONFIG_C2S "5222"

ConfigFile::ConfigFile(const std::string &filename) {
	config_filename = filename;
	reload();
}

void ConfigFile::reload() {
	delete config_tag;
	config_tag = parse_xml_file(config_filename);
	
	if(!config_tag) {
		// ошибка!
	}
	
	if(config_tag->name() != "maward") {
		// Ошибка: корневой элемент должен быть maward
	}
	
	limits = config_tag->getChild("limits");
	listen = config_tag->getChild("listen");
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

unsigned int ConfigFile::c2s() {
	if(listen) {
		return atoi(listen->getChildValue("c2s", CONFIG_C2S).c_str());
	}
	return atoi(CONFIG_C2S);
}
