#ifndef MAWAR_TAGBUILDER_H
#define MAWAR_TAGBUILDER_H

#include <xml-types.h>

class TagBuilder
{
	public:
		TagBuilder();
		~TagBuilder();
		void startElement(const std::string &name, const attributes_t &attributes, unsigned short int depth);
		void endElement(const std::string &name);
		void characterData(const std::string &cdata);
		XmlTag *fetchResult();
		
		/**
		* Сброс билдера
		*
		* Удаляет незавершенный тег и сбарсывает все внутренние
		* структуры к начальному состоянию для обработки нового
		* тега
		*/
		void reset();
	private:
		tags_stack_t stack;
		XmlTag *presult;
};

#endif
