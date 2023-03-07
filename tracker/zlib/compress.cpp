/* compress.c -- compress a memory buffer
 * Copyright (C) 1995-2002 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
 * Adapted for C++ and inclusion in Tracker.NewFS 2005 Michael Lotz
 */

#define ZLIB_INTERNAL
#include "zlib.h"

namespace zlib {

int
compress2(uint8 *dest, uint32 *destLen, const uint8 *source, uint32 sourceLen,
	int level)
{
    z_stream stream;
    int err;

    stream.next_in = (uint8 *)source;
    stream.avail_in = (uint32)sourceLen;
    stream.next_out = dest;
    stream.avail_out = (uint32)*destLen;
    if ((uint32)stream.avail_out != *destLen) return Z_BUF_ERROR;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = NULL;

    err = deflateInit(&stream, level);
    if (err != Z_OK) return err;

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    *destLen = stream.total_out;

    err = deflateEnd(&stream);
    return err;
}

int
compress(uint8 *dest, uint32 *destLen, const uint8 *source, uint32 sourceLen)
{
    return compress2(dest, destLen, source, sourceLen, Z_DEFAULT_COMPRESSION);
}

uint32
compressBound(uint32 sourceLen)
{
    return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + 11;
}

} // namespace zlib
