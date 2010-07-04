
#include <form.h>

Form::Form(Stanza stanza): ATXmlTag("x") {
	// TODO
}

Form::Form(ATXmlTag *tag): ATXmlTag("x") {
	// TODO
}

Form::Form(): ATXmlTag("x") {
	// TODO
}

Form::~Form() {
	// TODO
}

Stanza Form::formCancelledStanza(std::string hostname, Stanza stanza) {
	Stanza iq = new ATXmlTag("iq");
	iq->setAttribute("from", hostname);
	iq->setAttribute("to", stanza.from().full());
	iq->setAttribute("type", "result");
	if(!stanza.id().empty()) iq->setAttribute("id", stanza.id());
	TagHelper command = iq["command"];
		if(stanza["command"]->hasAttribute("sessionid")) command->setAttribute("sessionid", stanza["command"]->getAttribute("sessionid"));
		command->setAttribute("node", stanza["command"]->getAttribute("node", ""));
		command->setAttribute("status", "cancelled");
	return iq;
}

