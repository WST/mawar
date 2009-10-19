
#include <virtualhost.h>
#include <xmppstream.h>

/**
* Конструктор
* @param srv ссылка на сервер
* @param aName имя хоста
* @param config конфигурация хоста
*/
VirtualHost::VirtualHost(XMPPServer *srv, const std::string &aName, VirtualHostConfig config): server(srv), name(aName) {
	
}

/**
* Деструктор
*/
VirtualHost::~VirtualHost() {
}

/**
* Вернуть имя хоста
*/
const std::string& VirtualHost::hostname() {
	return name;
}

void VirtualHost::handleVHostIq(Stanza stanza) {
	if(stanza->hasChild("query") && stanza.type() == "get") {
		// Входящие запросы информации
		std::string query_xmlns = stanza["query"]->getAttribute("xmlns");
		
		if(query_xmlns == "jabber:iq:version") {
			Stanza version = Stanza::serverVersion(name, stanza.from(), stanza.id());
			getStreamByJid(stanza.from())->sendStanza(version);
			delete version;
			return;
		}
		
		if(query_xmlns == "jabber:iq:roster") {
			// Отправить ростер
			/*
			startElement("iq");
				setAttribute("type", "result");
				setAttribute("id", stanza->getAttribute("id"));
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
			// Ниже идёт костыль — рассылка присутствий от всех онлайнеров
			// TODO: конечно же, декостылизация ;)
			for(XMPPServer::sessions_t::const_iterator it = server->onliners.begin(); it != server->onliners.end(); it++) {
				for(XMPPServer::reslist_t::const_iterator jt = it->second.begin(); jt != it->second.end(); jt++) {
					cout << "presence " << jt->second->presence().getPriority() << " from " << jt->second->jid().full() << " to " << jid().full() << endl;
					Stanza roster_presence = Stanza::presence(jt->second->jid(), jid(), jt->second->presence());
					sendStanza(roster_presence);
					delete roster_presence;
				}
			}
			*/
		}
	}
}

void VirtualHost::handlePresence(Stanza stanza) {
	// from уже определено ранее
	for(VirtualHost::sessions_t::iterator it = onliners.begin(); it != onliners.end(); it++) {
		stanza->setAttribute("to", it->first);
		for(VirtualHost::reslist_t::iterator jt = it->second.begin(); jt != it->second.end(); jt++) {
			jt->second->sendStanza(stanza);
		}
	}
}

void VirtualHost::handleMessage(Stanza stanza) {
	
	VirtualHost::sessions_t::iterator it;
	VirtualHost::reslist_t reslist;
	VirtualHost::reslist_t::iterator jt;
	
	it = onliners.find(stanza.to().username());
	if(it != onliners.end()) {
		// Проверить, есть ли ресурс, если он указан
		JID to = stanza.to();
		if(!to.resource().empty()) {
			jt = it->second.find(to.resource());
			if(jt != it->second.end()) {
				jt->second->sendStanza(stanza);
				return;
			}
			// Не отправили на выбранный ресурс, смотрим дальше…
		}
		std::list<XMPPStream *> sendto_list;
		std::list<XMPPStream *>::iterator kt;
		int max_priority = 0;
		for(jt = it->second.begin(); jt != it->second.end(); jt++) {
			if(jt->second->presence().priority == max_priority) {
				sendto_list.push_back(jt->second);
			} else if(jt->second->presence().priority > max_priority) {
				sendto_list.clear();
				sendto_list.push_back(jt->second);
			}
		}
		if(sendto_list.empty()) {
			//cout << "Offline message for: " << stanza.to().bare() << endl;
			return;
		}
		for(kt = sendto_list.begin(); kt != sendto_list.end(); kt++) {
			(*kt)->sendStanza(stanza);
		}
	} else {
		//cout << "Offline message for: " << stanza.to().bare() << endl;
	}
}

void VirtualHost::handleIq(Stanza stanza) {
	if(stanza.to().username().empty()) {
		handleVHostIq(stanza);
	}
	if(stanza.to().resource().empty()) {
		/*
		<iq from="***" type="error" to="**" id="blah" >
		<query xmlns="foo"/>
		<error type="modify" code="400" >
		<bad-request xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/>
		</error>
		</iq>
		*/
	}
	XMPPStream *stream = getStreamByJid(stanza.to());
	if(stream == 0) {
		/*
		<iq from="averkov@jabberid.org/test" type="error" xml:lang="ru-RU" to="admin@underjabber.net.ru/home" id="blah" >
		<query xmlns="foo"/>
		<error type="wait" code="404" >
		<recipient-unavailable xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/>
		</error>
		</iq>
		*/
	}
}

XMPPStream *VirtualHost::getStreamByJid(JID jid) {
	sessions_t::iterator it = onliners.find(jid.username());
	if(it != onliners.end()) {
		reslist_t::iterator jt = it->second.find(jid.resource());
		if(jt != it->second.end()) {
			return jt->second;
		}
	}
	return 0;
}

/**
* Событие появления нового онлайнера
* @param stream поток
*/
void VirtualHost::onOnline(XMPPStream *stream) {
	reslist_t reslist;
	reslist[stream->jid().resource()] = stream;
	onliners[stream->jid().username()] = reslist;
	//cout << stream->jid().full() << " is online :-)\n";
}

/**
* Событие ухода пользователя в офлайн
*/
void VirtualHost::onOffline(XMPPStream *stream) {
	onliners[stream->jid().bare()].erase(stream->jid().resource());
	if(onliners[stream->jid().username()].empty()) {
		// Если карта ресурсов пуста, то соответствующий элемент вышестоящей карты нужно удалить
		onliners.erase(stream->jid().username());
	}
	//cout << stream->jid().full() << " is offline :-(\n";
}
