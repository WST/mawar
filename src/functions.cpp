
#include <functions.h>

unsigned long int id_counter;

std::string getUniqueId() {
	char buf[40];
	sprintf(buf, "mawar_%x", id_counter++);
	return std::string(buf);
}

void mawarInformation(std::string text) {
	std::cout << "\033[22;37m" << text << "\033[0m" << std::endl;
}

void mawarWarning(std::string text) {
	std::cout << "\033[01;33m" << text << "\033[0m" << std::endl;
}

void mawarError(std::string text, int exitcode) {
	std::cerr << "\033[22;31m" << text << "\033[0m" << std::endl;
	if(exitcode != 0) {
		exit(exitcode);
	}
}

/**
* Преобразовать двоичные данные в шестнадцатеричный вид
* @param dest буфер строки (должен быть достаточного размера - 2 * len + 1)
* @param src двоичные данные
* @param len размер двоичных данных
*/
void bin2hex(char *dest, const void *src, size_t len)
{
	const unsigned char *p = static_cast<const unsigned char*>(src);
	int s;
	for(size_t i = 0; i < len; i++, dest+=s, p++)
	{
		s = sprintf(dest, "%02x", *p);
	}
}
