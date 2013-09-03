# Mawar XMPP daemon

Mawar is a lighweight XMPP/Jabber server software originally written in C++ by Alexey Zolotov (shade) and currently maintained by Ilya Averkov (WST). It was meant to be the „official” XMPP server software of [SmartCommunity](http://jsmart.web.id), but recently turned into an open source project.

Mawar can be built only under Linux. It will certainly *not* work under Windows or even FreeBSD, because it uses Linux native system calls.

Installing Mawar should not be hard. First, you have to install the following dependencies using your Linux distribution’s package system:

* expat — an XML parser
* GnuTLS — transport layer security
* libgsasl — GNU SASL library
* libmysqlclient — MySQL client library
* uDNS — an asyncronous DNS resolver library
* GeoIP — IP geolocation library

Compile and install nanosoft libraries:

```bash
git clone https://github.com/WST/nanosoft.git
cd nanosoft
cmake .
make
sudo make install
```

Compile the XMPP server itself:

```bash
git clone https://github.com/WST/mawar.git
cd mawar
cmake .
make
mkdir /etc/mawar
cp dist/config.xml.example /etc/mawar/main.xml
nano /etc/mawar/main.xml
./maward
```
There is currently no installation script for the server. Only MySQL storage is currently supported. MySQL database schema can be found in dist/database.sql.
