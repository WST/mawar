#ifndef MAWAR_TAGHELPER_H
#define MAWAR_TAGHELPER_H

#include <xml-types.h>
#include <xml-tag.h>

/**
* Вспомогательный класс упрощающий работу с XML-тегами (XmlTag)
*
* Данный класс является умным "указателем" на XmlTag,
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
	XmlTag *tag;
public:
	/**
	* Конструктор по умолчанию - указатель на NULL
	*/
	TagHelper(): tag(0) { }
	
	/**
	* Конструктор с инициализацией тега
	*/
	TagHelper(XmlTag *pTag): tag(pTag) { }
	
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
	* Преобразование к XmlTag*
	*
	* Просто возвращаем тег:
	*   TagHelper foo = new XmlTag("foo");
	*   XmlTag *bar = foo; // здесь используется преобразование
	*/
	operator XmlTag* () {
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
	void operator = (const std::string &text) {
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
	* Добавить тег
	*
	* Если у тега нет родителя, то он добавляется в потомки
	* @TODO Если у тега есть родитель, то добавляется его копия
	*
	* @param t добавляемый тег
	* @return добавленный тег
	*/
	XmlTag* operator += (XmlTag *t);
	
	/**
	* Селектор
	*
	* Возращает указываемый тег
	*
	* Нужно обратиться к свойству XmlTag?
	*  - нет ничего проще:
	*     cout << tag["foo"]->getAttribute("bar");
	*     tag->setAttribute("bar", "banana");
	*/
	XmlTag* operator -> () {
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
		return this->operator [] (path.c_str());
	}
};

#endif // MAWAR_TAGHELPER_H
