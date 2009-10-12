
#include <tagbuilder.h>
#include <xml_tag.h>

ATTagBuilder::ATTagBuilder() {

}

ATTagBuilder::~ATTagBuilder() {
	
}

void ATTagBuilder::startElement(const std::string &name, const attributtes_t &attributes, unsigned short int depth) {
	if(stack.empty()) {
		stack.push(new ATXmlTag(name, attributes, 0, depth));
	} else {
		stack.push(new ATXmlTag(name, attributes, stack.top(), depth));
	}
}

void ATTagBuilder::endElement(const std::string &name) {
	ATXmlTag *current = stack.top();
	stack.pop();
	if(!stack.empty()) {
		stack.top()->insertChildElement(current);
	} else {
		presult = current;
	}
}

void ATTagBuilder::characterData(const std::string &cdata) {
	if(!stack.empty()) {
		stack.top()->insertCharacterData(cdata);
	}
}

ATXmlTag *ATTagBuilder::fetchResult() {
	ATXmlTag *retval = presult;
	presult = 0;
	return retval;
}