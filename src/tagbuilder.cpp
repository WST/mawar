
#include <tagbuilder.h>
#include <xml_tag.h>

ATTagBuilder::ATTagBuilder() {
	
}

void ATTagBuilder::clear() {
	delete presult;
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

ATXmlTag *ATTagBuilder::fetchResult() {
	return presult;
}
