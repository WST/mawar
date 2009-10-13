
#ifndef MAWAR_JID_H
#define MAWAR_JID_H

#include <string>

class JID
{
	public:
		JID(std::string jid_text);
		~JID();
		std::string bare();
		std::string hostname();
		std::string resource();
		std::string username();
	
	private:
		std::string jid_bare = "";
		std::string jid_hostname = "";
		std::string jid_resource = "";
		std::string jid_username = "";
};

#endif
