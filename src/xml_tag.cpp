
#include <xml_tag.h>
#include <iostream>
#include <string.h>

using namespace std;

ATXmlTag::ATXmlTag(std::string name, attributes_t tag_attributes, ATXmlTag *p, unsigned short int depth) {
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
	clear();
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
	tag->parent = this;
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
	std::string cdata = "";
	for(nodes_list_t::iterator it = childnodes.begin(); it != childnodes.end(); it++) {
		if ( (*it)->type == TCharacterData ) {
			cdata += (*it)->cdata;
		}
	}
	return cdata;
}

std::string ATXmlTag::asString() {
	std::string xml = "<";
	if(!prefix.empty()) {
		xml += prefix + ":";
	}
    xml += tag_name;
	for(attributes_t::iterator it = attributes.begin(); it != attributes.end(); it++) {
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

attributes_t ATXmlTag::getAttributes() {
	return attributes;
}

bool ATXmlTag::hasChild(std::string tag_name) {
	return (bool) getChild(tag_name);
}

ATXmlTag *ATXmlTag::getChild(std::string tag_name) {
	for(tags_list_t::iterator it = children.begin(); it != children.end(); it++) {
		if((*it)->name() == tag_name) {
			return *it;
		}
	}
	return 0;
}

nodes_list_t ATXmlTag::getChildNodes() {
	return childnodes;
}

std::string ATXmlTag::getChildValue(std::string tag_name, std::string default_value) {
  return hasChild(tag_name) ? getChild(tag_name)->getCharacterData() : default_value;
}

/**
* Проверить существование атрибута
* @param name имя атрибута
* @return TRUE - атрибут есть, FALSE - атрибута нет
*/
bool ATXmlTag::hasAttribute(const std::string &name)
{
	return attributes.find(name) != attributes.end();
}

/**
* Вернуть значение атрибута
* @param name имя атрибута
* @param default_value значение по умолчанию если атрибута нет
* @return значение атрибута
*/
const std::string& ATXmlTag::getAttribute(const std::string &name, const std::string &default_value)
{
	attributes_t::iterator attr = attributes.find(name);
	return attr != attributes.end() ? attr->second : default_value;
}

/**
* Установить значение атрибута
*
* Если атрибут уже есть, то сменить его значение
* Если атрибута нет, то добавить атрибут с указанным значением
*
* @param name имя атрибута
* @param value новое значение атрибута
*/
void ATXmlTag::setAttribute(const std::string &name, const std::string &value)
{
	attributes[name] = value;
}

/**
* Удалить атрибут
*
* Если атрибута нет, то ничего не делать - это не ошибка
*
* @param name имя удаляемого атрибута
*/
void ATXmlTag::removeAttribute(const std::string &name)
{
	attributes.erase(name);
}

/**
* Вернуть первого потомка
*/
ATXmlTag* ATXmlTag::firstChild()
{
	tags_list_t::iterator iter = children.begin();
	return iter != children.end() ? *iter : 0;
}

/**
* Вернуть следующего потомка следующего за тегом from
*/
ATXmlTag* ATXmlTag::nextChild(ATXmlTag *from)
{
	for(tags_list_t::iterator iter = children.begin(); iter != children.end(); ++iter)
	{
		if ( *iter == from )
		{
			++iter;
			return iter != children.end() ? *iter : 0;
		}
	}
	return 0;
}

/**
* Вернуть первого потомка с именем name
*/
ATXmlTag* ATXmlTag::firstChild(const char *name)
{
	for(tags_list_t::iterator iter = children.begin(); iter != children.end(); ++iter)
	{
		if ( (*iter)->name() == name ) return *iter;
	}
	return 0;
}

/**
* Вернуть первого потомка с именем name
*/
ATXmlTag* ATXmlTag::firstChild(const std::string &name)
{
	return firstChild(name.c_str());
}

/**
* Вернуть следующего потока с именем name следующего за тегом from
*/
ATXmlTag* ATXmlTag::nextChild(const char *name, ATXmlTag *from)
{
	for(tags_list_t::iterator iter = children.begin(); iter != children.end(); ++iter)
	{
		if ( *iter == from )
		{
			for(++iter; iter != children.end(); ++iter)
			{
				if ( (*iter)->name() == name ) return *iter;
			}
			return 0;
		}
	}
	return 0;
}

/**
* Вернуть следующего потока с именем name следующего за тегом from
*/
ATXmlTag* ATXmlTag::nextChild(const std::string &name, ATXmlTag *from)
{
	return nextChild(name.c_str(), from);
}

/**
* Вернуть первый дочерний узел, какого бы типа он ни был
*/
ATXmlNode* ATXmlTag::firstChildNode()
{
	nodes_list_t::iterator iter = childnodes.begin();
	return iter != childnodes.end() ? *iter : 0;
}

/**
* Вернуть следующий дочерний узел, какого бы типа он ни был
*/
ATXmlNode* ATXmlTag::nextChildNode(ATXmlNode* from)
{
	for(nodes_list_t::iterator iter = childnodes.begin(); iter != childnodes.end(); ++iter)
	{
		if ( *iter == from )
		{
			++iter;
			return iter != childnodes.end() ? *iter : 0;
		}
	}
	return 0;
}

/**
* Найти первого потомка по указанному пути
*/
ATXmlTag* ATXmlTag::find(const char *path)
{
	const char *remain = strchr(path, '/');
	if ( remain == 0 ) return firstChild(path);
	
	// TODO выделять строку во временном буфере
	string name(path, remain++);
	
	for(ATXmlTag *child = firstChild(name.c_str()); child; child = nextChild(name.c_str(), child))
	{
		ATXmlTag *result = child->find(remain);
		if ( result ) return result;
	}
	
	return 0;
}

/**
* Найти первого потомка по указанному пути
*/
ATXmlTag* ATXmlTag::find(const std::string &path)
{
	return find(path.c_str());
}

/**
* Найти следующий узел
* @param path путь к узлу
* @return найденый узел или 0 если узлов больше нет
*/
ATXmlTag* ATXmlTag::findNext(const char *path, ATXmlTag *from)
{
	const char *remain = strchr(path, '/');
	if ( remain == 0 ) return nextChild(path, from);
	
	// TODO выделять строку во временном буфере
	string name(path, remain++);
	
	ATXmlTag *parent = from->parent;
	
	for(ATXmlTag *child = firstChild(name.c_str()); child; child = nextChild(name.c_str(), child))
	{
		if ( child->hasChild(from) )
		{
			ATXmlTag *result = child->findNext(remain, from);
			if ( result ) return result;
			
			child = nextChild(name.c_str(), child);
			for(; child; child = nextChild(name.c_str(), child))
			{
				ATXmlTag *result = child->find(remain);
				if ( result ) return result;
			}
			
			return 0;
		}
	}
	
	return 0;
}

/**
* Найти следующий узел
* @param path путь к узлу
* @return найденый узел или 0 если узлов больше нет
*/
ATXmlTag* ATXmlTag::findNext(const std::string &path, ATXmlTag *from)
{
	return findNext(path.c_str(), from);
}

/**
* Проверить имеет ли потомка
* @note требуется для findNext
*/
bool ATXmlTag::hasChild(ATXmlTag *tag)
{
	// тег не является своим родителем
	if ( tag == this ) return false;
	while ( tag ) {
		if ( tag->parent == this ) return true;
		tag = tag->parent;
	}
	return false;
}

/**
* Удалить всех потомков
*/
void ATXmlTag::clear()
{
	for(tags_list_t::iterator it = children.begin(); it != children.end(); it++) {
		delete *it;
	}
	children.clear();
	
	for(nodes_list_t::iterator it = childnodes.begin(); it != childnodes.end(); it++) {
		delete *it;
	}
	childnodes.clear();
}
