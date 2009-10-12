
#ifndef MAWAR_XML_TAG_H
#define MAWAR_XML_TAG_H

// C++
#include <string>

// Mawar
#include <xml_types.h>

class ATXmlTag
{
	public:
		ATXmlTag(std::string name, attributtes_t tag_attributes, ATXmlTag *p, unsigned short int depth);
		ATXmlTag(std::string name);
		~ATXmlTag();
		std::string name();
		unsigned short int getDepth();
		void insertCharacterData(std::string cdata);
		std::string getCharacterData();
		void insertChildElement(ATXmlTag *tag);
		void insertAttribute(std::string name, std::string value);
		tags_list_t getChildren();
		ATXmlTag *getParent();
		void setNameSpace(std::string value);
		void setNameSpace(std::string name, std::string value);
		void setDefaultNameSpaceAttribute(std::string value);
		void setNameSpaceAttribute(std::string name, std::string value);
		std::string getNameSpace();
		std::string asString();
		bool hasAttribute(std::string attribute_name);
		std::string getAttribute(std::string attribute_name);
	
	private:
		std::string tag_name;
		std::string tag_cdata;
		unsigned short int tag_depth;
		attributtes_t attributes;
		tags_list_t children;
		nodes_list_t childnodes;
		ATXmlTag *parent;
		std::string prefix;
};

#endif
