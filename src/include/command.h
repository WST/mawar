
#ifndef MAWAR_COMMAND_H
#define MAWAR_COMMAND_H

#include <stanza.h>
#include <form.h>
#include <taghelper.h>


class Command
{
	public:
		Command(Stanza iq);
		~Command();
		static Stanza commandCancelledStanza(std::string from, Stanza from_stanza);
		static Stanza commandDoneStanza(std::string from, Stanza from_stanza);
		std::string node();
		std::string action();
		Form *form();
		
	private:
		Form *_form;
		TagHelper iqtag;
};

#endif

