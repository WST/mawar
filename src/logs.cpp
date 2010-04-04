
#include <logs.h>
#include <time.h>
#include <string>

FILE *stdlog = 0;
FILE *access_log = 0;

/**
* Открыть лог файл
* @param log ссылка на файловую переменную
* @param path путь к лог файлу
*/
static bool open_log(FILE *&log, const char *path)
{
	FILE *f = fopen(path, "a");
	if ( f )
	{
		setlinebuf(f);
		if ( log ) fclose(log);
		log = f;
		return true;
	}
	return false;
}

/**
* Открыть стандартый поток лог-файла
*/
bool open_stdlog(const char *path)
{
	return open_log(stdlog, path);
}

/**
* Открыть access.log
*/
bool open_access_log(const char *path)
{
	return open_log(access_log, path);
}

/**
* Открыть error.log
*/
bool open_error_log(const char *path)
{
	return open_log(stderr, path);
}

std::string logtime()
{
	char buf[80];
	time_t t = time(0);
	struct tm *tmp = localtime(&t);
	size_t len = strftime(buf, sizeof(buf), "%c", tmp);
	return std::string(buf, len);
}
