
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
