
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

JID Stanza::from() {
	if(stanza_tag->hasAttribute("from")) {
		return JID(stanza_tag->getAttribute("from"));
	} else {
		return JID("");
	}
}

JID Stanza::to() {
	if(stanza_tag->hasAttribute("to")) {
		return JID(stanza_tag->getAttribute("to"));
	} else {
		return JID("");
	}
}

std::string Stanza::type() {
	if(stanza_tag->hasAttribute("type")) {
		return stanza_tag->getAttribute("type");
	} else {
		return std::string("");
	}
}

std::string Stanza::id() {
	if(stanza_tag->hasAttribute("id")) {
		return stanza_tag->getAttribute("id");
	} else {
		return std::string("");
	}
}

Stanza Stanza::badRequest(JID server, JID reply_to, std::string id) {
	// <iq from="admin@underjabber.net.ru" type="error" xml:lang="ru-RU" to="admin@underjabber.net.ru/home" >
	// <error type="modify" code="400" >
	//	<bad-request xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/>
	//	</error>
	// </iq>
	ATXmlTag iq("iq");
	return Stanza(&iq);
}

Stanza Stanza::serverVersion(JID server, JID reply_to, std::string id) {
	ATXmlTag iq("iq");
		iq.insertAttribute("from", server.bare());
		iq.insertAttribute("to", reply_to.full());
		iq.insertAttribute("type", "result");
		if(!id.empty()) iq.insertAttribute("id", id);
		ATXmlTag query("query");
			query.setDefaultNameSpaceAttribute("jabber:iq:version");
				ATXmlTag name("name");
				ATXmlTag version("version");
				ATXmlTag os("os");
				name.insertCharacterData("mawar Jabber/XMPP engine");
				version.insertCharacterData("development branch");
				os.insertCharacterData("UNIX");
	
	query.insertChildElement(&name);
	query.insertChildElement(&version);
	query.insertChildElement(&os);
	iq.insertChildElement(&query);
	
	return Stanza(&iq);
}
