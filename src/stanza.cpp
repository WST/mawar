
#include <stanza.h>

Stanza::Stanza(ATXmlTag *tag) {
	stanza_tag = tag;
}

Stanza::~Stanza() {
	delete stanza_tag;
}

ATXmlTag *Stanza::tag() {
	return stanza_tag;
}

JID *Stanza::from() {
	if(stanza_tag->hasAttribute("from")) {
		return new JID(stanza_tag->getAttribute("from"));
	} else {
		return 0;
	}
}

JID *Stanza::to() {
	if(stanza_tag->hasAttribute("to")) {
		return new JID(stanza_tag->getAttribute("to"));
	} else {
		return 0;
	}
}

std::string Stanza::type() {
	if(stanza_tag->hasAttribute("type")) {
		return stanza_tag->getAttribute("type");
	} else {
		return std::string("");
	}
}
