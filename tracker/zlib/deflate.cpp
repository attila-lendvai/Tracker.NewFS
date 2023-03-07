/* deflate.c -- compress data using the deflation algorithm
 * Copyright (C) 1995-2003 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
 * Adapted for C++ and inclusion in Tracker.NewFS 2005 Michael Lotz
 */

#include <stdio.h>
#include "deflate.h"

namespace zlib {

const char deflate_copyright[] =
	" deflate 1.2.1 Copyright 1995-2003 Jean-loup Gailly ";

typedef enum {
    need_more,      /* block not completed, need more input or more output */
    block_done,     /* block flush performed */
    finish_started, /* finish started, need only more output at next deflate */
    finish_done     /* finish done, accept no more input or output */
} block_state;

typedef block_state (* compress_func)(deflate_state *s, int flush);

static void fill_window(deflate_state *s);
static block_state deflate_stored(deflate_state *s, int flush);
static block_state deflate_fast(deflate_state *s, int flush);
static block_state deflate_slow(deflate_state *s, int flush);
static void lm_init(deflate_state *s);
static void flush_pending(z_streamp strm);
static uint32 longest_match(deflate_state *s, IPos cur_match);
static uint32 longest_match_fast(deflate_state *s, IPos cur_match);

#define NIL 0
#ifndef TOO_FAR
#define TOO_FAR 4096
#endif

typedef struct config_s {
	uint16 good_length; /* reduce lazy search above this match length */
	uint16 max_lazy;    /* do not perform lazy search above this match length */
	uint16 nice_length; /* quit search above this match length */
	uint16 max_chain;
	compress_func func;
} config;

static const config configuration_table[10] = {
	{0,    0,  0,     0, deflate_stored},
	{4,    4,  8,     4, deflate_fast},
	{4,    5, 16,     8, deflate_fast},
	{4,    6, 32,    32, deflate_fast},

	{4,    4, 16,    16, deflate_slow},
	{8,   16, 32,    32, deflate_slow},
	{8,   16, 128,  128, deflate_slow},
	{8,   32, 128,  256, deflate_slow},
	{32, 128, 258, 1024, deflate_slow},
	{32, 258, 258, 4096, deflate_slow}
};

#define UPDATE_HASH(s,h,c) (h = (((h)<<s->hash_shift) ^ (c)) & s->hash_mask)
#define INSERT_STRING(s, str, match_head) \
   (UPDATE_HASH(s, s->ins_h, s->window[(str) + (MIN_MATCH-1)]), \
    match_head = s->prev[(str) & s->w_mask] = s->head[s->ins_h], \
    s->head[s->ins_h] = (Pos)(str))

#define CLEAR_HASH(s) \
    s->head[s->hash_size-1] = NIL; \
    memset((uint8 *)s->head, 0, (unsigned)(s->hash_size-1)*sizeof(*s->head));


int
deflateInit_(z_streamp strm, int level, const char *version, int stream_size)
{
    return deflateInit2_(strm, level, Z_DEFLATED, MAX_WBITS, DEF_MEM_LEVEL,
                         Z_DEFAULT_STRATEGY, version, stream_size);
}


int
deflateInit2_(z_streamp strm, int level, int method, int windowBits,
	int memLevel, int strategy, const char *version, int stream_size)
{
    deflate_state *s;
    int wrap = 1;
    static const char my_version[] = ZLIB_VERSION;

    uint16 *overlay;
    if (version == NULL || version[0] != my_version[0] ||
        stream_size != sizeof(z_stream)) {
        return Z_VERSION_ERROR;
    }
    if (strm == NULL) return Z_STREAM_ERROR;

    strm->msg = NULL;
    if (strm->zalloc == NULL) {
        strm->zalloc = (alloc_func)zcalloc;
        strm->opaque = NULL;
    }
    if (strm->zfree == (free_func)0) strm->zfree = zcfree;

    if (level == Z_DEFAULT_COMPRESSION) level = 6;

    if (windowBits < 0) { /* suppress zlib wrapper */
        wrap = 0;
        windowBits = -windowBits;
    }
#ifdef GZIP
    else if (windowBits > 15) {
        wrap = 2;       /* write gzip wrapper instead */
        windowBits -= 16;
    }
#endif
    if (memLevel < 1 || memLevel > MAX_MEM_LEVEL || method != Z_DEFLATED ||
        windowBits < 8 || windowBits > 15 || level < 0 || level > 9 ||
        strategy < 0 || strategy > Z_RLE) {
        return Z_STREAM_ERROR;
    }
    if (windowBits == 8) windowBits = 9;  /* until 256-byte window bug fixed */
    s = (deflate_state *) ZALLOC(strm, 1, sizeof(deflate_state));
    if (s == NULL) return Z_MEM_ERROR;
    strm->state = (struct internal_state *)s;
    s->strm = strm;

    s->wrap = wrap;
    s->w_bits = windowBits;
    s->w_size = 1 << s->w_bits;
    s->w_mask = s->w_size - 1;

    s->hash_bits = memLevel + 7;
    s->hash_size = 1 << s->hash_bits;
    s->hash_mask = s->hash_size - 1;
    s->hash_shift =  ((s->hash_bits+MIN_MATCH-1)/MIN_MATCH);

    s->window = (uint8 *)ZALLOC(strm, s->w_size, 2 * sizeof(uint8));
    s->prev   = (Pos *)ZALLOC(strm, s->w_size, sizeof(Pos));
    s->head   = (Pos *)ZALLOC(strm, s->hash_size, sizeof(Pos));

    s->lit_bufsize = 1 << (memLevel + 6); /* 16K elements by default */

    overlay = (uint16 *)ZALLOC(strm, s->lit_bufsize, sizeof(uint16) + 2);
    s->pending_buf = (uint8 *)overlay;
    s->pending_buf_size = (uint32)s->lit_bufsize * (sizeof(uint16) + 2L);

    if (s->window == NULL || s->prev == NULL || s->head == NULL ||
        s->pending_buf == NULL) {
        s->status = FINISH_STATE;
        strm->msg = (char*)ERR_MSG(Z_MEM_ERROR);
        deflateEnd (strm);
        return Z_MEM_ERROR;
    }
    s->d_buf = overlay + s->lit_bufsize / sizeof(uint16);
    s->l_buf = s->pending_buf + (1 + sizeof(uint16))*s->lit_bufsize;

    s->level = level;
    s->strategy = strategy;
    s->method = (uint8)method;

    return deflateReset(strm);
}


int
deflateSetDictionary(z_streamp strm, const uint8 *dictionary,
	uint32 dictLength)
{
    deflate_state *s;
    uint32 length = dictLength;
    uint32 n;
    IPos hash_head = 0;

    if (strm == NULL || strm->state == NULL || dictionary == NULL ||
        strm->state->wrap == 2 ||
        (strm->state->wrap == 1 && strm->state->status != INIT_STATE))
        return Z_STREAM_ERROR;

    s = strm->state;
    if (s->wrap)
        strm->adler = adler32(strm->adler, dictionary, dictLength);

    if (length < MIN_MATCH) return Z_OK;
    if (length > MAX_DIST(s)) {
        length = MAX_DIST(s);
#ifndef USE_DICT_HEAD
        dictionary += dictLength - length; /* use the tail of the dictionary */
#endif
    }
    memcpy(s->window, dictionary, length);
    s->strstart = length;
    s->block_start = (long)length;

    s->ins_h = s->window[0];
    UPDATE_HASH(s, s->ins_h, s->window[1]);
    for (n = 0; n <= length - MIN_MATCH; n++) {
        INSERT_STRING(s, n, hash_head);
    }
    if (hash_head) hash_head = 0;  /* to make compiler happy */
    return Z_OK;
}


int
deflateReset(z_streamp strm)
{
    deflate_state *s;

    if (strm == NULL || strm->state == NULL ||
        strm->zalloc == (alloc_func)0 || strm->zfree == (free_func)0) {
        return Z_STREAM_ERROR;
    }

    strm->total_in = strm->total_out = 0;
    strm->msg = NULL; /* use zfree if we ever allocate msg dynamically */
    strm->data_type = Z_UNKNOWN;

    s = (deflate_state *)strm->state;
    s->pending = 0;
    s->pending_out = s->pending_buf;

    if (s->wrap < 0) {
        s->wrap = -s->wrap; /* was made negative by deflate(..., Z_FINISH); */
    }
    s->status = s->wrap ? INIT_STATE : BUSY_STATE;
    strm->adler =
#ifdef GZIP
        s->wrap == 2 ? crc32(0L, NULL, 0) :
#endif
        adler32(0L, NULL, 0);
    s->last_flush = Z_NO_FLUSH;

    _tr_init(s);
    lm_init(s);

    return Z_OK;
}


int
deflatePrime(z_streamp strm, int bits, int value)
{
    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    strm->state->bi_valid = bits;
    strm->state->bi_buf = (uint16)(value & ((1 << bits) - 1));
    return Z_OK;
}


int
deflateParams(z_streamp strm, int level, int strategy)
{
    deflate_state *s;
    compress_func func;
    int err = Z_OK;

    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    s = strm->state;

    if (level == Z_DEFAULT_COMPRESSION) level = 6;
    if (level < 0 || level > 9 || strategy < 0 || strategy > Z_RLE) {
        return Z_STREAM_ERROR;
    }
    func = configuration_table[s->level].func;

    if (func != configuration_table[level].func && strm->total_in != 0) {
        /* Flush the last buffer: */
        err = deflate(strm, Z_PARTIAL_FLUSH);
    }
    if (s->level != level) {
        s->level = level;
        s->max_lazy_match   = configuration_table[level].max_lazy;
        s->good_match       = configuration_table[level].good_length;
        s->nice_match       = configuration_table[level].nice_length;
        s->max_chain_length = configuration_table[level].max_chain;
    }
    s->strategy = strategy;
    return err;
}


uint32
deflateBound(z_streamp strm, uint32 sourceLen)
{
    deflate_state *s;
    uint32 destLen;

    /* conservative upper bound */
    destLen = sourceLen +
              ((sourceLen + 7) >> 3) + ((sourceLen + 63) >> 6) + 11;

    /* if can't get parameters, return conservative bound */
    if (strm == NULL || strm->state == NULL)
        return destLen;

    /* if not default parameters, return conservative bound */
    s = strm->state;
    if (s->w_bits != 15 || s->hash_bits != 8 + 7)
        return destLen;

    /* default settings: return tight bound for that case */
    return compressBound(sourceLen);
}


static void
putShortMSB(deflate_state *s, uint32 b)
{
    put_byte(s, (uint8)(b >> 8));
    put_byte(s, (uint8)(b & 0xff));
}


static void
flush_pending(z_streamp strm)
{
    unsigned len = strm->state->pending;

    if (len > strm->avail_out) len = strm->avail_out;
    if (len == 0) return;

    memcpy(strm->next_out, strm->state->pending_out, len);
    strm->next_out  += len;
    strm->state->pending_out  += len;
    strm->total_out += len;
    strm->avail_out  -= len;
    strm->state->pending -= len;
    if (strm->state->pending == 0) {
        strm->state->pending_out = strm->state->pending_buf;
    }
}


int
deflate(z_streamp strm, int flush)
{
    int old_flush; /* value of flush param for previous deflate call */
    deflate_state *s;

    if (strm == NULL || strm->state == NULL ||
        flush > Z_FINISH || flush < 0) {
        return Z_STREAM_ERROR;
    }
    s = strm->state;

    if (strm->next_out == NULL ||
        (strm->next_in == NULL && strm->avail_in != 0) ||
        (s->status == FINISH_STATE && flush != Z_FINISH)) {
        ERR_RETURN(strm, Z_STREAM_ERROR);
    }
    if (strm->avail_out == 0) ERR_RETURN(strm, Z_BUF_ERROR);

    s->strm = strm; /* just in case */
    old_flush = s->last_flush;
    s->last_flush = flush;

    /* Write the header */
    if (s->status == INIT_STATE) {
#ifdef GZIP
        if (s->wrap == 2) {
            put_byte(s, 31);
            put_byte(s, 139);
            put_byte(s, 8);
            put_byte(s, 0);
            put_byte(s, 0);
            put_byte(s, 0);
            put_byte(s, 0);
            put_byte(s, 0);
            put_byte(s, s->level == 9 ? 2 :
                        (s->strategy >= Z_HUFFMAN_ONLY || s->level < 2 ?
                         4 : 0));
            put_byte(s, 255);
            s->status = BUSY_STATE;
            strm->adler = crc32(0L, NULL, 0);
        }
        else
#endif
        {
            uint32 header = (Z_DEFLATED + ((s->w_bits-8)<<4)) << 8;
            uint32 level_flags;

            if (s->strategy >= Z_HUFFMAN_ONLY || s->level < 2)
                level_flags = 0;
            else if (s->level < 6)
                level_flags = 1;
            else if (s->level == 6)
                level_flags = 2;
            else
                level_flags = 3;
            header |= (level_flags << 6);
            if (s->strstart != 0) header |= PRESET_DICT;
            header += 31 - (header % 31);

            s->status = BUSY_STATE;
            putShortMSB(s, header);

            /* Save the adler32 of the preset dictionary: */
            if (s->strstart != 0) {
                putShortMSB(s, (uint32)(strm->adler >> 16));
                putShortMSB(s, (uint32)(strm->adler & 0xffff));
            }
            strm->adler = adler32(0L, NULL, 0);
        }
    }

    /* Flush as much pending output as possible */
    if (s->pending != 0) {
        flush_pending(strm);
        if (strm->avail_out == 0) {
            /* Since avail_out is 0, deflate will be called again with
             * more output space, but possibly with both pending and
             * avail_in equal to zero. There won't be anything to do,
             * but this is not an error situation so make sure we
             * return OK instead of BUF_ERROR at next call of deflate:
             */
            s->last_flush = -1;
            return Z_OK;
        }

    /* Make sure there is something to do and avoid duplicate consecutive
     * flushes. For repeated and useless calls with Z_FINISH, we keep
     * returning Z_STREAM_END instead of Z_BUF_ERROR.
     */
    } else if (strm->avail_in == 0 && flush <= old_flush &&
               flush != Z_FINISH) {
        ERR_RETURN(strm, Z_BUF_ERROR);
    }

    /* User must not provide more input after the first FINISH: */
    if (s->status == FINISH_STATE && strm->avail_in != 0) {
        ERR_RETURN(strm, Z_BUF_ERROR);
    }

    /* Start a new block or continue the current one.
     */
    if (strm->avail_in != 0 || s->lookahead != 0 ||
        (flush != Z_NO_FLUSH && s->status != FINISH_STATE)) {
        block_state bstate;

        bstate = (*(configuration_table[s->level].func))(s, flush);

        if (bstate == finish_started || bstate == finish_done) {
            s->status = FINISH_STATE;
        }
        if (bstate == need_more || bstate == finish_started) {
            if (strm->avail_out == 0) {
                s->last_flush = -1; /* avoid BUF_ERROR next call, see above */
            }
            return Z_OK;
            /* If flush != Z_NO_FLUSH && avail_out == 0, the next call
             * of deflate should use the same flush parameter to make sure
             * that the flush is complete. So we don't have to output an
             * empty block here, this will be done at next call. This also
             * ensures that for a very small output buffer, we emit at most
             * one empty block.
             */
        }
        if (bstate == block_done) {
            if (flush == Z_PARTIAL_FLUSH) {
                _tr_align(s);
            } else { /* FULL_FLUSH or SYNC_FLUSH */
                _tr_stored_block(s, NULL, 0, 0);
                /* For a full flush, this empty block will be recognized
                 * as a special marker by inflate_sync().
                 */
                if (flush == Z_FULL_FLUSH) {
                    CLEAR_HASH(s);             /* forget history */
                }
            }
            flush_pending(strm);
            if (strm->avail_out == 0) {
              s->last_flush = -1; /* avoid BUF_ERROR at next call, see above */
              return Z_OK;
            }
        }
    }

    if (flush != Z_FINISH) return Z_OK;
    if (s->wrap <= 0) return Z_STREAM_END;

    /* Write the trailer */
#ifdef GZIP
    if (s->wrap == 2) {
        put_byte(s, (uint8)(strm->adler & 0xff));
        put_byte(s, (uint8)((strm->adler >> 8) & 0xff));
        put_byte(s, (uint8)((strm->adler >> 16) & 0xff));
        put_byte(s, (uint8)((strm->adler >> 24) & 0xff));
        put_byte(s, (uint8)(strm->total_in & 0xff));
        put_byte(s, (uint8)((strm->total_in >> 8) & 0xff));
        put_byte(s, (uint8)((strm->total_in >> 16) & 0xff));
        put_byte(s, (uint8)((strm->total_in >> 24) & 0xff));
    }
    else
#endif
    {
        putShortMSB(s, (uint32)(strm->adler >> 16));
        putShortMSB(s, (uint32)(strm->adler & 0xffff));
    }
    flush_pending(strm);
    /* If avail_out is zero, the application will call deflate again
     * to flush the rest.
     */
    if (s->wrap > 0) s->wrap = -s->wrap; /* write the trailer only once! */
    return s->pending != 0 ? Z_OK : Z_STREAM_END;
}


int
deflateEnd(z_streamp strm)
{
    int status;

    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;

    status = strm->state->status;
    if (status != INIT_STATE && status != BUSY_STATE &&
        status != FINISH_STATE) {
      return Z_STREAM_ERROR;
    }

    /* Deallocate in reverse order of allocations: */
    TRY_FREE(strm, strm->state->pending_buf);
    TRY_FREE(strm, strm->state->head);
    TRY_FREE(strm, strm->state->prev);
    TRY_FREE(strm, strm->state->window);

    ZFREE(strm, strm->state);
    strm->state = NULL;

    return status == BUSY_STATE ? Z_DATA_ERROR : Z_OK;
}


int
deflateCopy(z_streamp dest, z_streamp source)
{
    deflate_state *ds;
    deflate_state *ss;
    uint16 *overlay;

    if (source == NULL || dest == NULL || source->state == NULL) {
        return Z_STREAM_ERROR;
    }

    ss = source->state;

    *dest = *source;

    ds = (deflate_state *) ZALLOC(dest, 1, sizeof(deflate_state));
    if (ds == NULL) return Z_MEM_ERROR;
    dest->state = (struct internal_state *) ds;
    *ds = *ss;
    ds->strm = dest;

    ds->window = (uint8 *) ZALLOC(dest, ds->w_size, 2*sizeof(uint8));
    ds->prev   = (Pos *)  ZALLOC(dest, ds->w_size, sizeof(Pos));
    ds->head   = (Pos *)  ZALLOC(dest, ds->hash_size, sizeof(Pos));
    overlay = (uint16 *) ZALLOC(dest, ds->lit_bufsize, sizeof(uint16)+2);
    ds->pending_buf = (uint8 *) overlay;

    if (ds->window == NULL || ds->prev == NULL || ds->head == NULL ||
        ds->pending_buf == NULL) {
        deflateEnd(dest);
        return Z_MEM_ERROR;
    }
    
    memcpy(ds->window, ss->window, ds->w_size * 2 * sizeof(uint8));
    memcpy(ds->prev, ss->prev, ds->w_size * sizeof(Pos));
    memcpy(ds->head, ss->head, ds->hash_size * sizeof(Pos));
    memcpy(ds->pending_buf, ss->pending_buf, (uint32)ds->pending_buf_size);

    ds->pending_out = ds->pending_buf + (ss->pending_out - ss->pending_buf);
    ds->d_buf = overlay + ds->lit_bufsize/sizeof(uint16);
    ds->l_buf = ds->pending_buf + (1+sizeof(uint16))*ds->lit_bufsize;

    ds->l_desc.dyn_tree = ds->dyn_ltree;
    ds->d_desc.dyn_tree = ds->dyn_dtree;
    ds->bl_desc.dyn_tree = ds->bl_tree;

    return Z_OK;
}


static int
read_buf(z_streamp strm, uint8 *buf, uint32 size)
{
    unsigned len = strm->avail_in;

    if (len > size) len = size;
    if (len == 0) return 0;

    strm->avail_in  -= len;

    if (strm->state->wrap == 1) {
        strm->adler = adler32(strm->adler, strm->next_in, len);
    }
#ifdef GZIP
    else if (strm->state->wrap == 2) {
        strm->adler = crc32(strm->adler, strm->next_in, len);
    }
#endif
    memcpy(buf, strm->next_in, len);
    strm->next_in  += len;
    strm->total_in += len;

    return (int)len;
}


static void
lm_init(deflate_state *s)
{
    s->window_size = (uint32)2L*s->w_size;

    CLEAR_HASH(s);

    /* Set the default configuration parameters:
     */
    s->max_lazy_match   = configuration_table[s->level].max_lazy;
    s->good_match       = configuration_table[s->level].good_length;
    s->nice_match       = configuration_table[s->level].nice_length;
    s->max_chain_length = configuration_table[s->level].max_chain;

    s->strstart = 0;
    s->block_start = 0L;
    s->lookahead = 0;
    s->match_length = s->prev_length = MIN_MATCH-1;
    s->match_available = 0;
    s->ins_h = 0;
}

static uint32
longest_match(deflate_state *s, IPos cur_match)
{
    unsigned chain_length = s->max_chain_length;/* max hash chain length */
    register uint8 *scan = s->window + s->strstart; /* current string */
    register uint8 *match;                       /* matched string */
    register int len;                           /* length of current match */
    int best_len = s->prev_length;              /* best match length so far */
    int nice_match = s->nice_match;             /* stop if match long enough */
    IPos limit = s->strstart > (IPos)MAX_DIST(s) ?
        s->strstart - (IPos)MAX_DIST(s) : NIL;
    /* Stop when cur_match becomes <= limit. To simplify the code,
     * we prevent matches with the string of window index 0.
     */
    Pos *prev = s->prev;
    uint32 wmask = s->w_mask;

#ifdef UNALIGNED_OK
    /* Compare two bytes at a time. Note: this is not always beneficial.
     * Try with and without -DUNALIGNED_OK to check.
     */
    register uint8 *strend = s->window + s->strstart + MAX_MATCH - 1;
    register uint16 scan_start = *(uint16*)scan;
    register uint16 scan_end   = *(uint16*)(scan+best_len-1);
#else
    register uint8 *strend = s->window + s->strstart + MAX_MATCH;
    register uint8 scan_end1  = scan[best_len-1];
    register uint8 scan_end   = scan[best_len];
#endif

    /* The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple of 16.
     * It is easy to get rid of this optimization if necessary.
     */
    ZASSERT(s->hash_bits >= 8 && MAX_MATCH == 258, "Code too clever");

    /* Do not waste too much time if we already have a good match: */
    if (s->prev_length >= s->good_match) {
        chain_length >>= 2;
    }
    /* Do not look for matches beyond the end of the input. This is necessary
     * to make deflate deterministic.
     */
    if ((uint32)nice_match > s->lookahead) nice_match = s->lookahead;

    ZASSERT((uint32)s->strstart <= s->window_size-MIN_LOOKAHEAD, "need lookahead");

    do {
        ZASSERT(cur_match < s->strstart, "no future");
        match = s->window + cur_match;

        /* Skip to next match if the match length cannot increase
         * or if the match length is less than 2:
         */
#if (defined(UNALIGNED_OK) && MAX_MATCH == 258)
        /* This code assumes sizeof(unsigned short) == 2. Do not use
         * UNALIGNED_OK if your compiler uses a different size.
         */
        if (*(uint16*)(match+best_len-1) != scan_end ||
            *(uint16*)match != scan_start) continue;

        /* It is not necessary to compare scan[2] and match[2] since they are
         * always equal when the other bytes match, given that the hash keys
         * are equal and that HASH_BITS >= 8. Compare 2 bytes at a time at
         * strstart+3, +5, ... up to strstart+257. We check for insufficient
         * lookahead only every 4th comparison; the 128th check will be made
         * at strstart+257. If MAX_MATCH-2 is not a multiple of 8, it is
         * necessary to put more guard bytes at the end of the window, or
         * to check more often for insufficient lookahead.
         */
        ZASSERT(scan[2] == match[2], "scan[2]?");
        scan++, match++;
        do {
        } while (*(uint16*)(scan+=2) == *(uint16*)(match+=2) &&
                 *(uint16*)(scan+=2) == *(uint16*)(match+=2) &&
                 *(uint16*)(scan+=2) == *(uint16*)(match+=2) &&
                 *(uint16*)(scan+=2) == *(uint16*)(match+=2) &&
                 scan < strend);
        /* The funny "do {}" generates better code on most compilers */

        /* Here, scan <= window+strstart+257 */
        ZASSERT(scan <= s->window+(unsigned)(s->window_size-1), "wild scan");
        if (*scan == *match) scan++;

        len = (MAX_MATCH - 1) - (int)(strend-scan);
        scan = strend - (MAX_MATCH-1);

#else /* UNALIGNED_OK */

        if (match[best_len]   != scan_end  ||
            match[best_len-1] != scan_end1 ||
            *match            != *scan     ||
            *++match          != scan[1])      continue;

        /* The check at best_len-1 can be removed because it will be made
         * again later. (This heuristic is not always a win.)
         * It is not necessary to compare scan[2] and match[2] since they
         * are always equal when the other bytes match, given that
         * the hash keys are equal and that HASH_BITS >= 8.
         */
        scan += 2, match++;
        ZASSERT(*scan == *match, "match[2]?");

        /* We check for insufficient lookahead only every 8th comparison;
         * the 256th check will be made at strstart+258.
         */
        do {
        } while (*++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 scan < strend);

        ZASSERT(scan <= s->window+(unsigned)(s->window_size-1), "wild scan");

        len = MAX_MATCH - (int)(strend - scan);
        scan = strend - MAX_MATCH;

#endif /* UNALIGNED_OK */

        if (len > best_len) {
            s->match_start = cur_match;
            best_len = len;
            if (len >= nice_match) break;
#ifdef UNALIGNED_OK
            scan_end = *(uint16*)(scan+best_len-1);
#else
            scan_end1  = scan[best_len-1];
            scan_end   = scan[best_len];
#endif
        }
    } while ((cur_match = prev[cur_match & wmask]) > limit
             && --chain_length != 0);

    if ((uint32)best_len <= s->lookahead) return (uint32)best_len;
    return s->lookahead;
}


static uint32
longest_match_fast(deflate_state *s, IPos cur_match)
{
    register uint8 *scan = s->window + s->strstart; /* current string */
    register uint8 *match;                       /* matched string */
    register int len;                           /* length of current match */
    register uint8 *strend = s->window + s->strstart + MAX_MATCH;

    /* The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple of 16.
     * It is easy to get rid of this optimization if necessary.
     */
    ZASSERT(s->hash_bits >= 8 && MAX_MATCH == 258, "Code too clever");

    ZASSERT((uint32)s->strstart <= s->window_size-MIN_LOOKAHEAD, "need lookahead");

    ZASSERT(cur_match < s->strstart, "no future");

    match = s->window + cur_match;

    /* Return failure if the match length is less than 2:
     */
    if (match[0] != scan[0] || match[1] != scan[1]) return MIN_MATCH-1;

    /* The check at best_len-1 can be removed because it will be made
     * again later. (This heuristic is not always a win.)
     * It is not necessary to compare scan[2] and match[2] since they
     * are always equal when the other bytes match, given that
     * the hash keys are equal and that HASH_BITS >= 8.
     */
    scan += 2, match += 2;
    ZASSERT(*scan == *match, "match[2]?");

    /* We check for insufficient lookahead only every 8th comparison;
     * the 256th check will be made at strstart+258.
     */
    do {
    } while (*++scan == *++match && *++scan == *++match &&
             *++scan == *++match && *++scan == *++match &&
             *++scan == *++match && *++scan == *++match &&
             *++scan == *++match && *++scan == *++match &&
             scan < strend);

    ZASSERT(scan <= s->window+(unsigned)(s->window_size-1), "wild scan");

    len = MAX_MATCH - (int)(strend - scan);

    if (len < MIN_MATCH) return MIN_MATCH - 1;

    s->match_start = cur_match;
    return (uint32)len <= s->lookahead ? (uint32)len : s->lookahead;
}


static void
fill_window(deflate_state *s)
{
    register unsigned n, m;
    register Pos *p;
    unsigned more;    /* Amount of free space at the end of the window. */
    uint32 wsize = s->w_size;

    do {
        more = (unsigned)(s->window_size -(uint32)s->lookahead -(uint32)s->strstart);

        /* Deal with !@#$% 64K limit: */
        if (sizeof(int) <= 2) {
            if (more == 0 && s->strstart == 0 && s->lookahead == 0) {
                more = wsize;

            } else if (more == (unsigned)(-1)) {
                /* Very unlikely, but possible on 16 bit machine if
                 * strstart == 0 && lookahead == 1 (input done a byte at time)
                 */
                more--;
            }
        }

        /* If the window is almost full and there is insufficient lookahead,
         * move the upper half to the lower one to make room in the upper half.
         */
        if (s->strstart >= wsize+MAX_DIST(s)) {

            memcpy(s->window, s->window+wsize, (unsigned)wsize);
            s->match_start -= wsize;
            s->strstart    -= wsize; /* we now have strstart >= MAX_DIST */
            s->block_start -= (long) wsize;

            /* Slide the hash table (could be avoided with 32 bit values
               at the expense of memory usage). We slide even when level == 0
               to keep the hash table consistent if we switch back to level > 0
               later. (Using level 0 permanently is not an optimal usage of
               zlib, so we don't care about this pathological case.)
             */
            n = s->hash_size;
            p = &s->head[n];
            do {
                m = *--p;
                *p = (Pos)(m >= wsize ? m-wsize : NIL);
            } while (--n);

            n = wsize;
            p = &s->prev[n];
            do {
                m = *--p;
                *p = (Pos)(m >= wsize ? m-wsize : NIL);
                /* If n is not on any hash chain, prev[n] is garbage but
                 * its value will never be used.
                 */
            } while (--n);
            more += wsize;
        }
        if (s->strm->avail_in == 0) return;

        /* If there was no sliding:
         *    strstart <= WSIZE+MAX_DIST-1 && lookahead <= MIN_LOOKAHEAD - 1 &&
         *    more == window_size - lookahead - strstart
         * => more >= window_size - (MIN_LOOKAHEAD-1 + WSIZE + MAX_DIST-1)
         * => more >= window_size - 2*WSIZE + 2
         * In the BIG_MEM or MMAP case (not yet supported),
         *   window_size == input_size + MIN_LOOKAHEAD  &&
         *   strstart + s->lookahead <= input_size => more >= MIN_LOOKAHEAD.
         * Otherwise, window_size == 2*WSIZE so more >= 2.
         * If there was sliding, more >= WSIZE. So in all cases, more >= 2.
         */
        ZASSERT(more >= 2, "more < 2");

        n = read_buf(s->strm, s->window + s->strstart + s->lookahead, more);
        s->lookahead += n;

        /* Initialize the hash value now that we have some input: */
        if (s->lookahead >= MIN_MATCH) {
            s->ins_h = s->window[s->strstart];
            UPDATE_HASH(s, s->ins_h, s->window[s->strstart+1]);
#if MIN_MATCH != 3
            Call UPDATE_HASH() MIN_MATCH-3 more times
#endif
        }
        /* If the whole input has less than MIN_MATCH bytes, ins_h is garbage,
         * but this is not important since only literal bytes will be emitted.
         */

    } while (s->lookahead < MIN_LOOKAHEAD && s->strm->avail_in != 0);
}


#define FLUSH_BLOCK_ONLY(s, eof) { \
   _tr_flush_block(s, (s->block_start >= 0L ? \
                   (uint8 *)&s->window[(unsigned)s->block_start] : \
                   (uint8 *)NULL), \
                (uint32)((long)s->strstart - s->block_start), \
                (eof)); \
   s->block_start = s->strstart; \
   flush_pending(s->strm); \
   TRACEV((stderr, "[FLUSH]")); \
}


