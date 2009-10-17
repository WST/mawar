
#include <configfile.h>
#include <cstdlib>
#include <attagparser.h>

#define CONFIG_WORKERS "3"
#define CONFIG_C2S_SESSIONS "1000"

ConfigFile::ConfigFile(const std::string &filename) {
	config_filename = filename;
	reload();
	
	ATXmlTag *current;
	if(config_tag->name() != "maward") {
		// Ошибка: корневой элемент должен быть maward
	}
}

void ConfigFile::reload() {
	delete config_tag;
	config_tag = parse_xml_file(config_filename);
	if(!config_tag) {
		// ошибка!
	}
}

unsigned short int ConfigFile::workers() {
	if(config_tag->hasChild("limits")) {
		current = config_tag->getChild("limits");
		return atoi(current->getChildValue("workers", CONFIG_WORKERS).c_str());
	}
	return atoi(CONFIG_WORKERS);
}

unsigned long int ConfigFile::c2s_sessions() {
	if(config_tag->hasChild("limits")) {
		current = config_tag->getChild("limits");
		return atoi(current->getChildValue("c2s_sessions", CONFIG_C2S_SESSIONS).c_str());
	}
	return atoi(CONFIG_C2S_SESSIONS);
}

unsigned int ConfigFile::c2s() {
	if(config_tag->hasChild("listen")) {
		current = config_tag->getChild("listen");
		return atoi(current->getChildValue("c2s", CONFIG_WORKERS).c_str());
	}
	return atoi(CONFIG_WORKERS);
}
