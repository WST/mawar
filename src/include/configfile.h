
#ifndef MAWAR_CONFIGFILE_H
#define MAWAR_CONFIGFILE_H

#include <xml_tag.h>
#include <string>

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
	
	private:
		ATXmlTag *config_tag;
		ATXmlTag *listen;
		ATXmlTag *limits;
		ATXmlTag *current;
		std::string config_filename;
};

#endif
