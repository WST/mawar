
#include <stanza.h>

JID Stanza::from() {
	return JID( tag->getAttribute("from", "") );
}

JID Stanza::to() {
	return JID( tag->getAttribute("to", "") );
}

std::string Stanza::type() {
	return tag->getAttribute("type", "");
}

std::string Stanza::id() {
	return tag->getAttribute("id", "");
}

void Stanza::setFrom(JID &jid) {
	tag->setAttribute("from", jid.full());
}

Stanza Stanza::badRequest(JID server, JID reply_to, std::string id) {
	// <iq from="admin@underjabber.net.ru" type="error" xml:lang="ru-RU" to="admin@underjabber.net.ru/home" >
	// <error type="modify" code="400" >
	//	<bad-request xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/>
	//	</error>
	// </iq>
	ATXmlTag *iq = new ATXmlTag("iq");
	return iq; // тег будет удалён в деструкторе станзы
}

Stanza Stanza::serverVersion(JID server, JID reply_to, std::string id) {
	Stanza iq = new ATXmlTag("iq");
	iq->setAttribute("from", server.bare());
	iq->setAttribute("to", reply_to.full());
	iq->setAttribute("type", "result");
	if( !id.empty() ) iq->setAttribute("id", id);
	
	TagHelper query = iq["query"];
		query->setDefaultNameSpaceAttribute("jabber:iq:version");
		query["name"] = "Mawar Jabber/XMPP engine";
		query["version"] = "development branch";
		query["os"] = "UNIX";
	
	return iq;
}

Stanza Stanza::presence(JID from, JID to, ClientPresence p) {
	Stanza presence = new ATXmlTag("presence");
	presence->setAttribute("from", from.full());
	presence->setAttribute("to", to.full());
	presence["show"] = p.show;
	presence["priority"] = p.getPriority();
	presence["status"] = p.status_text;
	return presence;
}

/**
* Stream errors (RFC 3920, 4.7)
* @param condition имя тега ошибки
* @param message поясняющий текст
* @param lang язык
* @return сформированная станза
*/
Stanza Stanza::streamError(const std::string &condition, const std::string &message, const std::string &lang)
{
	Stanza error = new ATXmlTag("stream:error");
	error->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-streams");
	error[condition];
	
	if ( message != "" ) {
		error["text"] = message;
		if ( lang != "" ) error["text"]->setAttribute("xml:lang", lang);
	}
	
	return error;
}

/**
* IQ error
* @param stanza станза вызвавшая ошибку
* @param condition имя тега ошибки
* @param type тип (cancel, continue, modify, auth, wait)
* @param message поясняющий текст
* @param lang язык
* @return сформированная станза
*/
Stanza Stanza::iqError(Stanza stanza, const std::string &condition, const std::string &type, const std::string &message, const std::string &lang)
{
	Stanza iq = new ATXmlTag("iq");
	iq->setAttribute("from", stanza.to().full());
	iq->setAttribute("to", stanza.from().full());
	iq->setAttribute("type", "error");
	iq->setAttribute("id", stanza->getAttribute("id", ""));
	//iq += (ATXmlTag*)stanza;
	
	TagHelper error = iq["error"];
	error->setAttribute("type", type);
	error[condition]->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-stanzas");
	
	return iq;
}

/**
* Presence error
* @param stanza станза вызвавшая ошибку
* @param condition имя тега ошибки
* @param type тип (cancel, continue, modify, auth, wait)
* @param message поясняющий текст
* @param lang язык
* @return сформированная станза
*/
Stanza Stanza::presenceError(Stanza stanza, const std::string &condition, const std::string &type, const std::string &message, const std::string &lang)
{
	Stanza iq = new ATXmlTag("presence");
	iq->setAttribute("from", stanza.to().full());
	iq->setAttribute("to", stanza.from().full());
	iq->setAttribute("type", "error");
	if ( stanza->hasAttribute("id") ) iq->setAttribute("id", stanza->getAttribute("id", ""));
	//iq += (ATXmlTag*)stanza;
	
	TagHelper error = iq["error"];
	error->setAttribute("type", type);
	error[condition]->setDefaultNameSpaceAttribute("urn:ietf:params:xml:ns:xmpp-stanzas");
	
	return iq;
}
