#include <attagparser.h>
#include <nanosoft/fstream.h>
#include <iostream>

//using namespace std;
using namespace nanosoft;

/**
* Конструктор парсера
*/
ATTagParser::ATTagParser()
{
}

/**
* Деструктор потока
*/
ATTagParser::~ATTagParser()
{
}

/**
* Парсинг файла
* @param path путь к файлу
* @return тег в случае успеха и NULL в случае ошибки
*/
ATXmlTag * ATTagParser::parseFile(const char *path)
{
	fstream file;
	if ( ! file.open(path, fstream::ro) ) {
		std::cerr << "[ATTagParser] file not found: " << path << std::endl;
		return 0;
	}
	return parseStream(file);
}

/**
* Парсинг файла
* @param path путь к файлу
* @return тег в случае успеха и NULL в случае ошибки
*/
ATXmlTag * ATTagParser::parseFile(const std::string &path)
{
	return parseFile(path.c_str());
}

/**
* Парсинг произвольного потока
* @param path путь к файлу
* @return тег в случае успеха и NULL в случае ошибки
*/
ATXmlTag * ATTagParser::parseStream(nanosoft::stream &s)
{
	depth = 0;
	char buf[4096];
	while ( 1 )
	{
		int r = s.read(buf, sizeof(buf));
		if ( r > 0 ) parseXML(buf, r, false);
		else if ( r == 0 ) return fetchResult();
		else
		{
			ATTagBuilder::reset();
			return 0;
		}
	}
}

/**
* Обработчик открытия тега
*/
void ATTagParser::onStartElement(const std::string &name, const nanosoft::XMLParser::attributtes_t &attributes)
{
	depth ++;
	startElement(name, attributes, depth);
}

/**
* Обработчик символьных данных
*/
void ATTagParser::onCharacterData(const std::string &cdata)
{
	characterData(cdata);
}

/**
* Обработчик закрытия тега
*/
void ATTagParser::onEndElement(const std::string &name)
{
	endElement(name);
	depth --;
}

/**
* Обработчик ошибок парсера
*/
void ATTagParser::onParseError(const char *message)
{
	std::cerr << message << std::endl;
}

/**
* Парсинг файла
* @param path путь к файлу
* @return тег в случае успеха и NULL в случае ошибки
*/
ATXmlTag * parse_xml_file(const char *path)
{
	ATTagParser parser;
	return parser.parseFile(path);
}

/**
* Парсинг файла
* @param path путь к файлу
* @return тег в случае успеха и NULL в случае ошибки
*/
ATXmlTag * parse_xml_file(const std::string &path)
{
	ATTagParser parser;
	return parser.parseFile(path);
}

/**
* Парсинг произвольного потока
* @param s поток с данными
* @return тег в случае успеха и NULL в случае ошибки
*/
ATXmlTag * parse_xml_stream(nanosoft::stream &s)
{
	ATTagParser parser;
	return parser.parseStream(s);
}
