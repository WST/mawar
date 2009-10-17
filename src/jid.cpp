
#include <jid.h>

JID::JID(std::string jid_text) {
	int pos = jid_text.find("@");
	if(pos != -1) {
		jid_username = jid_text.substr(0, pos);
		jid_text = jid_text.substr(pos + 1);
	}
	pos = jid_text.find("/");
	if(pos != -1) {
		jid_hostname = jid_text.substr(0, pos);
		jid_resource = jid_text.substr(pos + 1);
	} else {
		jid_hostname = jid_text;
	}
}

JID::JID() {
	
}

JID::~JID() {
	
}

std::string JID::full() {
	std::string retval;
	if(!jid_username.empty()) {
		retval += jid_username + "@";
	}
	retval += jid_hostname;
	if(!jid_resource.empty()) {
		retval += "/" + jid_resource;
	}
	return retval;
}

std::string JID::bare() {
	std::string retval;
	if(!jid_username.empty()) {
		retval += jid_username + "@";
	}
	retval += jid_hostname;
	return retval;
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

void JID::setHostname(std::string hostname) {
	jid_hostname = hostname;
}

void JID::setResource(std::string resource) {
	jid_resource = resource;
}

void JID::setUsername(std::string username) {
	jid_username = username;
}
