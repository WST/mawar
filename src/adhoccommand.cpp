
#include <adhoccommand.h>

AdHocCommand::AdHocCommand()
{
}

AdHocCommand::AdHocCommand(Stanza stanza): Stanza((XmlTag*)stanza)
{
}

AdHocCommand::AdHocCommand(XmlTag *tag): Stanza(tag)
{
}

/**
* Создать заготовку-ответ на комманду
*/
AdHocCommand AdHocCommand::reply(Stanza stanza)
{
	Stanza reply = new XmlTag("iq");
	reply->setAttribute("from", stanza.to().full());
	reply->setAttribute("to", stanza.from().full());
	reply->setAttribute("id", stanza->getAttribute("id"));
	reply->setAttribute("type", "result");
	Stanza command = reply["command"];
	command->setDefaultNameSpaceAttribute("http://jabber.org/protocol/commands");
	command->setAttribute("node", stanza["command"]->getAttribute("node"));
	command["x"]->setDefaultNameSpaceAttribute("jabber:x:data");
	return reply;
}

/**
* Вернуть атрибут sessionid
*/
std::string AdHocCommand::getSessionId()
{
	return (*this)["command"]->getAttribute("sessionid");
}

/**
* Установить атрибут sessionid
*/
void AdHocCommand::setSessionId(const char *sid)
{
	(*this)["command"]->setAttribute("sessionid", sid);
}

/**
* Установить атрибут sessionid
*/
void AdHocCommand::setSessionId(const std::string &sid)
{
	(*this)["command"]->setAttribute("sessionid", sid);
}

/**
* Вернуть атрибут node (имя комманды)
*/
std::string AdHocCommand::getNode()
{
	return (*this)["command"]->getAttribute("node");
}

/**
* Установить атрибут node (имя комманды)
*/
void AdHocCommand::setNode(const char *node)
{
	(*this)["command"]->setAttribute("node", node);
}

/**
* Установить атрибут node (имя комманды)
*/
void AdHocCommand::setNode(const std::string &node)
{
	(*this)["command"]->setAttribute("node", node);
}

/**
* Вернуть атрибут status
*/
std::string AdHocCommand::getStatus()
{
	return (*this)["command"]->getAttribute("status");
}

/**
* Установить атрибут status
*/
void AdHocCommand::setStatus(const char *status)
{
	(*this)["command"]->setAttribute("status", status);
}

/**
* Установить атрибут status
*/
void AdHocCommand::setStatus(const std::string &status)
{
	(*this)["command"]->setAttribute("status", status);
}

/**
* Вернуть заголовок формы
*/
std::string AdHocCommand::getTitle()
{
	return (*this)["command"]["x"]["title"]->getCharacterData();
}

/**
* Вернуть тип формы
*/
std::string AdHocCommand::getType()
{
	TagHelper cmd = (*this)["command"];
	TagHelper xdata = cmd->firstChild("x");
	if ( xdata ) return xdata->getAttribute("type");
	return "";
}

/**
* Установить тип формы
*/
void AdHocCommand::setType(const char *type)
{
	(*this)["command"]["x"]->setAttribute("type", type);
}

/**
* Установить тип формы
*/
void AdHocCommand::setType(const std::string &type)
{
	(*this)["command"]["x"]->setAttribute("type", type);
}


/**
* Проверка есть ли данные в форме
* 
* TRUE - клиент прислал данные
* FALSE - данных нет, клиент запросил форму
*/
bool AdHocCommand::isSubmit()
{
	TagHelper cmd = (*this)["command"];
	TagHelper xdata = cmd->firstChild("x");
	return xdata && xdata->getAttribute("type") == "submit";
}

/**
* TRUE - клиент отменил форму
*/
bool AdHocCommand::isCancel()
{
	return (*this)["command"]->getAttribute("action") == "cancel";
}

/**
* Установить заголовок формы
*/
void AdHocCommand::setTitle(const char *title)
{
	(*this)["command"]["x"]["title"] = title;
}

/**
* Установить заголовок формы
*/
void AdHocCommand::setTitle(const std::string &title)
{
	(*this)["command"]["x"]["title"] = title;
}

/**
* Вернуть инструкции
*/
std::string AdHocCommand::getInstructions()
{
	return (*this)["command"]["x"]["instructions"]->getCharacterData();
}

/**
* Установить инструкции
*/
void AdHocCommand::setInstructions(const char *text)
{
	(*this)["command"]["x"]["instructions"] = text;
}

/**
* Установить инструкции
*/
void AdHocCommand::setInstructions(const std::string &text)
{
	(*this)["command"]["x"]["instructions"] = text;
}

/**
* Включить/отключить кнопку
*/
void AdHocCommand::setButtonEnable(const char *name, bool enable)
{
	if ( enable )
	{
		(*this)["command"]["actions"][name];
	}
	else
	{
		TagHelper actions = (*this)["command"]->firstChild("actions");
		if ( actions )
		{
			actions->removeChild(name);
		}
	}
}

/**
* Вернуть примечание
*/
std::string AdHocCommand::getNote()
{
	return (*this)["command"]["note"]->getCharacterData();
}

/**
* Установить примечание
*/
void AdHocCommand::setNote(const char *text)
{
	TagHelper note = (*this)["command"]["note"];
	note->setAttribute("type", "info");
	note = text;
}

/**
* Установить примечание
*/
void AdHocCommand::setNote(const std::string &text)
{
	TagHelper note = (*this)["command"]["note"];
	note->setAttribute("type", "info");
	note = text;
}

/**
* Найти описание поля
*
* Если поля нет, то возращается NULL
*/
TagHelper AdHocCommand::findField(const char *name)
{
	TagHelper xdata = (*this)["command"]["x"];
	TagHelper field = xdata->firstChild("field");
	while ( field )
	{
		if ( field->getAttribute("var") == name ) return field;
		field = xdata->nextChild("field", field);
	}
	return 0;
}

/**
* Найти описание поля
*
* Если поля нет, то оно создается
*/
TagHelper AdHocCommand::lookupField(const char *name)
{
	TagHelper field = findField(name);
	if ( field ) return field;
	field = new XmlTag("field");
	field->setAttribute("var", name);
	(*this)["command"]["x"] += (XmlTag*)field;
	return field;
}

/**
* Установить тип поля
*/
void AdHocCommand::setField(const char *name, const char *type, const char *label)
{
	TagHelper field = lookupField(name);
	field->setAttribute("type", type);
	if ( label ) field->setAttribute("label", label);
}

/**
* Вернуть значение поля
*/
std::string AdHocCommand::getFieldValue(const char *name, const char *default_value)
{
	TagHelper field = findField(name);
	if ( field )
	{
		TagHelper value = field->firstChild("value");
		if ( value ) return value;
	}
	return default_value;
}

/**
* Установить значение поля
*/
void AdHocCommand::setFieldValue(const char *name, const char *value)
{
	TagHelper field = lookupField(name);
	field["value"] = value;
}
