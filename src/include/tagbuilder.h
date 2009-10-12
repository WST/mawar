#ifndef MAWAR_TAGBUILDER_H
#define MAWAR_TAGBUILDER_H

#include <xml_types.h>

class ATTagBuilder
{
	public:
		ATTagBuilder();
		~ATTagBuilder();
		void startElement(const std::string &name, const attributtes_t &attributes, unsigned short int depth);
		void endElement(const std::string &name);
		ATXmlTag *fetchResult();
	
	private:
		tags_stack_t stack;
		ATXmlTag *presult;
};

#endif
