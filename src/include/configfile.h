
#ifndef MAWAR_CONFIGFILE_H
#define MAWAR_CONFIGFILE_H

#include <xml-tag.h>
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
		XmlTag *firstHost();
		
		/**
		* Вернуть следующий виртуальный хост
		*/
		XmlTag *nextHost(XmlTag *from);
	
		/**
		* Вернуть первый сервер групповых сообщений
		*/
		XmlTag *firstGroupsHost();
		
		/**
		* Вернуть следующий сервер групповых сообщений
		*/
		XmlTag *nextGroupsHost(XmlTag *from);
	
		/**
		* Вернуть первый виртуальный хост
		*/
		TagHelper firstExternal();
		
		/**
		* Вернуть следующий виртуальный хост
		*/
		TagHelper nextExternal(TagHelper from);
	private:
		XmlTag *config_tag;
		XmlTag *listen;
		XmlTag *limits;
		XmlTag *system;
		XmlTag *current;
		std::string config_filename;
};

#endif