#define FLUSH_BLOCK(s, eof) { \
   FLUSH_BLOCK_ONLY(s, eof); \
   if (s->strm->avail_out == 0) return (eof) ? finish_started : need_more; \
}


static block_state
deflate_stored(deflate_state *s, int flush)
{
    /* Stored blocks are limited to 0xffff bytes, pending_buf is limited
     * to pending_buf_size, and each stored block has a 5 byte header:
     */
    uint32 max_block_size = 0xffff;
    uint32 max_start;

    if (max_block_size > s->pending_buf_size - 5) {
        max_block_size = s->pending_buf_size - 5;
    }

    /* Copy as much as possible from input to output: */
    for (;;) {
        /* Fill the window as much as possible: */
        if (s->lookahead <= 1) {

            ZASSERT(s->strstart < s->w_size+MAX_DIST(s) ||
                   s->block_start >= (long)s->w_size, "slide too late");

            fill_window(s);
            if (s->lookahead == 0 && flush == Z_NO_FLUSH) return need_more;

            if (s->lookahead == 0) break; /* flush the current block */
        }
        ZASSERT(s->block_start >= 0L, "block gone");

        s->strstart += s->lookahead;
        s->lookahead = 0;

        /* Emit a stored block if pending_buf will be full: */
        max_start = s->block_start + max_block_size;
        if (s->strstart == 0 || (uint32)s->strstart >= max_start) {
            /* strstart == 0 is possible when wraparound on 16-bit machine */
            s->lookahead = (uint32)(s->strstart - max_start);
            s->strstart = (uint32)max_start;
            FLUSH_BLOCK(s, 0);
        }
        /* Flush if we may have to slide, otherwise block_start may become
         * negative and the data will be gone:
         */
        if (s->strstart - (uint32)s->block_start >= MAX_DIST(s)) {
            FLUSH_BLOCK(s, 0);
        }
    }
    FLUSH_BLOCK(s, flush == Z_FINISH);
    return flush == Z_FINISH ? finish_done : block_done;
}


