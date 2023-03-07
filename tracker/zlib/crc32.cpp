/* crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2003 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
 * Adapted for C++ and inclusion in Tracker.NewFS 2005 Michael Lotz
 */

#include "zlib.h"
#include "crc32.h"
#include <limits.h>

namespace zlib {

#define REV(w)	(((w) >> 24) + (((w) >> 8) & 0xff00) + \
				(((w) & 0xff00) << 8) + (((w) & 0xff) << 24))

typedef unsigned int u4;

static uint32 crc32_little(uint32, const uint8 *, uint32);
static uint32 crc32_big(uint32, const uint8 *, uint32);

const uint32 *
get_crc_table()
{
  return (const uint32 *)crc_table;
}

#define DO1 crc = crc_table[0][((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8)
#define DO8 DO1; DO1; DO1; DO1; DO1; DO1; DO1; DO1

uint32
crc32(uint32 crc, const uint8 *buf, uint32 len)
{
    if (buf == NULL) return 0UL;

    if (sizeof(void *) == sizeof(ptrdiff_t)) {
        u4 endian;

        endian = 1;
        if (*((uchar *)(&endian)))
            return crc32_little(crc, buf, len);
        else
            return crc32_big(crc, buf, len);
    }

    crc = crc ^ 0xffffffffUL;
    while (len >= 8) {
        DO8;
        len -= 8;
    }
    if (len) do {
        DO1;
    } while (--len);
    return crc ^ 0xffffffffUL;
}

#define DOLIT4 c ^= *buf4++; \
        c = crc_table[3][c & 0xff] ^ crc_table[2][(c >> 8) & 0xff] ^ \
            crc_table[1][(c >> 16) & 0xff] ^ crc_table[0][c >> 24]
#define DOLIT32 DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4

static uint32
crc32_little(uint32 crc, const uint8 *buf, uint32 len)
{
    register u4 c;
    register const u4 *buf4;

    c = (u4)crc;
    c = ~c;
    while (len && ((ptrdiff_t)buf & 3)) {
        c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
        len--;
    }

    buf4 = (const u4 *)buf;
    while (len >= 32) {
        DOLIT32;
        len -= 32;
    }
    while (len >= 4) {
        DOLIT4;
        len -= 4;
    }
    buf = (const uchar *)buf4;

    if (len) do {
        c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
    } while (--len);
    c = ~c;
    return (uint32)c;
}

#define DOBIG4 c ^= *++buf4; \
        c = crc_table[4][c & 0xff] ^ crc_table[5][(c >> 8) & 0xff] ^ \
            crc_table[6][(c >> 16) & 0xff] ^ crc_table[7][c >> 24]
#define DOBIG32 DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4

static uint32
crc32_big(uint32 crc, const uint8 *buf, uint32 len)
{
    register u4 c;
    register const u4 *buf4;

    c = REV((u4)crc);
    c = ~c;
    while (len && ((ptrdiff_t)buf & 3)) {
        c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
        len--;
    }

    buf4 = (const u4 *)buf;
    buf4--;
    while (len >= 32) {
        DOBIG32;
        len -= 32;
    }
    while (len >= 4) {
        DOBIG4;
        len -= 4;
    }
    buf4++;
    buf = (const uchar *)buf4;

    if (len) do {
        c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
    } while (--len);
    c = ~c;
    return (uint32)(REV(c));
}

} // namespace zlib
