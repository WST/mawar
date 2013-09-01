cmake_minimum_required(VERSION 2.8)
project(mawar)

add_definitions(-DHAVE_GNUTLS)

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall")
SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall")

set(MAWAR_SOURCE src/adhoccommand.cpp src/form.cpp src/logs.cpp src/serverstatus.cpp src/virtualhost.cpp src/xmppclient.cpp src/xmppserveroutput.cpp src/attagparser.cpp src/functions.cpp src/main.cpp src/stanza.cpp src/xep0114.cpp src/xmppdomain.cpp src/xmppstream.cpp src/command.cpp src/geoip.cpp src/tagbuilder.cpp src/xep0114listener.cpp src/xmppserver.cpp src/configfile.cpp src/jid.cpp src/s2slistener.cpp src/taghelper.cpp src/xml_tag.cpp src/xmppserverinput.cpp)
include_directories(src/include)
add_executable(maward ${MAWAR_SOURCE})
target_link_libraries(maward ns_core ns_netd ns_xml ns_sasl mysqlclient udns pcre expat z stdc++  gsasl  gnutls GeoIP)
