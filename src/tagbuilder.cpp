
#include <tagbuilder.h>
#include <xml-tag.h>

TagBuilder::TagBuilder(): presult(0) {

}

TagBuilder::~TagBuilder() {
	
}

void TagBuilder::startElement(const std::string &name, const attributes_t &attributes, unsigned short int depth) {
	if(stack.empty()) {
		stack.push_back(new XmlTag(name, attributes, 0, depth));
	} else {
		stack.push_back(new XmlTag(name, attributes, stack.back(), depth));
	}
}

void TagBuilder::endElement(const std::string &name) {
	XmlTag *current = stack.back();
	stack.pop_back();
	if(!stack.empty()) {
		stack.back()->insertChildElement(current);
	} else {
		presult = current;
	}
}

void TagBuilder::characterData(const std::string &cdata) {
	if(!stack.empty()) {
		stack.back()->insertCharacterData(cdata);
	}
}

XmlTag *TagBuilder::fetchResult() {
	XmlTag *retval = presult;
	presult = 0;
	return retval;
}

/**
* Сброс билдера
*
* Удаляет незавершенный тег и сбарсывает все внутренние
* структуры к начальному состоянию для обработки нового
* тега
*/
void TagBuilder::reset()
{
	if ( presult ) {
		stack.clear();
		delete presult;
		presult = 0;
	}
}
