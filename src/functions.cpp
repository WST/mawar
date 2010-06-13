
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
