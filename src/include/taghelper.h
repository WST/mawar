#ifndef MAWAR_TAGHELPER_H
#define MAWAR_TAGHELPER_H

#include <xml_types.h>
#include <xml_tag.h>

/**
* Вспомогательный класс упрощающий работу с XML-тегами (ATXmlTag)
*
* Данный класс является умным "указателем" на ATXmlTag,
* реализующим специальные методы и операторы для модификации тегов
*
* @note Большиство методов достаточно тривиальные,
*   поэтому реализованны в виде inline-функций
*/
class TagHelper
{
protected:
	/**
	* Собственно тег
	*/
	ATXmlTag *tag;
public:
	/**
	* Конструктор по умолчанию - указатель на NULL
	*/
	TagHelper(): tag(0) { }
	
	/**
	* Конструктор с инициализацией тега
	*/
	TagHelper(ATXmlTag *pTag): tag(pTag) { }
	
	/**
	* Конструктор копии, только копируем указатель
	*/
	TagHelper(const TagHelper &tagh): tag(tagh.tag) { }
	
	/**
	* Преобразование к bool
	*
	* Нужно проверить на NULL?
	*  - нет ничего проще:
	*     if ( tag ) is not null
	*     else is null
	*/
	operator bool () {
		return tag != 0;
	}
	
	/**
	* Проверка на NULL
	*/
	bool operator ! () {
		return tag == 0;
	}
	
	/**
	* Преобразование к ATXmlTag*
	*
	* Просто возвращаем тег:
	*   TagHelper foo = new ATXmlTag("foo");
	*   ATXmlTag *bar = foo; // здесь используется преобразование
	*/
	operator ATXmlTag* () {
		return tag;
	}
	
	/**
	* Преобразование к строке
	*
	* Возращает рекурсивную конкатенацию всех секций CDATA
	*
	* Нужно получить значение тега foo?
	*  - нет ничего проще:
	*      string s = tag["foo"];
	*/
	operator std::string () {
		return tag->getCharacterData();
	}
	
	/**
	* Оператор присваивания
	*
	* Удаляет всех потомков и добавляет одну секцию CDATA
	*
	* Нужно изменить значение тега foo?
	*  - нет ничего проще:
	*     tag["foo"] = "Hello world!";
	*/
	const std::string & operator = (const std::string &text) {
		tag->clear();
		tag->insertCharacterData(text);
	}
	
	/**
	* Оператор добавления CDATA
	*
	* Добавить секцию CDATA в конец тега
	*/
	void operator += (const std::string &text) {
		tag->insertCharacterData(text);
	}
	
	/**
	* Селектор
	*
	* Возращает указываемый тег
	*
	* Нужно обратиться к свойству ATXmlTag?
	*  - нет ничего проще:
	*     cout << tag["foo"]->getAttribute("bar");
	*     tag->setAttribute("bar", "banana");
	*/
	ATXmlTag* operator -> () {
		return tag;
	}
	
	/**
	* Индексный оператор
	*
	* Ищет первый тег по указанному пути
	*
	* если найден, то возвращает для него хелпер
	* если не найден, то создает все необходимые
	* промежуточные элементы и возращает хелпер
	* созданного элемента.
	*/
	TagHelper operator [] (const char *path);
	TagHelper operator [] (const std::string &path) {
		this->operator [] (path.c_str());
	}
};

#endif // MAWAR_TAGHELPER_H
