/*
 * Copyright (c) 2000, Larry McVoy & Andrew Chang
 */
#include "bkd.h"

/*
 * Send the probe keys for this lod.
 * XXX - all the lods will overlap near the top, if we have a lot of
 * lods, this may become an issue.
 */
lod_probekey(sccs *s, delta *d, FILE *f)
{
	int	i, j;
	char	key[MAXKEY];

	/*
	 * Phase 1, send the probe keys.
	 * NB: must be in most recent to least recent order.
	 */
	for (i = 1; d && (d != s->tree); i *= 2) {
		for (j = i; d && --j; d = d->parent);
		if (d) {
			assert(d->type == 'D');
			sccs_sdelta(s, d, key);
			fprintf(f, "%s\n", key);
		}
	}

	/*
	 * Always send the root key because we want to force a match.
	 * No match is a error condition.
	 */
	sccs_sdelta(s, sccs_ino(s), key);
	fprintf(f, "%s\n", key);
}

tag_probekey(sccs *s, FILE *f)
{
	delta	*d;
	int	i, j;
	char	key[MAXKEY];

	for (d = s->table; d && !d->ptag; d = d->next);
	unless (d) return;

	fputs("@TAG PROBE@\n", f);
	for (i = 1; d; i *= 2) {
		for (j = i; d->ptag && --j; d = sfind(s, d->ptag));
		sccs_sdelta(s, d, key);
		fprintf(f, "%s\n", key);
//fprintf(f, "%s (%s)\n", key, d->rev);
		unless (d->ptag) return;
	}
}

int
probekey_main(int ac, char **av)
{
	int	i;
	sccs	*s;
	delta	*d;
	char	s_cset[] = CHANGESET;
	char	rev[MAXREV+1];

	unless ((s = sccs_init(s_cset, 0, 0)) && s->tree) {
		fprintf(stderr, "Can't init changeset\n");
		exit(1);
	}
	for (i = 1; i <= 0xffff; ++i) {
		sprintf(rev, "%d.1", i);
		unless (d = findrev(s, rev)) break;
		while (d->kid && (d->kid->type == 'D')) d = d->kid;
		fputs("@LOD PROBE@\n", stdout);
		lod_probekey(s, d, stdout);
	}
	tag_probekey(s, stdout);
	fputs("@END PROBE@\n", stdout);
	sccs_free(s);
	return (0);
}

void
sccs_tagcolor(sccs *s, delta *d)
{
//fprintf(stderr, "TC(%s, %d)\n", d->rev, d->serial);
        if (d->ptag) sccs_tagcolor(s, sfind(s, d->ptag));
        if (d->mtag) sccs_tagcolor(s, sfind(s, d->mtag));
        d->flags |= D_VISITED;
}                 

int
listkey_main(int ac, char **av)
{
	sccs	*s;
	delta	*d = 0;
	int	c, debug = 0, quiet = 0, nomatch = 1;
	char	key[MAXKEY] = "", s_cset[] = CHANGESET;

	while ((c = getopt(ac, av, "dq")) != -1) {
		switch (c) {
		    case 'd':	debug = 1; break;
		    case 'q':	quiet = 1; break;
		    default:	fprintf(stderr,
					"usage: bk _listkey [-d] [-q]\n");
				return (5);
		}
	}
	unless ((s = sccs_init(s_cset, 0, 0)) && s->tree) {
		fprintf(stderr, "Can't init changeset\n");
		return(3); /* cset error */
	}

	/*
	 * Phase 1, get the probe keys.
	 */
	if (getline(0, key, sizeof(key)) <= 0) {
		unless (quiet) {
			fprintf(stderr, "Expected \"@LOD PROBE\"@, Got EOF\n");
			return (2);
		}
	}
	unless (streq("@LOD PROBE@", key)) {
		unless (quiet) {
			fprintf(stderr,
			    "Expected \"@LOD PROBE\"@, Got \"%s\"\n", key);
		}
		return(3); /* protocol error or repo locked */
	}

	if (debug) fprintf(stderr, "listkey: looking for match key\n");
	while (getline(0, key, sizeof(key)) > 0) {
		if (streq("@END PROBE@", key)) break;
		if (streq("@TAG PROBE@", key)) break;
		if (streq("@LOD PROBE@", key)) {
			d = 0;
			continue;
		}
		if (!d && (d = sccs_findKey(s, key))) {
			sccs_color(s, d);
			if (debug) {
				fprintf(stderr, "listkey: found a match key\n");
			}
			if (nomatch) out("@LOD MATCH@\n");	/* aka first */
			sccs_sdelta(s, d, key);
			out(key);
//out(" ("); out(d->rev); out(" "); out(d->type == 'D' ? "D":"R"); out(")");
			out("\n");
			nomatch = 0;

		}
	}
	if (nomatch) {
		if (debug) fprintf(stderr, "listkey: no match key\n");
		out("@NO MATCH@\n");
		out("@END@\n");
		return (1); /* package key mismatch */
	}

	if (streq("@TAG PROBE@", key)) {
		d = 0;
		while (getline(0, key, sizeof(key)) > 0) {
			if (streq("@END PROBE@", key)) break;
			if (!d && (d = sccs_findKey(s, key))) {
				sccs_tagcolor(s, d);
				out("@TAG MATCH@\n");
				sccs_sdelta(s, d, key);
				out(key);
//out(" ("); out(d->rev); out(" "); out(d->type == 'D' ? "D":"R"); out(")");
				out("\n");
			}
		}
	}
	out("@END MATCHES@\n");

	/*
	 * Phase 2, send the non marked keys.
	 */
	for (d = s->table; d; d = d->next) {
		if (d->flags & D_VISITED) continue;
		sccs_sdelta(s, d, key);
		out(key);
//out(" ("); out(d->rev); out(" "); out(d->type == 'D' ? "D":"R"); out(")");
		out("\n");
	}
	out("@END@\n");
	return (0);
}

