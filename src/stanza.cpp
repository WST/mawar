
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

Stanza Stanza::serverVersion(JID server, JID reply_to, std::string id) {
	ATXmlTag iq("iq");
		iq.insertAttribute("from", server.bare());
		iq.insertAttribute("to", reply_to.full());
		iq.insertAttribute("type", "result");
		iq.insertAttribute("id", id);
		ATXmlTag query("query");
			query.setDefaultNameSpaceAttribute("jabber:iq:version");
				ATXmlTag name("name");
				ATXmlTag version("version");
				ATXmlTag os("os");
	
	query.insertChildElement(&name);
	query.insertChildElement(&version);
	query.insertChildElement(&os);
	iq.insertChildElement(&query);
	
	return Stanza(&iq);
}
