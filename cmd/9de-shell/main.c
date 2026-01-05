#include <u.h>
#include <libc.h>
#include <thread.h>

#include "srv9p.h"

/*
 * 9de-shell (contract owner, early)
 *
 * Owns placement + start order:
 *   - panel pinned to edge
 *   - dash is launched/toggled by panel (early)
 * Later: notifications, modules, workspace metadata.
 */

static char*
userhome(void)
{
	char *home = getenv("home");
	if(home == nil)
		home = getenv("HOME");
	if(home == nil)
		home = "/usr";
	return home;
}

static void
spawnrc(char *cmd)
{
	switch(rfork(RFPROC|RFNOWAIT|RFFDG)){
	case -1:
		return;
	case 0:
		execl("/bin/rc", "rc", "-c", cmd, nil);
		exits("exec");
	default:
		return;
	}
}

void threadmain(int argc, char **argv)
{
	int dev = 0;
	ARGBEGIN{
	case 'd':
		dev = 1;
		break;
	}ARGEND

	/* start control plane (/srv/9de) */
	srv9pstart();
	srv9ppostevent("shell boot");

	char cmd[2048];

	/* start panel pinned top with split logs */
	snprint(cmd, sizeof cmd,
		"mkdir -p %s/lib/9de/log %s/lib/9de/run; "
		"window 0,0,9999,34 rc -c '9de-panel >%s/lib/9de/log/9de-panel.log >[2]%s/lib/9de/log/9de-panel.err'",
		userhome(), userhome(), userhome(), userhome());
	spawnrc(cmd);

	/* dev: also start ui9demo */
	if(dev){
		snprint(cmd, sizeof cmd,
			"window -scroll rc -c 'ui9demo >%s/lib/9de/log/ui9demo.log >[2]%s/lib/9de/log/ui9demo.err'",
			userhome(), userhome());
		spawnrc(cmd);
	}

	for(;;) sleep(1000);
	threadexits(nil);
}