static block_state
deflate_fast(deflate_state *s, int flush)
{
    IPos hash_head = NIL; /* head of the hash chain */
    int bflush;           /* set if current block must be flushed */

    for (;;) {
        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need MAX_MATCH bytes
         * for the next match, plus MIN_MATCH bytes to insert the
         * string following the next match.
         */
        if (s->lookahead < MIN_LOOKAHEAD) {
            fill_window(s);
            if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH) {
                return need_more;
            }
            if (s->lookahead == 0) break; /* flush the current block */
        }

        /* Insert the string window[strstart .. strstart+2] in the
         * dictionary, and set hash_head to the head of the hash chain:
         */
        if (s->lookahead >= MIN_MATCH) {
            INSERT_STRING(s, s->strstart, hash_head);
        }

        /* Find the longest match, discarding those <= prev_length.
         * At this point we have always match_length < MIN_MATCH
         */
        if (hash_head != NIL && s->strstart - hash_head <= MAX_DIST(s)) {
            /* To simplify the code, we prevent matches with the string
             * of window index 0 (in particular we have to avoid a match
             * of the string with itself at the start of the input file).
             */
            if (s->strategy < Z_HUFFMAN_ONLY) {
                s->match_length = longest_match (s, hash_head);
            } else if (s->strategy == Z_RLE && s->strstart - hash_head == 1) {
                s->match_length = longest_match_fast (s, hash_head);
            }
            /* longest_match() or longest_match_fast() sets match_start */
        }
        if (s->match_length >= MIN_MATCH) {
            _tr_tally_dist(s, s->strstart - s->match_start,
                           s->match_length - MIN_MATCH, bflush);

            s->lookahead -= s->match_length;

            /* Insert new strings in the hash table only if the match length
             * is not too large. This saves time but degrades compression.
             */
            if (s->match_length <= s->max_insert_length &&
                s->lookahead >= MIN_MATCH) {
                s->match_length--; /* string at strstart already in table */
                do {
                    s->strstart++;
                    INSERT_STRING(s, s->strstart, hash_head);
                    /* strstart never exceeds WSIZE-MAX_MATCH, so there are
                     * always MIN_MATCH bytes ahead.
                     */
                } while (--s->match_length != 0);
                s->strstart++;
            } else {
                s->strstart += s->match_length;
                s->match_length = 0;
                s->ins_h = s->window[s->strstart];
                UPDATE_HASH(s, s->ins_h, s->window[s->strstart+1]);
#if MIN_MATCH != 3
                Call UPDATE_HASH() MIN_MATCH-3 more times
#endif
                /* If lookahead < MIN_MATCH, ins_h is garbage, but it does not
                 * matter since it will be recomputed at next deflate call.
                 */
            }
        } else {
            /* No match, output a literal byte */
            TRACEVV((stderr,"%c", s->window[s->strstart]));
            _tr_tally_lit (s, s->window[s->strstart], bflush);
            s->lookahead--;
            s->strstart++;
        }
        if (bflush) FLUSH_BLOCK(s, 0);
    }
    FLUSH_BLOCK(s, flush == Z_FINISH);
    return flush == Z_FINISH ? finish_done : block_done;
}

