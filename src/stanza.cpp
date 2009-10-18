
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

Stanza *Stanza::badRequest(JID server, JID reply_to, std::string id) {
	// <iq from="admin@underjabber.net.ru" type="error" xml:lang="ru-RU" to="admin@underjabber.net.ru/home" >
	// <error type="modify" code="400" >
	//	<bad-request xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/>
	//	</error>
	// </iq>
	ATXmlTag *iq = new ATXmlTag("iq");
	return new Stanza(iq); // тег будет удалён в деструкторе станзы
}

Stanza *Stanza::serverVersion(JID server, JID reply_to, std::string id) {
	ATXmlTag *iq = new ATXmlTag("iq");
		iq->insertAttribute("from", server.bare());
		iq->insertAttribute("to", reply_to.full());
		iq->insertAttribute("type", "result");
		if(!id.empty()) iq->insertAttribute("id", id);
		ATXmlTag *query = new ATXmlTag("query");
			query->setDefaultNameSpaceAttribute("jabber:iq:version");
				ATXmlTag *name = new ATXmlTag("name");
				ATXmlTag *version = new ATXmlTag("version");
				ATXmlTag *os = new ATXmlTag("os");
				name->insertCharacterData("mawar Jabber/XMPP engine");
				version->insertCharacterData("development branch");
				os->insertCharacterData("UNIX");
	
	query->insertChildElement(name);
	query->insertChildElement(version);
	query->insertChildElement(os);
	iq->insertChildElement(query);
	
	return new Stanza(iq);
}

Stanza *Stanza::presence(JID from, JID to, ClientPresence p) {
	ATXmlTag *presence = new ATXmlTag("presence");
		ATXmlTag *show = new ATXmlTag("show");
			show->insertCharacterData(p.getShow());
		ATXmlTag *priority = new ATXmlTag("priority");
			priority->insertCharacterData(p.getPriority());
		ATXmlTag *status = new ATXmlTag("status");
			status->insertCharacterData(p.status_text);
		
		presence->insertChildElement(show);
		presence->insertChildElement(priority);
		presence->insertChildElement(status);
		
		presence->insertAttribute("from", from.full());
		presence->insertAttribute("to", to.full());
	
	return new Stanza(presence);
}

/**
* Stream errors (RFC 3920, 4.7)
* @param condition имя тега ошибки
* @param message поясняющий текст
* @param lang язык
* @return сформированная станза
*/
Stanza* Stanza::streamError(const std::string &condition, const std::string &message, const std::string &lang)
{
	ATXmlTag *error = new ATXmlTag("stream:error");
	error->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-streams");
	error->insertChildElement( new ATXmlTag(condition) );
	
	if ( message != "" ) {
		ATXmlTag * text = new ATXmlTag("text");
		if( lang != "" ) text->insertAttribute("xml:lang", lang);
		text->insertCharacterData(message);
		error->insertChildElement(text);
	}
	
	return new Stanza(error);
}