/*
 * If there are multiple lods and a tag "lod" then we expect this:
 * @LOD MATCH@
 * <key>
 * <key>
 * ...
 * @TAG MATCH@
 * <tag key>
 * @END MATCHES@
 * <key>
 * <key>
 * ...
 * @END@
 */
int
prunekey(sccs *s, remote *r, int outfd,
	int quiet, int *local_only, int *remote_csets, int *remote_tags)
{
	char	key[MAXKEY] = "";
	delta	*d;
	int	rc = 0, rcsets = 0, rtags = 0, local = 0;

	unless (getline2(r, key, sizeof(key)) > 0) {
		unless (quiet) {
			fprintf(stderr,
			    "prunekey: expected @CMD@, got nothing.\n");
		}
		return (-1);
	}
	if (streq(key, "@NO MATCH@")) return (-2);
	if (strneq(key, "@FAIL@-", 7)) {
		unless (quiet) fprintf(stderr, "%s\n", &key[7]);
		return (-1);
	}
	if (streq(key, "@EMPTY TREE@")) {
		rc = -3;
		goto empty;
	}
	unless (streq(key, "@LOD MATCH@")) {
		unless (quiet) {
			fprintf(stderr,
			    "prunekey: protocol error: %s key\n");
		}
		return (-1);
	}

	/* Work through the LOD key matches and color the graph. */
	for ( ;; ) {
		unless (getline2(r, key, sizeof(key)) > 0) {
			perror("prunekey: expected key | @");
			exit(2);
		}
		if (key[0] == '@') break;
		d = sccs_findKey(s, key);
		assert(d);
		sccs_color(s, d);
	}

	/* Work through the tag key matches and color the tag graph. */
	if (streq(key, "@TAG MATCH@")) {
		for ( ;; ) {
			unless (getline2(r, key, sizeof(key)) > 0) {
				perror("prunekey: expected key | @");
				exit(2);
			}
			if (key[0] == '@') break;
			d = sccs_findKey(s, key);
			assert(d);
			sccs_tagcolor(s, d);
		}
	} else unless (streq(key, "@END MATCHES@")) {
		fprintf(stderr, "Protocol error, wanted @END MATCHES@\n");
		exit(2);
	}

	for ( ;; ) {
		unless (getline2(r, key, sizeof(key)) > 0) {
			perror("prunekey: expected key | @");
			exit(2);
		}
		if (streq("@END@", key)) break;
		if (d = sccs_findKey(s, key)) {
			d->flags |= D_VISITED;
		} else {
			if (sccs_istagkey(key)) {
				rtags++;
			} else {
				rcsets++;
			}
		}
	}

empty:	for (d = s->table; d; d = d->next) {
		if (d->flags & D_VISITED) continue;
		sprintf(key, "%u\n", d->serial);
		write(outfd, key, strlen(key));
		local++;
	}
	if (remote_csets) *remote_csets = rcsets;
	if (remote_tags) *remote_tags = rtags;
	if (local_only) *local_only = local;
	rc = local; 

done:	return (rc);
}