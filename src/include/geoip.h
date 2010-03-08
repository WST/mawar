#ifndef MAWAR_GEOIP_H
#define MAWAR_GEOIP_H

#include <GeoIP.h>

class GeoIPValidator
{
	public:
		GeoIPValidator();
		~GeoIPValidator();
		bool checkGeoIP(const char *ip_addr);
	
	private:
		GeoIP* geoip; // база GeoIP
};

#endif
