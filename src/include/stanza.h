
#ifndef MAWAR_STANZA_H
#define MAWAR_STANZA_H

#include <xml_tag.h>
#include <taghelper.h>
#include <jid.h>
#include <presence.h>

/**
* Класс станзы
*
* Класс Stanza наследует все свойства и семантику использования
* от TagHelper. Но Stanza не заменяет TagHelper, для работы с
* подэлементами рекомендуется использовать TagHelper, а не Stanza
*
* Хорошо:
*   void onStanza(Stanza iq) {
*     TagHelper query = iq["query"];
*     for(TagHelper item = query->firstChild("item"); item; item = query->nextChild("item", item))
*       ...
*   }
*
* Плохо:
*   void onStanza(Stanza iq) {
*     Stanza query = iq["query"];
*     for(Stanza item = query->firstChild("item"); item; item = query->nextChild("item", item))
*       ...
*   }
*/
class Stanza: public TagHelper
{
	public:
		Stanza() { }
		Stanza(ATXmlTag *tag): TagHelper(tag) { }
		Stanza(TagHelper tag): TagHelper(tag) { }
		
		JID from();
		JID to();
		std::string type();
		std::string id();
		void setFrom(JID &jid);
		
		static Stanza serverVersion(JID server, JID reply_to, std::string id);
		static Stanza badRequest(JID server, JID reply_to, std::string id);
		static Stanza presence(JID from, JID to, ClientPresence p);
		
		/**
		* Stream errors (RFC 3920, 4.7)
		* @param condition имя тега ошибки
		* @param message поясняющий текст
		* @param lang язык
		* @return сформированная станза
		*/
		static Stanza streamError(const std::string &condition, const std::string &message = "", const std::string &lang = "");
		
		/**
		* IQ error
		* @param stanza станза вызвавшая ошибку
		* @param condition имя тега ошибки
		* @param type тип (cancel, continue, modify, auth, wait)
		* @param message поясняющий текст
		* @param lang язык
		* @return сформированная станза
		*/
		static Stanza iqError(Stanza stanza, const std::string &condition, const std::string &type, const std::string &message = "", const std::string &lang = "");
		
		/**
		* Presence error
		* @param stanza станза вызвавшая ошибку
		* @param condition имя тега ошибки
		* @param type тип (cancel, continue, modify, auth, wait)
		* @param message поясняющий текст
		* @param lang язык
		* @return сформированная станза
		*/
		static Stanza presenceError(Stanza stanza, const std::string &condition, const std::string &type, const std::string &message = "", const std::string &lang = "");
};

#endif
