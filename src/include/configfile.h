
#ifndef MAWAR_CONFIGFILE_H
#define MAWAR_CONFIGFILE_H

#include <xml_tag.h>

class ConfigFile
{
	public:
		ConfigFile(std::string filename);
		
		// limits
		unsigned short int workers();
		unsigned long int c2s_sessions();
		
		// listen
		unsigned int c2s();
	
	private:
		unsigned short int config_workers;
		unsigned long int config_c2s_sessions;
		unsigned int config_c2s;
};

#endif
