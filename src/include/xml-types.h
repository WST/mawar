
#ifndef MAWAR_XML_TYPES_H
#define MAWAR_XML_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <list>

class XmlTag;

enum XmlNodeType { TTag, TCharacterData };

class XmlNode {
	public:
	XmlNode(XmlNodeType nodetype, XmlTag *t) {
		type = nodetype;
		tag = t;
	}
	XmlNode(XmlNodeType nodetype, std::string nodecdata) {
		type = nodetype;
		cdata = nodecdata;
	}
	~XmlNode() {}
	XmlNodeType type;
	XmlTag *tag;
	std::string cdata;
};

typedef std::map<std::string, std::string> attributes_t;
typedef std::vector<XmlTag*> tags_stack_t;
typedef std::list<XmlTag*> tags_list_t;
typedef std::list<XmlNode*> nodes_list_t;

#endif
