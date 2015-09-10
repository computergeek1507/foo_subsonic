#include "foo_subsonic.h"

// Sample initquit implementation. See also: initquit class documentation in relevant header.

class myinitquit : public initquit {
public:
	void on_init() {		
		console::print("Subsonic component: on_init()");		
	}
	void on_quit() {
		console::print("Subsonic component: on_quit()");
	}
};


static initquit_factory_t<myinitquit> g_myinitquit_factory;
