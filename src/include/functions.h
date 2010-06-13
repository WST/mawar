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

#endif
