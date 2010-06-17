
#include "geoip.h"

GeoIPValidator::GeoIPValidator() {
	geoip = GeoIP_open("GeoIP.dat", GEOIP_STANDARD);
}

GeoIPValidator::~GeoIPValidator() {
	delete geoip;
}

bool GeoIPValidator::checkGeoIP(const char *ip_addr) {
	const char* code = GeoIP_country_code_by_addr(geoip, ip_addr);
	if(code == "RU" || code == "US" || code == "ID") {
		return true;
	} else {
		return false;
	}
}

