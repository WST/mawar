
#ifndef MAWAR_FORM_H
#define MAWAR_FORM_H

#include <xml_tag.h>
#include <stanza.h>
#include <string>

class Form: public ATXmlTag
{
	public:
		Form(Stanza stanza); // конструктор из станзы
		Form(ATXmlTag *tag); // конструктор из тега x
		Form(); // конструктор из ничего
		~Form();
		static Stanza formCancelledStanza(std::string hostname, Stanza from_stanza);
		std::string getFieldValue(std::string field_name, std::string default_value);
};

#endif

