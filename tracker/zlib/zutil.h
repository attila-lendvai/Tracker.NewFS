/* zutil.h -- internal interface and configuration of the compression library
 * Copyright (C) 1995-2003 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
 * Adapted for C++ and inclusion in Tracker.NewFS 2005 Michael Lotz
 */

#ifndef ZUTIL_H
#define ZUTIL_H

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <malloc.h>

#define ZLIB_INTERNAL
#include "zlib.h"

namespace zlib {

extern const char * const z_errmsg[10]; /* indexed by 2-zlib_error */

#define ERR_MSG(err) z_errmsg[Z_NEED_DICT-(err)]
#define ERR_RETURN(strm,err) \
	return (strm->msg = (char*)ERR_MSG(err), (err))

#ifndef DEF_WBITS
#	define DEF_WBITS MAX_WBITS
#endif
/* default windowBits for decompression. MAX_WBITS is for compression only */

#if MAX_MEM_LEVEL >= 8
#	define DEF_MEM_LEVEL	8
#else
#	define DEF_MEM_LEVEL	MAX_MEM_LEVEL
#endif
/* default memLevel */

#define STORED_BLOCK 0
#define STATIC_TREES 1
#define DYN_TREES    2
/* The three kinds of block type */

#define MIN_MATCH  3
#define MAX_MATCH  258
/* The minimum and maximum match lengths */

#define PRESET_DICT 0x20 /* preset dictionary flag in zlib header */

#define OS_CODE	0xbe
#define fdopen(fd,mode) NULL /* No fdopen() */

#define zstrerror(errnum) strerror(errnum)

void *zcalloc(void *opaque, unsigned items, unsigned size);
void zcfree(void *opaque, void *ptr);

#define ZALLOC(strm, items, size) \
           (*((strm)->zalloc))((strm)->opaque, (items), (size))
#define ZFREE(strm, addr)  (*((strm)->zfree))((strm)->opaque, (void *)(addr))
#define TRY_FREE(s, p) { if (p) ZFREE(s, p); }

} // namespace zlib

#endif /* ZUTIL_H */
