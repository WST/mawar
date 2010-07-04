
#ifndef MAWAR_FORM_H
#define MAWAR_FORM_H

#include <xml_tag.h>
#include <stanza.h>
#include <string>

class Form
{
	public:
		Form(Stanza stanza); // конструктор из станзы
		Form(ATXmlTag *tag); // конструктор из тега x
		Form(std::string type); // конструктор из ничего
		~Form();
		void setTitle(std::string form_title);
		void setInstructions(std::string form_instructions);
		std::string getFieldValue(std::string field_name, std::string default_value);
		void insertLineEdit(std::string var, std::string label, std::string value, bool required = false);
		ATXmlTag *asTag();
		
	private:
		std::string type;
		ATXmlTag *tag;
		bool should_delete; // удалять ли tag
};

#endif

