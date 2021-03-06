
#ifndef MAWAR_XML_TAG_H
#define MAWAR_XML_TAG_H

// C++
#include <string>

// Mawar
#include <xml-types.h>
#include <xml-parser.h>

class XmlTag
{
	public:
		XmlTag(std::string name, attributes_t tag_attributes, XmlTag *p, unsigned short int depth);
		XmlTag(std::string name);
		~XmlTag();
		
		// === потенциально устаревшие методы ===
		// какие-то их них после ревизии перенесены в новые
		// (такие как например getAttribute), другие отмечены
		// как действительно устаревшие
		
		std::string name();
		unsigned short int getDepth();
		void insertCharacterData(std::string cdata);
		std::string getCharacterData();
		void insertChildElement(XmlTag *tag);
		void insertAttribute(std::string name, std::string value);
		tags_list_t getChildren();
		XmlTag *getParent();
		void setNameSpace(std::string value);
		void setNameSpace(std::string name, std::string value);
		void setDefaultNameSpaceAttribute(std::string value);
		void setNameSpaceAttribute(std::string name, std::string value);
		std::string getNameSpace();
		std::string asString();
		bool hasChild(std::string tag_name);
		XmlTag *getChild(std::string tag_name);
		XmlTag *getChildByAttribute(std::string tag_name, std::string attribute, std::string attribute_value);
		nodes_list_t getChildNodes();
		attributes_t getAttributes();
		XmlTag *clone();
		
		// === новые методы ===
		
		/**
		* Проверить существование атрибута
		* @param name имя атрибута
		* @return TRUE - атрибут есть, FALSE - атрибута нет
		*/
		bool hasAttribute(const std::string &name);
		
		/**
		* Вернуть значение атрибута
		* @param name имя атрибута
		* @param default_value значение по умолчанию если атрибута нет
		* @return значение атрибута
		*/
		const std::string& getAttribute(const std::string &name, const std::string &default_value = "");
		
		/**
		* Установить значение атрибута
		*
		* Если атрибут уже есть, то сменить его значение
		* Если атрибута нет, то добавить атрибут с указанным значением
		*
		* @param name имя атрибута
		* @param value новое значение атрибута
		*/
		void setAttribute(const std::string &name, const std::string &value);
		
		/**
		* Удалить атрибут
		*
		* Если атрибута нет, то ничего не делать - это не ошибка
		*
		* @param name имя удаляемого атрибута
		*/
		void removeAttribute(const std::string &name);
		
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
		XmlTag* firstChild();
		
		/**
		* Вернуть следующего потомка следующего за тегом from
		* @param from узел с которого нужно начать поиск
		* @return найденый потомок или 0 если больше потомков нет
		*/
		XmlTag* nextChild(XmlTag *from);
		
		/**
		* Вернуть первого потомка с именем name
		* @param name имя искомого узла
		* @return найденый узел или 0
		*/
		XmlTag* firstChild(const char *name);
		XmlTag* firstChild(const std::string &name);
		
		/**
		* Вернуть следующего потока с именем name следующего за тегом from
		* @param name имя искомого потомка
		* @param from узел с которого надо начать поиск
		* @return найденый узел или 0 если больше потомков нет
		*/
		XmlTag* nextChild(const char *name, XmlTag *from);
		XmlTag* nextChild(const std::string &name, XmlTag *from);
		
		/**
		* Вернуть первый дочерний узел, какого бы типа он ни был
		* @return первый дочерний узел или 0 если дочерних узлов нет
		*/
		XmlNode* firstChildNode();
		
		/**
		* Вернуть следующий дочерний узел, какого бы типа он ни был
		* @return найденый узел или 0 если больше дочерних узлов нет
		*/
		XmlNode* nextChildNode(XmlNode *from);
		
		/**
		* Найти первого потомка по указанному пути
		* @param path путь к узлу
		* @return найденый узел или 0 если узел не найден
		*/
		XmlTag* find(const char *path);
		XmlTag* find(const std::string &path);
		
		/**
		* Найти следующий узел
		* @param path путь к узлу
		* @return найденый узел или 0 если узлов больше нет
		*/
		XmlTag* findNext(const char *path, XmlTag *from);
		XmlTag* findNext(const std::string &path, XmlTag *from);
		
		/**
		* Проверить имеет ли потомка
		* @note требуется для findNext
		*/
		bool hasChild(XmlTag *tag);
		
		/**
		* Удалить потомка с указанным именем
		*/
		void removeChild(const char *name);
		void removeChild(const std::string &name);
		
		/**
		* Удалить всех потомков
		*/
		void clear();
		
		/**
		* Вернуть число выделенных (активных) тегов
		*/
		static unsigned getTagsCount() { return tags_created - tags_destroyed; }
		
		/**
		* Вернуть максимальное число тегов
		*/
		static unsigned getTagsMaxCount() { return tags_max_count; }
		
		/**
		* Вернуть число созданных тегов (с момента запуска)
		*/
		static unsigned getTagsCreated() { return tags_created; }
		
		/**
		* Вернуть число удаленных тегов (с момента запуска)
		*/
		static unsigned getTagsDestroyed() { return tags_destroyed; }
		
	private:
		std::string tag_name;
		std::string tag_cdata;
		unsigned short int tag_depth;
		attributes_t attributes;
		tags_list_t children;
		nodes_list_t childnodes;
		XmlTag *parent;
		std::string prefix;
		
		// ------- Статистика ------- //
		
		/**
		* Текущее число выделенных тегов
		*/
		static unsigned tags_created;
		
		/**
		* Текущее число освобожденных тегов
		*/
		static unsigned tags_destroyed;
		
		/**
		* Максимальное число выделенных тегов
		*/
		static unsigned tags_max_count;
		
		/**
		* Вызывается при создании тега
		*/
		static void onTagCreate();
		
		/**
		* Вызывается при удалении тега
		*/
		static void onTagDestroy();
};

#endif
