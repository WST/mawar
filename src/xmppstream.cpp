
#include <xmppstream.h>
#include <xmppserver.h>
#include <tagbuilder.h>
#include <nanosoft/base64.h>

// for debug only
#include <string>
#include <iostream>

using namespace std;
using namespace nanosoft;

/**
* Конструктор потока
*/
XMPPStream::XMPPStream(XMPPServer *srv, int sock): AsyncXMLStream(sock), server(srv), XMLWriter(1024), state(init)
{
	depth = 0;
	builder = new ATTagBuilder();
}

/**
* Деструктор потока
*/
XMPPStream::~XMPPStream()
{
	delete builder;
}

/**
* Запись XML
*/
void XMPPStream::onWriteXML(const char *data, size_t len)
{
	int r = write(data, len);
	if ( r != len ) onError("write fault");
	cout << "written: \033[22;34m" << string(data, len) << "\033[0m\n";
}

/**
* Событие готовности к записи
*
* Вызывается, когда в поток готов принять
* данные для записи без блокировки
*/
void XMPPStream::onWrite()
{
	cerr << "not implemented XMPPStream::onWrite()" << endl;
}

/**
* Событие закрытие соединения
*
* Вызывается если peer закрыл соединение
*/
void XMPPStream::onShutdown()
{
	server->onOffline(this);
	AsyncXMLStream::onShutdown();
	XMLWriter::flush();
	cerr << "[TestStream]: peer shutdown connection" << endl;
	if ( shutdown(fd, SHUT_RDWR) != 0 )
	{
		stderror();
	}
	server->daemon->removeObject(this);
	delete this;
}

/**
* Обработчик открытия тега
*/
void XMPPStream::onStartElement(const std::string &name, const attributtes_t &attributes)
{
	depth ++;
	switch ( depth )
	{
	case 1:
		onStartStream(name, attributes);
		break;
	case 2: // начало станзы
		builder->startElement(name, attributes, depth);
	break;
	default: // добавить тег в станзу
		builder->startElement(name, attributes, depth);
	}
}

/**
* Обработчик символьных данных
*/
void XMPPStream::onCharacterData(const std::string &cdata)
{
	builder->characterData(cdata);
}

/**
* Обработчик закрытия тега
*/
void XMPPStream::onEndElement(const std::string &name)
{
	switch (depth)
	{
	case 1:
		onEndStream();
		break;
	case 2: {
		builder->endElement(name);
		Stanza *s = new Stanza(builder->fetchResult());
		onStanza(s);
		delete s; // Внимание — станза удаляется здесь
		break;
	}
	default:
		builder->endElement(name);
	}
	depth --;
}

/**
* Обработчик станз
*/
void XMPPStream::onStanza(Stanza *stanza)
{
	cout << "stanza: " << stanza->tag()->name() << endl;
	if (stanza->tag()->name() == "iq") onIqStanza(stanza);
	else if (stanza->tag()->name() == "auth") onAuthStanza(stanza);
	else if (stanza->tag()->name() == "response" ) onResponseStanza(stanza);
	else if (stanza->tag()->name() == "message") onMessageStanza(stanza);
	else if (stanza->tag()->name() == "presence") onPresenceStanza(stanza);
	else ; // ...
}

/**
* Обработчик авторизации
*/
void XMPPStream::onAuthStanza(Stanza *stanza)
{
	string mechanism = stanza->tag()->getAttribute("mechanism");
	
	sasl = server->start("xmpp", client_jid.hostname(), mechanism);
	onSASLStep(string());
}

/**
* Обработка этапа авторизации SASL
*/
void XMPPStream::onSASLStep(const std::string &input)
{
	string output;
	switch ( server->step(sasl, input, output) )
	{
	case SASLServer::ok:
		client_jid.setUsername(server->getUsername(sasl));
		server->close(sasl);
		startElement("success");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
		endElement("success");
		flush();
		resetWriter();
		state = authorized;
		depth = 1; // после выхода из onAuthStanza/onStanza() будет стандартный depth--
		resetParser();
		break;
	case SASLServer::next:
		startElement("challenge");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
			characterData(base64_encode(output));
		endElement("challenge");
		flush();
		break;
	case SASLServer::error:
		server->close(sasl);
		startElement("failure");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
			addElement("temporary-auth-failure", "");
		endElement("failure");
		flush();
		break;
	}
}

/**
* Обработчик авторизации: ответ клиента
*/
void XMPPStream::onResponseStanza(Stanza *stanza)
{
	onSASLStep(base64_decode(stanza->tag()->getCharacterData()));
}

