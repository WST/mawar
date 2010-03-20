
#include <logs.h>
#include <time.h>
#include <string>

FILE *stdlog = 0;

/**
* Открыть стандартый поток лог-файла
*/
bool open_stdlog(const char *path)
{
	FILE *f = fopen(path, "a");
	if ( f )
	{
		setlinebuf(f);
		if ( stdlog ) fclose(stdlog);
		stdlog = f;
		return true;
	}
	return false;
}

std::string logtime()
{
	char buf[80];
	time_t t = time(0);
	struct tm *tmp = localtime(&t);
	size_t len = strftime(buf, sizeof(buf), "%c", tmp);
	return std::string(buf, len);
}
