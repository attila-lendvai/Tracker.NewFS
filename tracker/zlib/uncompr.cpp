/* uncompr.c -- decompress a memory buffer
 * Copyright (C) 1995-2003 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
 * Adapted for C++ and inclusion in Tracker.NewFS 2005 Michael Lotz
 */

#define ZLIB_INTERNAL
#include "zlib.h"

namespace zlib {

int
uncompress(uint8 *dest, uint32 *destLen, const uint8 *source, uint32 sourceLen)
{
    z_stream stream;
    int err;

    stream.next_in = (uint8*)source;
    stream.avail_in = (uint32)sourceLen;
    /* Check for source > 64K on 16-bit machine: */
    if ((uint32)stream.avail_in != sourceLen) return Z_BUF_ERROR;

    stream.next_out = dest;
    stream.avail_out = (uint32)*destLen;
    if ((uint32)stream.avail_out != *destLen) return Z_BUF_ERROR;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;

    err = inflateInit(&stream);
    if (err != Z_OK) return err;

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        inflateEnd(&stream);
        if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0))
            return Z_DATA_ERROR;
        return err;
    }
    *destLen = stream.total_out;

    err = inflateEnd(&stream);
    return err;
}

} // namespace zlib