static block_state
deflate_slow(deflate_state *s, int flush)
{
    IPos hash_head = NIL;    /* head of hash chain */
    int bflush;              /* set if current block must be flushed */

    /* Process the input block. */
    for (;;) {
        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need MAX_MATCH bytes
         * for the next match, plus MIN_MATCH bytes to insert the
         * string following the next match.
         */
        if (s->lookahead < MIN_LOOKAHEAD) {
            fill_window(s);
            if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH) {
                return need_more;
            }
            if (s->lookahead == 0) break; /* flush the current block */
        }

        /* Insert the string window[strstart .. strstart+2] in the
         * dictionary, and set hash_head to the head of the hash chain:
         */
        if (s->lookahead >= MIN_MATCH) {
            INSERT_STRING(s, s->strstart, hash_head);
        }

        /* Find the longest match, discarding those <= prev_length.
         */
        s->prev_length = s->match_length, s->prev_match = s->match_start;
        s->match_length = MIN_MATCH-1;

        if (hash_head != NIL && s->prev_length < s->max_lazy_match &&
            s->strstart - hash_head <= MAX_DIST(s)) {
            /* To simplify the code, we prevent matches with the string
             * of window index 0 (in particular we have to avoid a match
             * of the string with itself at the start of the input file).
             */
            if (s->strategy < Z_HUFFMAN_ONLY) {
                s->match_length = longest_match (s, hash_head);
            } else if (s->strategy == Z_RLE && s->strstart - hash_head == 1) {
                s->match_length = longest_match_fast (s, hash_head);
            }
            /* longest_match() or longest_match_fast() sets match_start */

            if (s->match_length <= 5 && (s->strategy == Z_FILTERED
#if TOO_FAR <= 32767
                || (s->match_length == MIN_MATCH &&
                    s->strstart - s->match_start > TOO_FAR)
#endif
                )) {

                /* If prev_match is also MIN_MATCH, match_start is garbage
                 * but we will ignore the current match anyway.
                 */
                s->match_length = MIN_MATCH-1;
            }
        }
        /* If there was a match at the previous step and the current
         * match is not better, output the previous match:
         */
        if (s->prev_length >= MIN_MATCH && s->match_length <= s->prev_length) {
            uint32 max_insert = s->strstart + s->lookahead - MIN_MATCH;
            /* Do not insert strings in hash table beyond this. */

            _tr_tally_dist(s, s->strstart -1 - s->prev_match,
                           s->prev_length - MIN_MATCH, bflush);

            /* Insert in hash table all strings up to the end of the match.
             * strstart-1 and strstart are already inserted. If there is not
             * enough lookahead, the last two strings are not inserted in
             * the hash table.
             */
            s->lookahead -= s->prev_length-1;
            s->prev_length -= 2;
            do {
                if (++s->strstart <= max_insert) {
                    INSERT_STRING(s, s->strstart, hash_head);
                }
            } while (--s->prev_length != 0);
            s->match_available = 0;
            s->match_length = MIN_MATCH-1;
            s->strstart++;

            if (bflush) FLUSH_BLOCK(s, 0);

        } else if (s->match_available) {
            /* If there was no match at the previous position, output a
             * single literal. If there was a match but the current match
             * is longer, truncate the previous match to a single literal.
             */
            TRACEVV((stderr,"%c", s->window[s->strstart-1]));
            _tr_tally_lit(s, s->window[s->strstart-1], bflush);
            if (bflush) {
                FLUSH_BLOCK_ONLY(s, 0);
            }
            s->strstart++;
            s->lookahead--;
            if (s->strm->avail_out == 0) return need_more;
        } else {
            /* There is no previous match to compare with, wait for
             * the next step to decide.
             */
            s->match_available = 1;
            s->strstart++;
            s->lookahead--;
        }
    }
    ZASSERT(flush != Z_NO_FLUSH, "no flush?");
    if (s->match_available) {
        TRACEVV((stderr,"%c", s->window[s->strstart-1]));
        _tr_tally_lit(s, s->window[s->strstart-1], bflush);
        s->match_available = 0;
    }
    FLUSH_BLOCK(s, flush == Z_FINISH);
    return flush == Z_FINISH ? finish_done : block_done;
}

} // namespace zlib
