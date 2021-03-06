
#include <command.h>

/*
Конструктор команды на основе тега command
*/
Command::Command(XmlTag *command) {
	_form = command->hasChild("x") ? new Form(command->getChild("x")) : 0;
	cmdtag = command->clone();
}

/*
Конструктор команды из ничего (для порождения ответов на команды)
*/
Command::Command() {
	cmdtag = new XmlTag("command");
	cmdtag->setDefaultNameSpaceAttribute("http://jabber.org/protocol/commands");
}

Command::~Command() {
	delete _form;
	delete cmdtag;
}

std::string Command::action() {
	return cmdtag->getAttribute("action", "");
}

std::string Command::node() {
	return cmdtag->getAttribute("node", "");
}

std::string Command::sessionid() {
	return cmdtag->getAttribute("sessionid", "");
}

std::string Command::status() {
	return cmdtag->getAttribute("status", "");
}

Form *Command::form() {
	return _form;
}

void Command::setNode(std::string node) {
	cmdtag->setAttribute("node", node);
}

void Command::setAction(std::string action) {
	cmdtag->setAttribute("action", action);
}

void Command::setSessionId(std::string sessionid) {
	cmdtag->setAttribute("sessionid", sessionid);
}

void Command::setStatus(std::string status) {
	cmdtag->setAttribute("status", status);
}

void Command::createForm(std::string x_type) {
	_form = new Form(x_type);
}

Stanza Command::asIqStanza(std::string from, std::string to, std::string type, std::string id) {
	cmdtag->insertChildElement(_form->asTag()); // TODO: костыль!
	Stanza iq = new XmlTag("iq");
	iq->setAttribute("from", from);
	iq->setAttribute("to", to);
	iq->setAttribute("type", type);
	if(!id.empty()) iq->setAttribute("id", id);
	iq->insertChildElement(cmdtag->clone());
	return iq;
}

Stanza Command::commandDoneStanza(std::string from, Stanza stanza) {
	Stanza iq = new XmlTag("iq");
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
	Stanza iq = new XmlTag("iq");
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

