
#ifndef AT_XML_TYPES_H
#define AT_XML_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <list>

class XmlTag;

enum ATXmlNodeType { TTag, TCharacterData };

class ATXmlNode {
	public:
	ATXmlNode(ATXmlNodeType nodetype, XmlTag *t) {
		type = nodetype;
		tag = t;
	}
	ATXmlNode(ATXmlNodeType nodetype, std::string nodecdata) {
		type = nodetype;
		cdata = nodecdata;
	}
	~ATXmlNode() {}
	ATXmlNodeType type;
	XmlTag *tag;
	std::string cdata;
};

typedef std::map<std::string, std::string> attributes_t;
typedef std::vector<XmlTag*> tags_stack_t;
typedef std::list<XmlTag*> tags_list_t;
typedef std::list<ATXmlNode*> nodes_list_t;

#endif