/**
* Обработчик iq-станзы
*/
void XMPPStream::onIqStanza(Stanza *stanza)
{
	if(stanza->tag()->hasChild("bind") && (stanza->type() == "set" || stanza->type() == "get")) {
		client_jid.setResource(stanza->type() == "set" ? stanza->tag()->getChild("bind")->getChild("resource")->getCharacterData() : "foo");
		startElement("iq");
			setAttribute("type", "result");
			setAttribute("id", stanza->tag()->getAttribute("id"));
			startElement("bind");
				setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-bind");
				startElement("jid");
					characterData(jid().full());
				endElement("jid");
			endElement("bind");
		endElement("iq");
		flush();
	}
	if(stanza->tag()->hasChild("session") && stanza->type() == "set") {
		startElement("iq");
			setAttribute("id", stanza->tag()->getAttribute("id"));
			setAttribute("type", "result");
			startElement("session");
				setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-session");
			endElement("session");
		endElement("iq");
		flush();
		server->onOnline(this);
	}
	if(stanza->tag()->hasChild("query") && stanza->type() == "get") {
		// Входящие запросы информации
		std::string query_xmlns = stanza->tag()->getChild("query")->getAttribute("xmlns");
		if(query_xmlns == "jabber:iq:version") {
			// Отправить версию сервера
			// send(Stanza::serverVersion(сервер, stanza->from(), stanza->id()));
		} else if (query_xmlns == "jabber:iq:roster") {
			startElement("iq");
				setAttribute("type", "result");
				setAttribute("id", stanza->tag()->getAttribute("id"));
				startElement("query");
					setAttribute("xmlns", "jabber:iq:roster");
					XMPPServer::users_t users = server->getUserList();
					for(XMPPServer::users_t::iterator pos = users.begin(); pos != users.end(); ++pos)
					{
						startElement("item");
							setAttribute("jid", *pos);
							setAttribute("name", *pos);
							setAttribute("subscription", "both");
							addElement("group", "Friends");
						endElement("item");
					}
				endElement("query");
			endElement("iq");
			flush();
		} else {
		    // Неизвестный запрос
			//Stanza s = Stanza::badRequest(JID(""), stanza->from(), stanza->id());
		    //sendStanza(&s);
		}
	}
}

int XMPPStream::priority() {
	return resource_priority;
}

void XMPPStream::onMessageStanza(Stanza *stanza) {
	stanza->tag()->insertAttribute("from", jid().full());
	
	XMPPServer::sessions_t::iterator it;
	XMPPServer::reslist_t reslist;
	XMPPServer::reslist_t::iterator jt;
	
	it = server->onliners.find(stanza->to().bare());
	if(it != server->onliners.end()) {
		for(jt = it->second.begin(); jt != it->second.end(); jt++) {
			// Отправить нужно на ресурс(ы) с наибольшим приоритетом, пока шлю на все.
			jt->second->sendStanza(stanza);
		}
	} else {
		cout << "Offline message for: " << stanza->to().bare() << endl;
	}
}

void XMPPStream::onPresenceStanza(Stanza *stanza)
{
	resource_priority = 0; // TODO
	
	for(XMPPServer::sessions_t::iterator it = server->onliners.begin(); it != server->onliners.end(); it++) {
		stanza->tag()->insertAttribute("to", it->first);
		for(XMPPServer::reslist_t::iterator jt = it->second.begin(); jt != it->second.end(); jt++) {
			jt->second->sendStanza(stanza);
		}
	}
}


/**
* Событие: начало потока
*/
void XMPPStream::onStartStream(const std::string &name, const attributes_t &attributes)
{
	cout << "new stream" << endl;
	initXML();
	startElement("stream:stream");
	setAttribute("xmlns", "jabber:client");
	setAttribute("xmlns:stream", "http://etherx.jabber.org/streams");
	setAttribute("id", "123456"); // Требования к id — непредсказуемость и уникальность
	setAttribute("from", "localhost");
	setAttribute("version", "1.0");
	setAttribute("xml:lang", "en");
	
	startElement("stream:features");
	if ( state == init )
	{
		client_jid.setHostname(attributes.find("to")->second);
		startElement("mechanisms");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
			SASLServer::mechanisms_t list = server->getMechanisms();
			for(SASLServer::mechanisms_t::const_iterator pos = list.begin(); pos != list.end(); ++pos)
			{
				addElement("mechanism", *pos);
			}
		endElement("mechanisms");
		startElement("register");
			setAttribute("xmlns", "http://jabber.org/features/iq-register");
		endElement("register");
	}
	else
	{
		startElement("bind");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-bind");
		endElement("bind");
		/*
		startElement("session");
			setAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-session");
		endElement("session");
		*/
	}
	endElement("stream:features");
	flush();
}

/**
* Событие: конец потока
*/
void XMPPStream::onEndStream()
{
	cerr << "session closed" << endl;
	endElement("stream:stream");
}

/**
* JID потока
*/
JID XMPPStream::jid()
{
	return client_jid;
}

void XMPPStream::sendTag(ATXmlTag * tag) {
	startElement(tag->name());
	attributes_t attributes = tag->getAttributes();
	for(attributes_t::iterator it = attributes.begin(); it != attributes.end(); it++) {
		setAttribute(it->first, it->second);
	}
	nodes_list_t nodes = tag->getChildNodes();
	for(nodes_list_t::iterator it = nodes.begin(); it != nodes.end(); it++) {
		if((*it)->type == TTag) {
			sendTag((*it)->tag);
		} else {
			characterData((*it)->cdata);
		}
	}
	endElement(tag->name());
	// send(tag->asString()); — так будет куда проще…
}

void XMPPStream::sendStanza(Stanza * stanza) {
	sendTag(stanza->tag());
	flush();
}
