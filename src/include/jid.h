
#ifndef MAWAR_JID_H
#define MAWAR_JID_H

#include <string>

class JID
{
	public:
		JID(std::string jid_text);
		JID();
		~JID();
		std::string bare();
		std::string hostname();
		std::string resource();
		std::string username();
		std::string full();
		void setHostname(std::string hostname);
		void setResource(std::string resource);
		void setUsername(std::string username);
		void set(std::string jid_text);
	
	private:
		std::string jid_hostname;
		std::string jid_resource;
		std::string jid_username;
};

#endif
