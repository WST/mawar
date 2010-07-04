
#include <command.h>

Command::Command(Stanza iq) {
	form = new Form(iq);
	iqtag = iq;
}

Command::~Command() {
	delete form;
}

std::string Command::action() {
	return iqtag["command"]->getAttribute("action", "");
}

std::string Command::node() {
	return iqtag["command"]->getAttribute("node", "");
}

Stanza Command::commandDoneStanza(std::string from, Stanza stanza) {
	Stanza iq = new ATXmlTag("iq");
	iq->setAttribute("from", from);
	iq->setAttribute("to", stanza.from().full());
	iq->setAttribute("type", "result");
	if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
	TagHelper command = iq["command"];
		command->setAttribute("sessionid", stanza["command"]->getAttribute("sessionid", ""));
		command->setAttribute("node", stanza["command"]->getAttribute("node", ""));
		command->setAttribute("status", "completed");
	return iq;
}

Stanza Command::commandCancelledStanza(std::string from, Stanza stanza) {
	Stanza iq = new ATXmlTag("iq");
	iq->setAttribute("from", from);
	iq->setAttribute("to", stanza.from().full());
	iq->setAttribute("type", "result");
	if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
	TagHelper command = iq["command"];
		command->setAttribute("sessionid", stanza["command"]->getAttribute("sessionid", ""));
		command->setAttribute("node", stanza["command"]->getAttribute("node", ""));
		command->setAttribute("status", "cancelled");
	return iq;
}

