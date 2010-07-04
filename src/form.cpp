
#include <form.h>
#include <taghelper.h>

Form::Form(Stanza stanza): type() {
	tag = stanza->find("command/x");
	if(tag == 0) {
		tag = new ATXmlTag("x");
		tag->setDefaultNameSpaceAttribute("jabber:x:data");
		tag->setAttribute("type", "submit"); // Считаем, что создаём форму на основе субмита от юзера
		return;
	}
	type = tag->getAttribute("type", "");
}

Form::Form(ATXmlTag *from): type() {
	tag = from;
	type = from->getAttribute("type", "");
}

Form::Form(std::string x_type): type() {
	tag = new ATXmlTag("x");
	tag->setDefaultNameSpaceAttribute("jabber:x:data");
	tag->setAttribute("type", x_type);
	type = x_type;
}

Form::~Form() {
	// Удалять ли tag, и если да, то при каких условиях?
	// TODO: разобраться после реализации ad-hoc
}

ATXmlTag *Form::asTag() {
	return tag;
}

void Form::setTitle(std::string form_title) {
	ATXmlTag *title = new ATXmlTag("title");
	title->insertCharacterData(form_title);
	tag->insertChildElement(title);
}

void Form::setInstructions(std::string form_instructions) {
	ATXmlTag *instructions = new ATXmlTag("instructions");
	instructions->insertCharacterData(form_instructions);
	tag->insertChildElement(instructions);
}

void Form::insertLineEdit(std::string var, std::string label, std::string value, bool required) {
	ATXmlTag *field = new ATXmlTag("field");
	field->setAttribute("type", "text-single");
	field->setAttribute("var", var);
	field->setAttribute("label", label);
	
	ATXmlTag *value_tag = new ATXmlTag("value");
	value_tag->insertCharacterData(value);
	field->insertChildElement(value_tag);
	
	if(required) {
		field->insertChildElement(new ATXmlTag("required"));
	}
	
	tag->insertChildElement(field);
}

void Form::insertTextEdit(std::string var, std::string label, std::string value, bool required) {
	ATXmlTag *field = new ATXmlTag("field");
	field->setAttribute("type", "text-multi");
	field->setAttribute("var", var);
	field->setAttribute("label", label);
	
	ATXmlTag *value_tag = new ATXmlTag("value");
	value_tag->insertCharacterData(value);
	field->insertChildElement(value_tag);
	
	if(required) {
		field->insertChildElement(new ATXmlTag("required"));
	}
	
	tag->insertChildElement(field);
}

std::string Form::getFieldValue(std::string field_name, std::string default_value) {
	// TODO
	ATXmlTag *field = tag->getChildByAttribute("field", "var", field_name);
	if(!field) {
		return std::string("");
	}
	if(!field->hasChild("value")) {
		return std::string("");
	}
	ATXmlTag *value = field->getChild("value");
	return value->getCharacterData();
}

// TODO: другие типы виджетов

