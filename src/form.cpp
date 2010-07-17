
#include <form.h>
#include <taghelper.h>

/*
Конструктор формы на основе iq-станзы, содержащей форму.
В основном для интерпретации полученных форм.
*/
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

/*
Конструктор формы на основе тега x.
В основном для интерпретации полученных форм.
*/
Form::Form(ATXmlTag *from): type() {
	tag = from;
	type = from->getAttribute("type", "");
}

/*
Конструктор формы без основы
Служит для создания новых форм внутри сервера
*/
Form::Form(std::string x_type): type() {
	tag = new ATXmlTag("x");
	tag->setDefaultNameSpaceAttribute("jabber:x:data");
	tag->setAttribute("type", x_type);
	type = x_type;
}

Form::~Form() {
	delete tag;
}

/*
Получить представление формы в виде тега
*/
ATXmlTag *Form::asTag() {
	return tag->clone();
}

/*
Установить заголовок формы
*/
void Form::setTitle(std::string form_title) {
	ATXmlTag *title = new ATXmlTag("title");
	title->insertCharacterData(form_title);
	tag->insertChildElement(title);
}

/*
Установить текстовые инструкции к форме для пользователя
*/
void Form::setInstructions(std::string form_instructions) {
	ATXmlTag *instructions = new ATXmlTag("instructions");
	instructions->insertCharacterData(form_instructions);
	tag->insertChildElement(instructions);
}

/*
Вставить однострочное текстовое поле
*/
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

/*
Вставить многострочное текстовое поле
*/
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

/*
Вставить выпадающий список
*/
void Form::insertList(std::string var, std::string label, std::list<std::string> values, std::string default_value, bool required, bool allow_multiple) {
	ATXmlTag *field = new ATXmlTag("field");
	field->setAttribute("type", allow_multiple ? "list-multi" : "list-single");
	field->setAttribute("var", var);
	field->setAttribute("label", label);
	
	ATXmlTag *option;
	ATXmlTag *value = new ATXmlTag("value");
	value->insertCharacterData(default_value);
	field->insertChildElement(value);
	
	std::list<std::string>::iterator it;
	for(it = values.begin(); it != values.end(); it++) {
		option = new ATXmlTag("option");
		option->setAttribute("label", *it);
		value = new ATXmlTag("value");
		value->insertCharacterData(*it);
		option->insertChildElement(value);
		field->insertChildElement(option);
	}
	
	if(required) {
		field->insertChildElement(new ATXmlTag("required"));
	}
	
	tag->insertChildElement(field);
}

/*
Получить значение поля по его имени
*/
std::string Form::getFieldValue(std::string field_name, std::string default_value) {
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

