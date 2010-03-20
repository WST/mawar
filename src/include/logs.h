#ifndef MAWAR_LOGS_H
#define MAWAR_LOGS_H

#include <stdio.h>
#include <string>

extern FILE *stdlog;

/**
* Открыть стандартый поток лог-файла
*/
bool open_stdlog(const char *path);

std::string logtime();

#endif // MAWAR_LOGS_H
