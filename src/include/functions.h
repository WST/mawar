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

#endif