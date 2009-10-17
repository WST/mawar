
#ifndef AT_XML_TYPES_H
#define AT_XML_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <list>

class ATXmlTag;

enum ATXmlNodeType { TTag, TCharacterData };

class ATXmlNode {
	public:
	ATXmlNode(ATXmlNodeType nodetype, ATXmlTag *t) {
		type = nodetype;
		tag = t;
	}
	ATXmlNode(ATXmlNodeType nodetype, std::string nodecdata) {
		type = nodetype;
		cdata = nodecdata;
	}
	~ATXmlNode() {}
	ATXmlNodeType type;
	ATXmlTag *tag;
	std::string cdata;
};

typedef std::map<std::string, std::string> attributes_t;
typedef std::vector<ATXmlTag*> tags_stack_t;
typedef std::list<ATXmlTag*> tags_list_t;
typedef std::list<ATXmlNode*> nodes_list_t;

#endif
