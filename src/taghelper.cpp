#include <taghelper.h>
#include <string.h>
#include <stdio.h>

/**
* Добавить тег
*
* Если у тега нет родителя, то он добавляется в потомки
* @TODO Если у тега есть родитель, то добавляется его копия
*
* @param t добавляемый тег
* @return добавленный тег
*/
ATXmlTag* TagHelper::operator += (ATXmlTag *t)
{
	if ( t->getParent() != 0 )
	{
		fprintf(stderr, "[TagHelper::operator += (ATXmlTag *)] TODO Если у тега есть родитель, то добавляется его копия\n");
		return t;
	}
	tag->insertChildElement(t);
	return t;
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
TagHelper TagHelper::operator [] (const char *path)
{
	ATXmlTag *cur = tag;
	ATXmlTag *child;
	const char *remain = strchr(path, '/');
	while ( remain ) {
		std::string name(path, remain);
		
		child = cur->firstChild(name.c_str());
		if ( child == 0 ) {
			child = new ATXmlTag(name);
			cur->insertChildElement(child);
		}
		cur = child;
		
		path = remain + 1;
		remain = strchr(path, '/');
	}
	
	child = tag->firstChild(path);
	if ( child == 0 ) {
		child = new ATXmlTag(path);
		cur->insertChildElement(child);
	}
	
	return child;
}

/*
TagHelper TagHelper::operator [] (const std::string &path) {
	return this->operator [] (path.c_str());
}
*/