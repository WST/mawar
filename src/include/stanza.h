
#ifndef MAWAR_STANZA_H
#define MAWAR_STANZA_H

#include <xml_tag.h>

class Stanza
{
	public:
		Stanza(ATXmlTag *tag);
		~Stanza();
		ATXmlTag *tag();
	
	private:
		ATXmlTag *stanza_tag;
};

#endif
