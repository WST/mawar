
#ifndef MAWAR_XML_TAG_H
#define MAWAR_XML_TAG_H

// C++
#include <string>

// Mawar
#include <xml_types.h>

class ATXmlTag
{
	public:
		ATXmlTag(std::string name, attributes_t tag_attributes, ATXmlTag *p, unsigned short int depth);
		ATXmlTag(std::string name);
		~ATXmlTag();
		
		// === потенциально устаревшие методы ===
		// какие-то их них после ревизии перенесены в новые
		// (такие как например getAttribute), другие отмечены
		// как действительно устаревшие
		
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
		bool hasChild(std::string tag_name);
		ATXmlTag *getChild(std::string tag_name);
		nodes_list_t getChildNodes();
		attributes_t getAttributes();
		std::string getAttribute(std::string name, std::string default_value);
		
		// === новые методы ===
		
		/**
		* Вернуть значение узла
		* @param tag_name имя тега
		* @param default_value значение по умолчанию если тега нет
		* @return значение узла
		*/
		std::string getChildValue(std::string tag_name, std::string default_value);
		
		/**
		* Вернуть первого потомка
		* @return первый потомок или 0 если потомков нет
		*/
		ATXmlTag* firstChild();
		
		/**
		* Вернуть следующего потомка следующего за тегом from
		* @param from узел с которого нужно начать поиск
		* @return найденый потомок или 0 если больше потомков нет
		*/
		ATXmlTag* nextChild(ATXmlTag *from);
		
		/**
		* Вернуть первого потомка с именем name
		* @param name имя искомого узла
		* @return найденый узел или 0
		*/
		ATXmlTag* firstChild(const char *name);
		ATXmlTag* firstChild(const std::string &name);
		
		/**
		* Вернуть следующего потока с именем name следующего за тегом from
		* @param name имя искомого потомка
		* @param from узел с которого надо начать поиск
		* @return найденый узел или 0 если больше потомков нет
		*/
		ATXmlTag* nextChild(const char *name, ATXmlTag *from);
		ATXmlTag* nextChild(const std::string &name, ATXmlTag *from);
		
		/**
		* Вернуть первый дочерний узел, какого бы типа он ни был
		* @return первый дочерний узел или 0 если дочерних узлов нет
		*/
		ATXmlNode* firstChildNode();
		
		/**
		* Вернуть следующий дочерний узел, какого бы типа он ни был
		* @return найденый узел или 0 если больше дочерних узлов нет
		*/
		ATXmlNode* nextChildNode(ATXmlNode *from);
		
		/**
		* Найти первого потомка по указанному пути
		* @param path путь к узлу
		* @return найденый узел или 0 если узел не найден
		*/
		ATXmlTag* find(const char *path);
		ATXmlTag* find(const std::string &path);
		
		/**
		* Найти следующий узел
		* @param path путь к узлу
		* @return найденый узел или 0 если узлов больше нет
		*/
		ATXmlTag* findNext(const char *path, ATXmlTag *from);
		ATXmlTag* findNext(const std::string &path, ATXmlTag *from);
		
		/**
		* Проверить имеет ли потомка
		* @note требуется для findNext
		*/
		bool hasChild(ATXmlTag *tag);
		
	private:
		std::string tag_name;
		std::string tag_cdata;
		unsigned short int tag_depth;
		attributes_t attributes;
		tags_list_t children;
		nodes_list_t childnodes;
		ATXmlTag *parent;
		std::string prefix;
};

#endif
