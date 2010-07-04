
#ifndef MAWAR_COMMAND_H
#define MAWAR_COMMAND_H

#include <stanza.h>
#include <form.h>
#include <taghelper.h>


class Command
{
	public:
		Command(ATXmlTag *command);
		Command();
		~Command();
		static Stanza commandCancelledStanza(std::string from, Stanza from_stanza);
		static Stanza commandDoneStanza(std::string from, Stanza from_stanza);
		std::string node();
		std::string action();
		std::string sessionid();
		std::string status();
		void setNode(std::string node);
		void setAction(std::string action);
		void setSessionId(std::string sessionid);
		void setStatus(std::string status);
		void createForm(std::string x_type);
		Stanza asIqStanza(std::string from, std::string to, std::string type, std::string id);
		Form *form();
		
	private:
		Form *_form;
		TagHelper cmdtag;
};

#endif

