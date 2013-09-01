
#ifndef MAWAR_CONFIGFILE_H
#define MAWAR_CONFIGFILE_H

#include <xml_tag.h>
#include <taghelper.h>
#include <string>

class ConfigFile
{
	public:
		ConfigFile(const std::string &filename);
		void reload();
		
		// limits
		unsigned short int workers();
		unsigned long int filesLimit();
		size_t getOutputBuffers();
		
		// listen
		unsigned int c2s();
		unsigned int s2s();
		std::string status(); // status socket
		
		//system
		const char *user();
		const char *extension_dir();
		
		/**
		* Порт для XEP-0114
		* @return -1 если ненужно открывать порт
		*/
		int xep0114();
		
		/**
		* Вернуть первый виртуальный хост
		*/
		ATXmlTag *firstHost();
		
		/**
		* Вернуть следующий виртуальный хост
		*/
		ATXmlTag *nextHost(ATXmlTag *from);
	
		/**
		* Вернуть первый сервер групповых сообщений
		*/
		ATXmlTag *firstGroupsHost();
		
		/**
		* Вернуть следующий сервер групповых сообщений
		*/
		ATXmlTag *nextGroupsHost(ATXmlTag *from);
	
		/**
		* Вернуть первый виртуальный хост
		*/
		TagHelper firstExternal();
		
		/**
		* Вернуть следующий виртуальный хост
		*/
		TagHelper nextExternal(TagHelper from);
	private:
		ATXmlTag *config_tag;
		ATXmlTag *listen;
		ATXmlTag *limits;
		ATXmlTag *system;
		ATXmlTag *current;
		std::string config_filename;
};

#endif
