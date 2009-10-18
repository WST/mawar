
#ifndef MAWAR_CONFIGFILE_H
#define MAWAR_CONFIGFILE_H

#include <xml_tag.h>
#include <string>

/**
* Конфигурация виртуального хоста
*/
class VirtualHostConfig
{
friend class ConfigFile;
private:
	/**
	* Тег виртуального хоста
	*/
	ATXmlTag *config;
public:
	/**
	* Конструктор
	*/
	VirtualHostConfig(ATXmlTag *tag): config(tag) {
	}
	
	operator bool() {
		return config != 0;
	}
	
	/**
	* Вернуть имя хоста
	*/
	std::string hostname() {
		return config->getAttribute("name");
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
		ATXmlTag *current;
		std::string config_filename;
};

#endif
