
#include <tagbuilder.h>
#include <xml_tag.h>

ATTagBuilder::ATTagBuilder(): presult(0) {

}

ATTagBuilder::~ATTagBuilder() {
	
}

void ATTagBuilder::startElement(const std::string &name, const attributes_t &attributes, unsigned short int depth) {
	if(stack.empty()) {
		stack.push_back(new ATXmlTag(name, attributes, 0, depth));
	} else {
		stack.push_back(new ATXmlTag(name, attributes, stack.back(), depth));
	}
}

void ATTagBuilder::endElement(const std::string &name) {
	ATXmlTag *current = stack.back();
	stack.pop_back();
	if(!stack.empty()) {
		stack.back()->insertChildElement(current);
	} else {
		presult = current;
	}
}

void ATTagBuilder::characterData(const std::string &cdata) {
	if(!stack.empty()) {
		stack.back()->insertCharacterData(cdata);
	}
}

ATXmlTag *ATTagBuilder::fetchResult() {
	ATXmlTag *retval = presult;
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
void ATTagBuilder::reset()
{
	if ( presult ) {
		stack.clear();
		delete presult;
		presult = 0;
	}
}
