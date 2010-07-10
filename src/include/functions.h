#ifndef MAWAR_FUNCTIONS_H
#define MAWAR_FUNCTIONS_H

#include <string>
#include <cstdio>
#include <iostream>
#include <cstdlib>

/**
* получить текущий уникальный ID
* @return std::string уникальый ID
*/
std::string getUniqueId();

// Скопировано из Lana, авось пригодится
std::string mawarPrintInteger(unsigned long int number);
void mawarInformation(std::string text);
void mawarWarning(std::string text);
void mawarError(std::string text, int exitcode = 0);

/**
* Преобразовать двоичные данные в шестнадцатеричный вид
* @param dest буфер строки (должен быть достаточного размера - 2 * len)
* @param src двоичные данные
* @param len размер двоичных данных
*/
void bin2hex(char *dest, const void *src, size_t len);

/**
* Вычислить sha1-хеш
*/
std::string sha1(const std::string &data);

#endif // MAWAR_FUNCTIONS_H
