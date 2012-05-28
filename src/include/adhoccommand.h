
#ifndef MAWAR_ADHOCCOMMAND_H
#define MAWAR_ADHOCCOMMAND_H

#include <xml_tag.h>
#include <taghelper.h>
#include <stanza.h>
#include <string>

/**
* Класс описывающий Ad-Hoc Command
*
* Данный класс является оболочкой над классом Stanza
* и предоставляет дополнительные удобные методы
* для обработки Ad-Hoc комманд и форм
*
* XEP-0004: Data Forms
* http://xmpp.org/extensions/xep-0004.html
* 
* XEP-0050: Ad-Hoc Commands
* http://xmpp.org/extensions/xep-0050.html
* 
* XEP-0141: Data Forms Layout
* http://xmpp.org/extensions/xep-0141.html
*/
class AdHocCommand: public Stanza
{
public:
	AdHocCommand();
	AdHocCommand(Stanza stanza);
	AdHocCommand(ATXmlTag *tag);
	
	/**
	* Создать заготовку-ответ на комманду
	*/
	static AdHocCommand reply(Stanza stanza);
	
	/**
	* Вернуть атрибут sessionid
	*/
	std::string getSessionId();
	
	/**
	* Установить атрибут sessionid
	*/
	void setSessionId(const char *sid);
	void setSessionId(const std::string &sid);
	
	/**
	* Вернуть атрибут node (имя комманды)
	*/
	std::string getNode();
	
	/**
	* Установить атрибут node (имя комманды)
	*/
	void setNode(const char *node);
	void setNode(const std::string &node);
	
	/**
	* Вернуть атрибут status
	*/
	std::string getStatus();
	
	/**
	* Установить атрибут status
	*/
	void setStatus(const char *status);
	void setStatus(const std::string &status);
	
	/**
	* Вернуть тип формы
	*/
	std::string getType();
	
	/**
	* Установить тип формы
	*/
	void setType(const char *type);
	void setType(const std::string &type);
	
	/**
	* Проверка есть ли данные в форме
	* 
	* TRUE - клиент прислал данные
	* FALSE - данных нет, клиент запросил форму
	*/
	bool isSubmit();
	
	/**
	* TRUE - клиент отменил форму
	*/
	bool isCancel();
	
	/**
	* Вернуть заголовок формы
	*/
	std::string getTitle();
	
	/**
	* Установить заголовок формы
	*/
	void setTitle(const char *title);
	void setTitle(const std::string &title);
	
	/**
	* Вернуть инструкции
	*/
	std::string getInstructions();
	
	/**
	* Установить инструкции
	*/
	void setInstructions(const char *text);
	void setInstructions(const std::string &text);
	
	/**
	* Включить/отключить кнопку
	* 
	* Допустимые значения name:
	*   prev - кнопка "назад"
	*   next - кнопка "далее"
	*   complete - кнопка "завешить"
	*/
	void setButtonEnable(const char *name, bool enable = true);
	
	/**
	* Вернуть примечание
	*/
	std::string getNote();
	
	/**
	* Установить примечание
	*/
	void setNote(const char *text);
	void setNote(const std::string &text);
	
	/**
	* Найти описание поля
	*
	* Если поля нет, то возращается NULL
	*/
	TagHelper findField(const char *name);
	
	/**
	* Найти описание поля
	*
	* Если поля нет, то оно создается
	*/
	TagHelper lookupField(const char *name);
	
	/**
	* Установить тип поля
	*/
	void setField(const char *name, const char *type, const char *label = 0);
	
	/**
	* Вернуть значение поля
	*/
	std::string getFieldValue(const char *name, const char *default_value = 0);
	
	/**
	* Установить значение поля
	*/
	void setFieldValue(const char *name, const char *value);
};

#endif // MAWAR_ADHOCCOMMAND_H
