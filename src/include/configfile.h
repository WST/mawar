
#ifndef MAWAR_CONFIGFILE_H
#define MAWAR_CONFIGFILE_H

#include <xml_tag.h>
#include <taghelper.h>
#include <string>

/**
* Конфигурация виртуального хоста
*/
class VirtualHostConfig: public TagHelper
{
friend class ConfigFile;
public:
	VirtualHostConfig(ATXmlTag *host_config): TagHelper(host_config) {
	}
	
	/**
	* Вернуть имя хоста
	*/
	std::string hostname() {
		return this->tag->getAttribute("name");
	}
};

class ConfigFile
{
	public:
		ConfigFile(const std::string &filename);
		void reload();
		
		// limits
		unsigned short int workers();
		unsigned long int c2s_sessions();
		
		// listen
		unsigned int c2s();
		unsigned int s2s();
	
		//system
		const char *user();
		
		/**
		* Порт для XEP-0114
		* @return -1 если ненужно открывать порт
		*/
		int xep0114();
		
		/**
		* Вернуть первый виртуальный хост
		*/
		VirtualHostConfig firstHost();
		
		/**
		* Вернуть следующий виртуальный хост
		*/
		VirtualHostConfig nextHost(VirtualHostConfig from);
	
	private:
		ATXmlTag *config_tag;
		ATXmlTag *listen;
		ATXmlTag *limits;
		ATXmlTag *system;
		ATXmlTag *current;
		std::string config_filename;
};

#endif
