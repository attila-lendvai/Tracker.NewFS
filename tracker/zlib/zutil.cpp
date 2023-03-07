/* zutil.c -- target dependent utility functions for the compression library
 * Copyright (C) 1995-2003 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
 * Adapted for C++ and inclusion in Tracker.NewFS 2005 Michael Lotz
 */

#include "zutil.h"

namespace zlib {

const char * const z_errmsg[10] = {
	"need dictionary",     /* Z_NEED_DICT       2  */
	"stream end",          /* Z_STREAM_END      1  */
	"",                    /* Z_OK              0  */
	"file error",          /* Z_ERRNO         (-1) */
	"stream error",        /* Z_STREAM_ERROR  (-2) */
	"data error",          /* Z_DATA_ERROR    (-3) */
	"insufficient memory", /* Z_MEM_ERROR     (-4) */
	"buffer error",        /* Z_BUF_ERROR     (-5) */
	"incompatible version",/* Z_VERSION_ERROR (-6) */
	""
};


const char *
zlibVersion()
{
    return ZLIB_VERSION;
}

uint32
zlibCompileFlags()
{
    uint32 flags;

    flags = 0;
    switch (sizeof(uint32)) {
    case 2:     break;
    case 4:     flags += 1;     break;
    case 8:     flags += 2;     break;
    default:    flags += 3;
    }
    switch (sizeof(uint32)) {
    case 2:     break;
    case 4:     flags += 1 << 2;        break;
    case 8:     flags += 2 << 2;        break;
    default:    flags += 3 << 2;
    }
    switch (sizeof(void *)) {
    case 2:     break;
    case 4:     flags += 1 << 4;        break;
    case 8:     flags += 2 << 4;        break;
    default:    flags += 3 << 4;
    }
    switch (sizeof(off_t)) {
    case 2:     break;
    case 4:     flags += 1 << 6;        break;
    case 8:     flags += 2 << 6;        break;
    default:    flags += 3 << 6;
    }

    return flags;
}


const char *
zError(int err)
{
    return ERR_MSG(err);
}

void *
zcalloc(void * /*opaque*/, unsigned items, unsigned size)
{
    return sizeof(uint32) > 2 ? (void *)malloc(items * size) :
                              (void *)calloc(items, size);
}

void
zcfree(void * /*opaque*/, void * ptr)
{
    free(ptr);
}

} // namespace zlib
