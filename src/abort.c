/* Copyright (c) 2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private char	*abort_help = "usage: abort [-f] [repository root]\n";
void	abort_patch();

/*
 * Abort a pull/resync by deleting the RESYNC dir and the patch file in
 * the PENDING dir.
 */
int
abort_main(int ac, char **av)
{
	int	c, force = 0;
	char	buf[MAXPATH];

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fputs(abort_help, stderr);
		return (1);
	}
	while ((c = getopt(ac, av, "f")) != -1) {
		switch (c) {
		    case 'f': force = 1; break;
		    default:
			goto usage;
		}
	}
	if (av[optind]) chdir(av[optind]);
	sccs_cd2root(0, 0);
	unless (exists(ROOT2RESYNC)) {
		fprintf(stderr, "No RESYNC dir, nothing to abort.\n");
		exit(0);
	}
	unless (force) {
		prompt("Abort update? (y/n)", buf);
		switch (buf[0]) {
		    case 'y':
		    case 'Y':
			break;
		    default:
			fprintf(stderr, "Not aborting.\n");
			exit(0);
		}
	}
	abort_patch();
	exit(0);
}

void
abort_patch()
{
	char	buf[MAXPATH];
	char	pendingFile[MAXPATH];
	FILE	*f;

	unless (exists(ROOT2RESYNC)) chdir(RESYNC2ROOT);
	unless (exists(ROOT2RESYNC)) {
		fprintf(stderr, "abort: can't find RESYNC dir\n");
		fprintf(stderr, "abort: nothing removed.\n");
		exit(1);
	}

	/*
	 * Get the patch file name from RESYNC before deleting RESYNC.
	 */
	sprintf(buf, "%s/%s", ROOT2RESYNC, "BitKeeper/tmp/patch");
	unless (f = fopen(buf, "rb")) {
		fprintf(stderr, "Warning: no BitKeeper/tmp/patch\n");
		pendingFile[0] = 0;
	} else {
		fnext(pendingFile, f);
		chop(pendingFile);
	}
	fclose(f);

	assert(exists("RESYNC"));
	sprintf(buf, "%s -rf RESYNC", RM);
	system(buf);
	if (pendingFile[0]) unlink(pendingFile);
	rmdir(ROOT2PENDING);
	repository_wrunlock(1);
	repository_lockers(0);
	exit(0);
}