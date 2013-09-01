
#include "geoip.h"

GeoIPValidator::GeoIPValidator() {
	geoip = GeoIP_open("GeoIP.dat", GEOIP_STANDARD);
}

GeoIPValidator::~GeoIPValidator() {
	delete geoip;
}

bool GeoIPValidator::checkGeoIP(const char *ip_addr) {
	return true;
}

