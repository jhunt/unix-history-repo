/*
 * Copyright (c) 1983 Eric P. Allman
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "@(#)sysexits.c	6.2 (Berkeley) %G%";
#endif /* not lint */

#include <sysexits.h>

/*
 *  SYSEXITS.C -- error messages corresponding to sysexits.h
 */
char *SysExMsg[] = {
	/* 64 USAGE */		"500 Bad usage",
	/* 65 DATAERR */	"501 Data format error",
	/* 66 NOINPUT */	"550 Cannot open input: %s",
	/* 67 NOUSER */		"550 User unknown",
	/* 68 NOHOST */		"550 Host unknown",
	/* 69 UNAVAILABLE */	"554 Service unavailable",
	/* 70 SOFTWARE */	"554 Internal error: %s",
	/* 71 OSERR */		"451 Operating system error: %s",
	/* 72 OSFILE */		"554 System file missing: %s",
	/* 73 CANTCREAT */	"550 Can't create output: %s",
	/* 74 IOERR */		"451 I/O error: %s",
	/* 75 TEMPFAIL */	"250 Deferred",
	/* 76 PROTOCOL */	"554 Remote protocol error",
	/* 77 NOPERM */		"550 Insufficient permission: %s",
	/* 78 CONFIG */		"554 Local configuration error",
};

int N_SysEx = sizeof(SysExMsg) / sizeof(SysExMsg[0]);
