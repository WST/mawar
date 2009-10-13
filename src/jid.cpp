
#include <jid.h>

JID::JID(std::string jid_text) {
	jid_full = jid_text;
	int pos = jid_text.find("@");
	if(pos != -1) {
		jid_username = jid_text.substr(0, pos);
		jid_text = jid_text.substr(pos + 1);
	}
	pos = jid_text.find("/");
	if(pos != -1) {
		jid_hostname = jid_text.substr(0, pos);
		jid_resource = jid_text.substr(pos + 1);
	}
	jid_bare = jid_username + "@" + jid_hostname;
}

JID::~JID() {
	
}

std::string JID::full() {
	return jid_full;
}

std::string JID::bare() {
	return jid_bare;
}

std::string JID::hostname() {
	return jid_hostname;
}

std::string JID::resource() {
	return jid_resource;
}

std::string JID::username() {
	return jid_username;
}
