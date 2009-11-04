#include <xmppdomain.h>
#include <string>
#include <stanza.h>
#include <xmppserver.h>

using namespace std;

/**
* Конструктор
* @param srv ссылка на сервер
* @param aName имя хоста
*/
XMPPDomain::XMPPDomain(XMPPServer *srv, const std::string &aName): server(srv), name(aName)
{
}

/**
* Деструктор
*/
XMPPDomain::~XMPPDomain()
{
}
