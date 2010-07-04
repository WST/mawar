
#include <form.h>
#include <taghelper.h>

Form::Form(Stanza stanza): type() {
	tag = stanza->find("command/x");
	if(tag == 0) {
		tag = new ATXmlTag("x");
		tag->setDefaultNameSpaceAttribute("jabber:x:data");
		tag->setAttribute("type", "submit"); // Считаем, что создаём форму на основе субмита от юзера
		should_delete = true;
		return;
	}
	type = tag->getAttribute("type", "");
	should_delete = false;
}

Form::Form(ATXmlTag *from): type() {
	tag = from;
	type = from->getAttribute("type", "");
	should_delete = false;
}

Form::Form(std::string x_type): type() {
	tag = new ATXmlTag("x");
	tag->setDefaultNameSpaceAttribute("jabber:x:data");
	tag->setAttribute("type", x_type);
	type = x_type;
	should_delete = true;
}

Form::~Form() {
	if(should_delete) {
		delete tag;
	}
}

std::string Form::getFieldValue(std::string field_name, std::string default_value) {
	// TODO
	return std::string("");
}

// TODO: конструирование формы, как в веб-фреймворках

