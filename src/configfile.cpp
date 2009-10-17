
#include <configfile.h>
#include <cstdlib>
#include <attagparser.h>

ConfigFile::ConfigFile(const std::string &filename) {
	ATXmlTag *tag = parse_xml_file(filename);
	if(!tag) {
		// ошибка!
	}
	ATXmlTag *current;
	if(tag->name() != "maward") {
		// Ошибка: корневой элемент должен быть maward
	}
	if(!tag->hasChild("limits")) {
		// Ошибка: должен быть вложенный элемент limits
	}
	current = tag->getChild("limits");
	if(!current->hasChild("workers")) {
		// Ошибка: в limits должен быть элемент workers
	}
	
	config_workers = atoi(current->getChild("workers")->getCharacterData().c_str());
	if(config_workers == 0) {
		// Ошибка: число воркеров получилось 0
	}
	
	config_c2s_sessions = atoi(current->getChild("c2s_sessions")->getCharacterData().c_str());
	if(config_c2s_sessions == 0) {
		// Ошибка: число воркеров получилось 0
	}
	
	if(!tag->hasChild("listen")) {
		// Ошибка: должен быть вложенный элемент listen
	}
	current = tag->getChild("listen");
	if(!current->hasChild("c2s")) {
		// Ошибка: в listen должен быть элемент c2s
	}
	config_c2s = atoi(current->getChild("c2s")->getCharacterData().c_str());
	if(config_c2s == 0) {
		// Ошибка: порт = 0
	}
	
	delete tag;
}

unsigned short int ConfigFile::workers() {
	return config_workers;
}

unsigned long int ConfigFile::c2s_sessions() {
	return config_c2s_sessions;
}

unsigned int ConfigFile::c2s() {
	return config_c2s;
}
