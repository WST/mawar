
#ifndef MAWAR_STANZA_H
#define MAWAR_STANZA_H

#include <xml_tag.h>
#include <jid.h>

class Stanza
{
	public:
		Stanza(ATXmlTag *tag);
		~Stanza();
		ATXmlTag *tag();
		JID from();
		JID to();
		std::string type();
		std::string id();
		
		static Stanza serverVersion(JID server, JID reply_to, std::string id);
		static Stanza badRequest(JID server, JID reply_to, std::string id);
	
	private:
		ATXmlTag *stanza_tag;
};

#endif
