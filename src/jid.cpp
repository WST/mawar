
#include <jid.h>

JID::JID(std::string jid_text) {
	
}

JID::~JID() {
	
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
