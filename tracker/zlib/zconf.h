/* zconf.h -- configuration of the zlib compression library
 * Copyright (C) 1995-2003 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
 * Adapted for C++ and inclusion in Tracker.NewFS 2005 Michael Lotz
 */

#ifndef ZCONF_H
#define ZCONF_H

#define MAX_MEM_LEVEL 9
#define MAX_WBITS   15 /* 32K LZ77 window */

#include <sys/types.h> /* for off_t */
#include <unistd.h>    /* for SEEK_* and off_t */

#endif /* ZCONF_H */
