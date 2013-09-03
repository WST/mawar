
#include <functions.h>
#include <gsasl.h>
#include <jid.h>

unsigned long int id_counter;

std::string getUniqueId() {
	char buf[40];
	sprintf(buf, "mawar_%lx", id_counter++);
	return std::string(buf);
}

/**
* Проверить корректность имени пользователя
*/
bool verifyUsername(const std::string &username)
{
	if(username.empty()) return false;
	if(username.find("\"") != std::string::npos) return false;
	if(username.find("\'") != std::string::npos) return false;
	if(username.find("&") != std::string::npos) return false;
	if(username.find("<") != std::string::npos) return false;
	if(username.find(">") != std::string::npos) return false;
	if(username.find("@") != std::string::npos) return false;
	if(username.find(" ") != std::string::npos) return false;
	return true;
}

/**
* Проверить корректность имени хоста
*/
bool verifyHostname(const std::string &hostname)
{
	if(hostname.empty()) return false;
	if(hostname.find("\"") != std::string::npos) return false;
	if(hostname.find("\'") != std::string::npos) return false;
	if(hostname.find("&") != std::string::npos) return false;
	if(hostname.find("<") != std::string::npos) return false;
	if(hostname.find(">") != std::string::npos) return false;
	if(hostname.find("@") != std::string::npos) return false;
	if(hostname.find(" ") != std::string::npos) return false;
	return true;
}

/**
* Проверить корректность JID
*/
bool verifyJID(const std::string &jid)
{
	JID j(jid);
	return verifyUsername(j.username()) && verifyHostname(j.hostname());
}

std::string mawarPrintInteger(unsigned long int number) {
	char buf[40];
	sprintf(buf, "%lu", number);
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

/**
* Вычислить sha1-хеш
*/
std::string sha1(const std::string &data)
{
	char *key;
	if ( gsasl_sha1(data.c_str(), data.length(), &key) == GSASL_OK )
	{
		char hash[80];
		bin2hex(hash, key, 20);
		gsasl_free(key);
		return hash;
	}
	return "";
}
