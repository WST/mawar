
#include <xml-tag.h>
#include <iostream>
#include <string.h>
#include <nanosoft/xmlwriter.h>

using namespace std;
using namespace nanosoft;

/**
* Текущее число выделенных тегов
*/
unsigned XmlTag::tags_created = 0;

/**
* Текущее число освобожденных тегов
*/
unsigned XmlTag::tags_destroyed = 0;

/**
* Максимальное число выделенных тегов
*/
unsigned XmlTag::tags_max_count;

XmlTag::XmlTag(std::string name, attributes_t tag_attributes, XmlTag *p, unsigned short int depth) {
	onTagCreate();
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

XmlTag::XmlTag(std::string name) {
	onTagCreate();
	tag_cdata = "";
	parent = 0;
	tag_depth = 0;
	tag_name = name;
}

XmlTag::~XmlTag() {
	onTagDestroy();
	clear();
}

/**
* Вызывается при создании тега
*/
void XmlTag::onTagCreate()
{
	tags_created++;
	
	unsigned count = getTagsCount();
	if ( count > tags_max_count ) tags_max_count = count;
}

/**
* Вызывается при удалении тега
*/
void XmlTag::onTagDestroy()
{
	tags_destroyed++;
}

XmlTag *XmlTag::getParent() {
	return parent;
}

XmlTag *XmlTag::clone() {
	return parse_xml_string(asString());
}

std::string XmlTag::getNameSpace() {
	return prefix;
}

std::string XmlTag::name() {
	return tag_name;
}

unsigned short int XmlTag::getDepth() {
	return tag_depth;
}

void XmlTag::insertChildElement(XmlTag *tag) {
	XmlNode *node = new XmlNode(TTag, tag);
	tag->parent = this;
	children.push_back(tag);
	childnodes.push_back(node);
}

tags_list_t XmlTag::getChildren() {
	return children;
}

void XmlTag::insertAttribute(std::string name, std::string value) {
	attributes[name] = value;
}

void XmlTag::setNameSpace(std::string value) {
	prefix = value;
}

void XmlTag::setNameSpaceAttribute(std::string name, std::string value) {
	insertAttribute("xmlns:" + name, value);
}

void XmlTag::setDefaultNameSpaceAttribute(std::string value) {
	insertAttribute("xmlns", value);
}

void XmlTag::insertCharacterData(std::string cdata) {
	XmlNode *node = new XmlNode(TCharacterData, cdata);
	childnodes.push_back(node);
}

std::string XmlTag::getCharacterData() {
	std::string cdata = "";
	for(nodes_list_t::iterator it = childnodes.begin(); it != childnodes.end(); it++) {
		if ( (*it)->type == TCharacterData ) {
			cdata += (*it)->cdata;
		}
	}
	return cdata;
}

std::string XmlTag::asString() {
	std::string xml = "<";
	if(!prefix.empty()) {
		xml += prefix + ":";
	}
    xml += tag_name;
	for(attributes_t::iterator it = attributes.begin(); it != attributes.end(); it++) {
		xml += " " + it->first + std::string("=\"") + XMLWriter::escape(it->second) + std::string("\"");
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
					xml += XMLWriter::escape((*it)->cdata);
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

attributes_t XmlTag::getAttributes() {
	return attributes;
}

bool XmlTag::hasChild(std::string tag_name) {
	return (bool) getChild(tag_name);
}

XmlTag *XmlTag::getChild(std::string tag_name) {
	for(tags_list_t::iterator it = children.begin(); it != children.end(); it++) {
		if((*it)->name() == tag_name) {
			return *it;
		}
	}
	return 0;
}

XmlTag *XmlTag::getChildByAttribute(std::string tag_name, std::string attribute, std::string attribute_value) {
	for(tags_list_t::iterator it = children.begin(); it != children.end(); it++) {
		if((*it)->name() == tag_name) {
			if((*it)->hasAttribute(attribute) && (*it)->getAttribute(attribute) == attribute_value) {
				return *it;
			} else {
				return 0;
			}
		}
	}
	return 0;
}

nodes_list_t XmlTag::getChildNodes() {
	return childnodes;
}

std::string XmlTag::getChildValue(std::string tag_name, std::string default_value) {
  return hasChild(tag_name) ? getChild(tag_name)->getCharacterData() : default_value;
}

/**
* Проверить существование атрибута
* @param name имя атрибута
* @return TRUE - атрибут есть, FALSE - атрибута нет
*/
bool XmlTag::hasAttribute(const std::string &name)
{
	return attributes.find(name) != attributes.end();
}

/**
* Вернуть значение атрибута
* @param name имя атрибута
* @param default_value значение по умолчанию если атрибута нет
* @return значение атрибута
*/
const std::string& XmlTag::getAttribute(const std::string &name, const std::string &default_value)
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
void XmlTag::setAttribute(const std::string &name, const std::string &value)
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
void XmlTag::removeAttribute(const std::string &name)
{
	attributes.erase(name);
}

/**
* Вернуть первого потомка
*/
XmlTag* XmlTag::firstChild()
{
	tags_list_t::iterator iter = children.begin();
	return iter != children.end() ? *iter : 0;
}

/**
* Вернуть следующего потомка следующего за тегом from
*/
XmlTag* XmlTag::nextChild(XmlTag *from)
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
XmlTag* XmlTag::firstChild(const char *name)
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
XmlTag* XmlTag::firstChild(const std::string &name)
{
	return firstChild(name.c_str());
}

/**
* Вернуть следующего потока с именем name следующего за тегом from
*/
XmlTag* XmlTag::nextChild(const char *name, XmlTag *from)
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
XmlTag* XmlTag::nextChild(const std::string &name, XmlTag *from)
{
	return nextChild(name.c_str(), from);
}

/**
* Вернуть первый дочерний узел, какого бы типа он ни был
*/
XmlNode* XmlTag::firstChildNode()
{
	nodes_list_t::iterator iter = childnodes.begin();
	return iter != childnodes.end() ? *iter : 0;
}

/**
* Вернуть следующий дочерний узел, какого бы типа он ни был
*/
XmlNode* XmlTag::nextChildNode(XmlNode* from)
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
XmlTag* XmlTag::find(const char *path)
{
	const char *remain = strchr(path, '/');
	if ( remain == 0 ) return firstChild(path);
	
	// TODO выделять строку во временном буфере
	string name(path, remain++);
	
	for(XmlTag *child = firstChild(name.c_str()); child; child = nextChild(name.c_str(), child))
	{
		XmlTag *result = child->find(remain);
		if ( result ) return result;
	}
	
	return 0;
}

/**
* Найти первого потомка по указанному пути
*/
XmlTag* XmlTag::find(const std::string &path)
{
	return find(path.c_str());
}

/**
* Найти следующий узел
* @param path путь к узлу
* @return найденый узел или 0 если узлов больше нет
*/
XmlTag* XmlTag::findNext(const char *path, XmlTag *from)
{
	const char *remain = strchr(path, '/');
	if ( remain == 0 ) return nextChild(path, from);
	
	// TODO выделять строку во временном буфере
	string name(path, remain++);
	
	//XmlTag *parent = from->parent;
	
	for(XmlTag *child = firstChild(name.c_str()); child; child = nextChild(name.c_str(), child))
	{
		if ( child->hasChild(from) )
		{
			XmlTag *result = child->findNext(remain, from);
			if ( result ) return result;
			
			child = nextChild(name.c_str(), child);
			for(; child; child = nextChild(name.c_str(), child))
			{
				XmlTag *result = child->find(remain);
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
XmlTag* XmlTag::findNext(const std::string &path, XmlTag *from)
{
	return findNext(path.c_str(), from);
}

/**
* Проверить имеет ли потомка
* @note требуется для findNext
*/
bool XmlTag::hasChild(XmlTag *tag)
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
* Удалить потомка с указанным именем
*/
void XmlTag::removeChild(const char *name)
{
	for(nodes_list_t::iterator iter = childnodes.begin(); iter != childnodes.end(); ++iter)
	{
		XmlNode *node = *iter;
		if ( node->type == TTag && node->tag->name() == name )
		{
			childnodes.erase(iter);
			children.remove(node->tag);
			delete node->tag;
			delete node;
		}
	}
}

/**
* Удалить потомка с указанным именем
*/
void XmlTag::removeChild(const std::string &name)
{
	for(nodes_list_t::iterator iter = childnodes.begin(); iter != childnodes.end(); ++iter)
	{
		XmlNode *node = *iter;
		if ( node->type == TTag && node->tag->name() == name )
		{
			childnodes.erase(iter);
			children.remove(node->tag);
			delete node->tag;
			delete node;
		}
	}
}

/**
* Удалить всех потомков
*/
void XmlTag::clear()
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

