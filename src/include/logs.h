#ifndef MAWAR_LOGS_H
#define MAWAR_LOGS_H

#include <stdio.h>
#include <string>

extern FILE *stdlog;

/**
* Открыть стандартый поток лог-файла
*/
bool open_stdlog(const char *path);

/**
* Открыть access.log
*/
bool open_access_log(const char *path);

/**
* Открыть error.log
*/
bool open_error_log(const char *path);

std::string logtime();

#endif // MAWAR_LOGS_H
