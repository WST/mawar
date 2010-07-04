
#ifndef MAWAR_FORM_H
#define MAWAR_FORM_H

#include <xml_tag.h>
#include <stanza.h>

class Form: public ATXmlTag
{
	public:
		Form(Stanza stanza); // конструктор из станзы
		Form(ATXmlTag *tag); // конструктор из тега x
		Form(); // конструктор из ничего
		~Form();
};


#endif

