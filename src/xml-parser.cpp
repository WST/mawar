#include <xml-parser.h>
#include <nanosoft/fstream.h>
#include <iostream>

//using namespace std;
using namespace nanosoft;

/**
* Конструктор парсера
*/
XmlParser::XmlParser()
{
}

/**
* Деструктор потока
*/
XmlParser::~XmlParser()
{
}

/**
* Парсинг файла
* @param path путь к файлу
* @return тег в случае успеха и NULL в случае ошибки
*/
XmlTag * XmlParser::parseFile(const char *path)
{
	fstream file;
	if ( ! file.open(path, fstream::ro) ) {
		std::cerr << "[XmlParser] file not found: " << path << std::endl;
		return 0;
	}
	return parseStream(file);
}

/**
* Парсинг файла
* @param path путь к файлу
* @return тег в случае успеха и NULL в случае ошибки
*/
XmlTag * XmlParser::parseFile(const std::string &path)
{
	return parseFile(path.c_str());
}

/**
* Парсинг произвольного потока
* @param path путь к файлу
* @return тег в случае успеха и NULL в случае ошибки
*/
XmlTag * XmlParser::parseStream(nanosoft::stream &s)
{
	depth = 0;
	char buf[4096];
	while ( 1 )
	{
		int r = s.read(buf, sizeof(buf));
		
		if ( r < 0 )
		{
			ATTagBuilder::reset();
			return 0;
		}
		
		if ( r == 0 )
		{
			return fetchResult();
		}
		
		if ( ! parseXML(buf, r, false) )
		{
			ATTagBuilder::reset();
			return 0;
		}
	}
}

/**
* Парсинг строки
* @param xml XML
* @return тег в случае успеха и NULL в случае ошибки
*/
XmlTag * XmlParser::parseString(const std::string &xml)
{
	depth = 0;
	if ( parseXML(xml.c_str(), xml.length(), true) )
	{
		return fetchResult();
	}
	return 0;
}


/**
* Обработчик открытия тега
*/
void XmlParser::onStartElement(const std::string &name, const nanosoft::XMLParser::attributtes_t &attributes)
{
	depth ++;
	startElement(name, attributes, depth);
}

/**
* Обработчик символьных данных
*/
void XmlParser::onCharacterData(const std::string &cdata)
{
	characterData(cdata);
}

/**
* Обработчик закрытия тега
*/
void XmlParser::onEndElement(const std::string &name)
{
	endElement(name);
	depth --;
}

/**
* Обработчик ошибок парсера
*/
void XmlParser::onParseError(const char *message)
{
	std::cerr << "XmlParser::onParseError: "<< message << std::endl;
}

/**
* Парсинг файла
* @param path путь к файлу
* @return тег в случае успеха и NULL в случае ошибки
*/
XmlTag * parse_xml_file(const char *path)
{
	XmlParser parser;
	return parser.parseFile(path);
}

/**
* Парсинг файла
* @param path путь к файлу
* @return тег в случае успеха и NULL в случае ошибки
*/
XmlTag * parse_xml_file(const std::string &path)
{
	XmlParser parser;
	return parser.parseFile(path);
}

/**
* Парсинг произвольного потока
* @param s поток с данными
* @return тег в случае успеха и NULL в случае ошибки
*/
XmlTag * parse_xml_stream(nanosoft::stream &s)
{
	XmlParser parser;
	return parser.parseStream(s);
}

/**
* Парсинг произвольной строки
* @param xml XML
* @return тег в случае успеха и NULL в случае ошибки
*/
XmlTag * parse_xml_string(const std::string &xml)
{
	XmlParser parser;
	return parser.parseString(xml);
}
