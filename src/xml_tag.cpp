
#include <xml_tag.h>
#include <iostream>

ATXmlTag::ATXmlTag(std::string name, attributtes_t tag_attributes, ATXmlTag *p, unsigned short int depth) {
	tag_cdata = "";
	parent = p;
	tag_depth = depth;
	attributes = tag_attributes;
	short int pos = name.find(":");
	if(pos != -1) {
		prefix = name.substr(0, pos);
		tag_name = name.substr(pos + 1);
	} else {
		tag_name = name;
		prefix = "";
	}
}

ATXmlTag::ATXmlTag(std::string name) {
	tag_cdata = "";
	parent = 0;
	tag_depth = 0;
	tag_name = name;
}

ATXmlTag::~ATXmlTag() {
	for(tags_list_t::iterator it = children.begin(); it != children.end(); it++) {
		delete *it;
	}
	
	for(nodes_list_t::iterator it = childnodes.begin(); it != childnodes.end(); it++) {
		delete *it;
	}
}

ATXmlTag *ATXmlTag::getParent() {
	return parent;
}

std::string ATXmlTag::getNameSpace() {
	return prefix;
}

std::string ATXmlTag::name() {
	return tag_name;
}

unsigned short int ATXmlTag::getDepth() {
	return tag_depth;
}

void ATXmlTag::insertChildElement(ATXmlTag *tag) {
	ATXmlNode *node = new ATXmlNode(TTag, tag);
	children.push_back(tag);
	childnodes.push_back(node);
}

tags_list_t ATXmlTag::getChildren() {
	return children;
}

void ATXmlTag::insertAttribute(std::string name, std::string value) {
	attributes[name] = value;
}

void ATXmlTag::setNameSpace(std::string value) {
	prefix = value;
}

void ATXmlTag::setNameSpaceAttribute(std::string name, std::string value) {
	insertAttribute("xmlns:" + name, value);
}

void ATXmlTag::setDefaultNameSpaceAttribute(std::string value) {
	insertAttribute("xmlns", value);
}

void ATXmlTag::insertCharacterData(std::string cdata) {
	ATXmlNode *node = new ATXmlNode(TCharacterData, cdata);
	childnodes.push_back(node);
}

std::string ATXmlTag::getCharacterData() {
	return ""; // TODO
}

std::string ATXmlTag::asString() {
	std::string xml = "<";
	if(!prefix.empty()) {
		xml += prefix + ":";
	}
    xml += tag_name;
	for(attributtes_t::iterator it = attributes.begin(); it != attributes.end(); it++) {
		xml += " " + it->first + std::string("=\"") + it->second + std::string("\"");
	}
    if(childnodes.empty()) {
		xml += " />";
	} else {
		xml += ">";
		for(nodes_list_t::iterator it = childnodes.begin(); it != childnodes.end(); it++) {
			switch((*it)->type) {
				case TTag:
					xml += (*it)->tag->asString();
				break;
				case TCharacterData:
				xml += (*it)->cdata;
				break;
			}
		}
		xml += "</";
		if(!prefix.empty()) {
			xml += prefix + ":";
		}
		xml += tag_name;
		xml += ">";
	}
	return xml;
}

bool ATXmlTag::hasAttribute(std::string attribute_name) {
	return attributes.find(attribute_name) != attributes.end();
}

std::string ATXmlTag::getAttribute(std::string attribute_name) {
	return attributes.find(attribute_name)->second;
}
