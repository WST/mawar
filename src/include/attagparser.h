#ifndef MAWAR_ATTAGPARSER_H
#define MAWAR_ATTAGPARSER_H

#include <nanosoft/xmlparser.h>
#include <nanosoft/stream.h>
#include <xml_types.h>
#include <xml_tag.h>
#include <tagbuilder.h>

/**
* Паресер XML-файлов
*
* ATTagParser парсит файл целиком в один ATXmlTag
*/
class ATTagParser: protected nanosoft::XMLParser, protected ATTagBuilder
{
public:
	/**
	* Конструктор парсера
	*/
	ATTagParser();
	
	/**
	* Деструктор парсера
	*/
	~ATTagParser();
	
	/**
	* Парсинг файла
	* @param path путь к файлу
	* @return тег в случае успеха и NULL в случае ошибки
	*/
	ATXmlTag * parseFile(const char *path);
	ATXmlTag * parseFile(const std::string &path);
	
	/**
	* Парсинг произвольного потока
	* @param s поток с данными
	* @return тег в случае успеха и NULL в случае ошибки
	*/
	ATXmlTag * parseStream(nanosoft::stream &s);
	
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
ATXmlTag * parse_xml_file(const char *path);
ATXmlTag * parse_xml_file(const std::string &path);

/**
* Парсинг произвольного потока
* @param s поток с данными
* @return тег в случае успеха и NULL в случае ошибки
*/
ATXmlTag * parse_xml_stream(nanosoft::stream &s);

#endif // MAWAR_ATTAGPARSER_H
