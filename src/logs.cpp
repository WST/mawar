
#include <logs.h>
#include <time.h>
#include <string>

FILE *stdlog;

std::string logtime()
{
	char buf[80];
	time_t t = time(0);
	struct tm *tmp = localtime(&t);
	size_t len = strftime(buf, sizeof(buf), "%c", tmp);
	return std::string(buf, len);
}
