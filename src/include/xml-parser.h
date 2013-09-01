#ifndef MAWAR_XML_PARSER_H
#define MAWAR_XML_PARSER_H

#include <nanosoft/xmlparser.h>
#include <nanosoft/stream.h>
#include <xml-types.h>
#include <xml-tag.h>
#include <tagbuilder.h>

/**
* Паресер XML-файлов
*
* XmlParser парсит файл целиком в один XmlTag
*/
class XmlParser: protected nanosoft::XMLParser, protected TagBuilder
{
public:
	/**
	* Конструктор парсера
	*/
	XmlParser();
	
	/**
	* Деструктор парсера
	*/
	~XmlParser();
	
	/**
	* Парсинг файла
	* @param path путь к файлу
	* @return тег в случае успеха и NULL в случае ошибки
	*/
	XmlTag * parseFile(const char *path);
	XmlTag * parseFile(const std::string &path);
	
	/**
	* Парсинг произвольного потока
	* @param s поток с данными
	* @return тег в случае успеха и NULL в случае ошибки
	*/
	XmlTag * parseStream(nanosoft::stream &s);
	
	/**
	* Парсинг строки
	* @param xml XML
	* @return тег в случае успеха и NULL в случае ошибки
	*/
	XmlTag * parseString(const std::string &xml);
	
protected:
	/**
	* Обработчик открытия тега
	*/
	virtual void onStartElement(const std::string &name, const nanosoft::XMLParser::attributtes_t &attributes);
	
	/**
	* Обработчик символьных данных
	*/
	virtual void onCharacterData(const std::string &cdata);
	
	/**
	* Обработчик закрытия тега
	*/
	virtual void onEndElement(const std::string &name);
	
	/**
	* Обработчик ошибок парсера
	*/
	void onParseError(const char *message);
private:
	/**
	* Глубина обрабатываемого тега
	*/
	int depth;
};

/**
* Парсинг файла
* @param path путь к файлу
* @return тег в случае успеха и NULL в случае ошибки
*/
XmlTag * parse_xml_file(const char *path);
XmlTag * parse_xml_file(const std::string &path);

/**
* Парсинг произвольного потока
* @param s поток с данными
* @return тег в случае успеха и NULL в случае ошибки
*/
XmlTag * parse_xml_stream(nanosoft::stream &s);

/**
* Парсинг произвольной строки
* @param xml XML
* @return тег в случае успеха и NULL в случае ошибки
*/
XmlTag * parse_xml_string(const std::string &xml);

#endif // MAWAR_XML_PARSER_H
