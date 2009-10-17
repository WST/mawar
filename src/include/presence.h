
#ifndef MAWAR_PRESENCE_H
#define MAWAR_PRESENCE_H

#include <string>
#include <cstdio>


struct ClientPresence {
	enum show_t {Available, Unavailable, XA, DND, Free} show;
	std::string status_text;
	int priority;
	std::string getPriority() {
		char buf[40];
		sprintf(buf, "%d", priority);
		return std::string(buf);
	}
	void setShow(std::string show) {
		// lowercase(show)
		if(show == "available") show = Available;
		if(show == "unavailable") show = Unavailable;
		if(show == "xa") show = XA;
		if(show == "dnd") show = DND;
		if(show == "free") show = Free;
	};
	std::string getShow() {
		if(show == Available) return std::string("available");
		if(show == Unavailable) return std::string("unavailable");
		if(show == XA) return std::string("xa");
		if(show == DND) return std::string("dnd");
		if(show == Free) return std::string("free");
	}
};

#endif
