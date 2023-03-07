/* inflate.c -- zlib decompression
 * Copyright (C) 1995-2003 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
 * Adapted for C++ and inclusion in Tracker.NewFS 2005 Michael Lotz
 */

#include "zutil.h"
#include "inftrees.h"
#include "inflate.h"
#include "inffast.h"

namespace zlib {

static void fixedtables(struct inflate_state *state);
static int updatewindow(z_streamp strm, unsigned out);
static unsigned syncsearch(uint32 *have, unsigned char *buf,
	unsigned len);

int
inflateReset(z_streamp strm)
{
    struct inflate_state *state;

    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    state = (struct inflate_state *)strm->state;
    strm->total_in = strm->total_out = state->total = 0;
    strm->msg = NULL;
    state->mode = HEAD;
    state->last = 0;
    state->havedict = 0;
    state->wsize = 0;
    state->whave = 0;
    state->hold = 0;
    state->bits = 0;
    state->lencode = state->distcode = state->next = state->codes;
    TRACEV((stderr, "inflate: reset\n"));
    return Z_OK;
}


int
inflateInit2_(z_streamp strm, int windowBits, const char *version,
	int stream_size)
{
    struct inflate_state *state;

    if (version == NULL || version[0] != ZLIB_VERSION[0] ||
        stream_size != (int)(sizeof(z_stream)))
        return Z_VERSION_ERROR;
    if (strm == NULL) return Z_STREAM_ERROR;
    strm->msg = NULL;                 /* in case we return an error */
    if (strm->zalloc == NULL) {
        strm->zalloc = (alloc_func)zcalloc;
        strm->opaque = NULL;
    }
    if (strm->zfree == (free_func)0) strm->zfree = zcfree;
    state = (struct inflate_state *)
            ZALLOC(strm, 1, sizeof(struct inflate_state));
    if (state == NULL) return Z_MEM_ERROR;
    TRACEV((stderr, "inflate: allocated\n"));
    strm->state = (internal_state *)state;
    if (windowBits < 0) {
        state->wrap = 0;
        windowBits = -windowBits;
    }
    else {
        state->wrap = (windowBits >> 4) + 1;
        if (windowBits < 48) windowBits &= 15;
    }
    if (windowBits < 8 || windowBits > 15) {
        ZFREE(strm, state);
        strm->state = NULL;
        return Z_STREAM_ERROR;
    }
    state->wbits = (unsigned)windowBits;
    state->window = NULL;
    return inflateReset(strm);
}


int
inflateInit_(z_streamp strm, const char *version, int stream_size)
{
    return inflateInit2_(strm, DEF_WBITS, version, stream_size);
}


static void
fixedtables(struct inflate_state *state)
{
	#include "inffixed.h"
    state->lencode = lenfix;
    state->lenbits = 9;
    state->distcode = distfix;
    state->distbits = 5;
}


static int
updatewindow(z_streamp strm, unsigned out)
{
    struct inflate_state *state;
    unsigned copy, dist;

    state = (struct inflate_state *)strm->state;

    /* if it hasn't been done already, allocate space for the window */
    if (state->window == NULL) {
        state->window = (unsigned char *)
                        ZALLOC(strm, 1U << state->wbits,
                               sizeof(unsigned char));
        if (state->window == NULL) return 1;
    }

    /* if window not in use yet, initialize */
    if (state->wsize == 0) {
        state->wsize = 1U << state->wbits;
        state->write = 0;
        state->whave = 0;
    }

    /* copy state->wsize or less output bytes into the circular window */
    copy = out - strm->avail_out;
    if (copy >= state->wsize) {
        memcpy(state->window, strm->next_out - state->wsize, state->wsize);
        state->write = 0;
        state->whave = state->wsize;
    }
    else {
        dist = state->wsize - state->write;
        if (dist > copy) dist = copy;
        memcpy(state->window + state->write, strm->next_out - copy, dist);
        copy -= dist;
        if (copy) {
            memcpy(state->window, strm->next_out - copy, copy);
            state->write = copy;
            state->whave = state->wsize;
        }
        else {
            state->write += dist;
            if (state->write == state->wsize) state->write = 0;
            if (state->whave < state->wsize) state->whave += dist;
        }
    }
    return 0;
}


#define UPDATE(check, buf, len) \
    (state->flags ? crc32(check, buf, len) : adler32(check, buf, len))

#define CRC2(check, word) \
    do { \
        hbuf[0] = (unsigned char)(word); \
        hbuf[1] = (unsigned char)((word) >> 8); \
        check = crc32(check, hbuf, 2); \
    } while (0)

#define CRC4(check, word) \
    do { \
        hbuf[0] = (unsigned char)(word); \
        hbuf[1] = (unsigned char)((word) >> 8); \
        hbuf[2] = (unsigned char)((word) >> 16); \
        hbuf[3] = (unsigned char)((word) >> 24); \
        check = crc32(check, hbuf, 4); \
    } while (0)

#define LOAD() \
    do { \
        put = strm->next_out; \
        left = strm->avail_out; \
        next = strm->next_in; \
        have = strm->avail_in; \
        hold = state->hold; \
        bits = state->bits; \
    } while (0)

#define RESTORE() \
    do { \
        strm->next_out = put; \
        strm->avail_out = left; \
        strm->next_in = next; \
        strm->avail_in = have; \
        state->hold = hold; \
        state->bits = bits; \
    } while (0)

#define INITBITS() \
    do { \
        hold = 0; \
        bits = 0; \
    } while (0)

#define PULLBYTE() \
    do { \
        if (have == 0) goto inf_leave; \
        have--; \
        hold += (unsigned long)(*next++) << bits; \
        bits += 8; \
    } while (0)

#define NEEDBITS(n) \
    do { \
        while (bits < (unsigned)(n)) \
            PULLBYTE(); \
    } while (0)

#define BITS(n) \
    ((unsigned)hold & ((1U << (n)) - 1))

#define DROPBITS(n) \
    do { \
        hold >>= (n); \
        bits -= (unsigned)(n); \
    } while (0)

#define BYTEBITS() \
    do { \
        hold >>= bits & 7; \
        bits -= bits & 7; \
    } while (0)

#define REVERSE(q) \
    ((((q) >> 24) & 0xff) + (((q) >> 8) & 0xff00) + \
     (((q) & 0xff00) << 8) + (((q) & 0xff) << 24))


int
inflate(z_streamp strm, int flush)
{
    struct inflate_state *state;
    unsigned char *next;    /* next input */
    unsigned char *put;     /* next output */
    unsigned have, left;        /* available input and output */
    unsigned long hold;         /* bit buffer */
    unsigned bits;              /* bits in bit buffer */
    unsigned in, out;           /* save starting available input and output */
    unsigned copy;              /* number of stored or match bytes to copy */
    unsigned char *from;    /* where to copy match bytes from */
    code this_code;                  /* current decoding table entry */
    code last;                  /* parent table entry */
    unsigned len;               /* length to copy for repeats, bits to drop */
    int ret;                    /* return code */
    unsigned char hbuf[4];      /* buffer for gzip header crc calculation */
    static const unsigned short order[19] = /* permutation of code lengths */
        {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

    if (strm == NULL || strm->state == NULL || strm->next_out == NULL ||
        (strm->next_in == NULL && strm->avail_in != 0))
        return Z_STREAM_ERROR;

    state = (struct inflate_state *)strm->state;
    if (state->mode == TYPE) state->mode = TYPEDO;      /* skip check */
    LOAD();
    in = have;
    out = left;
    ret = Z_OK;
    for (;;)
        switch (state->mode) {
        case HEAD:
            if (state->wrap == 0) {
                state->mode = TYPEDO;
                break;
            }
            NEEDBITS(16);
            if ((state->wrap & 2) && hold == 0x8b1f) {  /* gzip header */
                state->check = crc32(0L, NULL, 0);
                CRC2(state->check, hold);
                INITBITS();
                state->mode = FLAGS;
                break;
            }
            state->flags = 0;           /* expect zlib header */
            if (!(state->wrap & 1) ||   /* check if zlib header allowed */
                ((BITS(8) << 8) + (hold >> 8)) % 31) {
                strm->msg = (char *)"incorrect header check";
                state->mode = BAD;
                break;
            }
            if (BITS(4) != Z_DEFLATED) {
                strm->msg = (char *)"unknown compression method";
                state->mode = BAD;
                break;
            }
            DROPBITS(4);
            if (BITS(4) + 8 > state->wbits) {
                strm->msg = (char *)"invalid window size";
                state->mode = BAD;
                break;
            }
            TRACEV((stderr, "inflate:   zlib header ok\n"));
            strm->adler = state->check = adler32(0L, NULL, 0);
            state->mode = hold & 0x200 ? DICTID : TYPE;
            INITBITS();
            break;
        case FLAGS:
            NEEDBITS(16);
            state->flags = (int)(hold);
            if ((state->flags & 0xff) != Z_DEFLATED) {
                strm->msg = (char *)"unknown compression method";
                state->mode = BAD;
                break;
            }
            if (state->flags & 0xe000) {
                strm->msg = (char *)"unknown header flags set";
                state->mode = BAD;
                break;
            }
            if (state->flags & 0x0200) CRC2(state->check, hold);
            INITBITS();
            state->mode = TIME;
        case TIME:
            NEEDBITS(32);
            if (state->flags & 0x0200) CRC4(state->check, hold);
            INITBITS();
            state->mode = OS;
        case OS:
            NEEDBITS(16);
            if (state->flags & 0x0200) CRC2(state->check, hold);
            INITBITS();
            state->mode = EXLEN;
        case EXLEN:
            if (state->flags & 0x0400) {
                NEEDBITS(16);
                state->length = (unsigned)(hold);
                if (state->flags & 0x0200) CRC2(state->check, hold);
                INITBITS();
            }
            state->mode = EXTRA;
        case EXTRA:
            if (state->flags & 0x0400) {
                copy = state->length;
                if (copy > have) copy = have;
                if (copy) {
                    if (state->flags & 0x0200)
                        state->check = crc32(state->check, next, copy);
                    have -= copy;
                    next += copy;
                    state->length -= copy;
                }
                if (state->length) goto inf_leave;
            }
            state->mode = NAME;
        case NAME:
            if (state->flags & 0x0800) {
                if (have == 0) goto inf_leave;
                copy = 0;
                do {
                    len = (unsigned)(next[copy++]);
                } while (len && copy < have);
                if (state->flags & 0x02000)
                    state->check = crc32(state->check, next, copy);
                have -= copy;
                next += copy;
                if (len) goto inf_leave;
            }
            state->mode = COMMENT;
        case COMMENT:
            if (state->flags & 0x1000) {
                if (have == 0) goto inf_leave;
                copy = 0;
                do {
                    len = (unsigned)(next[copy++]);
                } while (len && copy < have);
                if (state->flags & 0x02000)
                    state->check = crc32(state->check, next, copy);
                have -= copy;
                next += copy;
                if (len) goto inf_leave;
            }
            state->mode = HCRC;
        case HCRC:
            if (state->flags & 0x0200) {
                NEEDBITS(16);
                if (hold != (state->check & 0xffff)) {
                    strm->msg = (char *)"header crc mismatch";
                    state->mode = BAD;
                    break;
                }
                INITBITS();
            }
            strm->adler = state->check = crc32(0L, NULL, 0);
            state->mode = TYPE;
            break;
        case DICTID:
            NEEDBITS(32);
            strm->adler = state->check = REVERSE(hold);
            INITBITS();
            state->mode = DICT;
        case DICT:
            if (state->havedict == 0) {
                RESTORE();
                return Z_NEED_DICT;
            }
            strm->adler = state->check = adler32(0L, NULL, 0);
            state->mode = TYPE;
        case TYPE:
            if (flush == Z_BLOCK) goto inf_leave;
        case TYPEDO:
            if (state->last) {
                BYTEBITS();
                state->mode = CHECK;
                break;
            }
            NEEDBITS(3);
            state->last = BITS(1);
            DROPBITS(1);
            switch (BITS(2)) {
            case 0:                             /* stored block */
                TRACEV((stderr, "inflate:     stored block%s\n",
                        state->last ? " (last)" : ""));
                state->mode = STORED;
                break;
            case 1:                             /* fixed block */
                fixedtables(state);
                TRACEV((stderr, "inflate:     fixed codes block%s\n",
                        state->last ? " (last)" : ""));
                state->mode = LEN;              /* decode codes */
                break;
            case 2:                             /* dynamic block */
                TRACEV((stderr, "inflate:     dynamic codes block%s\n",
                        state->last ? " (last)" : ""));
                state->mode = TABLE;
                break;
            case 3:
                strm->msg = (char *)"invalid block type";
                state->mode = BAD;
            }
            DROPBITS(2);
            break;
        case STORED:
            BYTEBITS();                         /* go to byte boundary */
            NEEDBITS(32);
            if ((hold & 0xffff) != ((hold >> 16) ^ 0xffff)) {
                strm->msg = (char *)"invalid stored block lengths";
                state->mode = BAD;
                break;
            }
            state->length = (unsigned)hold & 0xffff;
            TRACEV((stderr, "inflate:       stored length %u\n",
                    state->length));
            INITBITS();
            state->mode = COPY;
        case COPY:
            copy = state->length;
            if (copy) {
                if (copy > have) copy = have;
                if (copy > left) copy = left;
                if (copy == 0) goto inf_leave;
                memcpy(put, next, copy);
                have -= copy;
                next += copy;
                left -= copy;
                put += copy;
                state->length -= copy;
                break;
            }
            TRACEV((stderr, "inflate:       stored end\n"));
            state->mode = TYPE;
            break;
        case TABLE:
            NEEDBITS(14);
            state->nlen = BITS(5) + 257;
            DROPBITS(5);
            state->ndist = BITS(5) + 1;
            DROPBITS(5);
            state->ncode = BITS(4) + 4;
            DROPBITS(4);
            if (state->nlen > 286 || state->ndist > 30) {
                strm->msg = (char *)"too many length or distance symbols";
                state->mode = BAD;
                break;
            }
            TRACEV((stderr, "inflate:       table sizes ok\n"));
            state->have = 0;
            state->mode = LENLENS;
        case LENLENS:
            while (state->have < state->ncode) {
                NEEDBITS(3);
                state->lens[order[state->have++]] = (unsigned short)BITS(3);
                DROPBITS(3);
            }
            while (state->have < 19)
                state->lens[order[state->have++]] = 0;
            state->next = state->codes;
            state->lencode = (code const *)(state->next);
            state->lenbits = 7;
            ret = inflate_table(CODES, state->lens, 19, &(state->next),
                                &(state->lenbits), state->work);
            if (ret) {
                strm->msg = (char *)"invalid code lengths set";
                state->mode = BAD;
                break;
            }
            TRACEV((stderr, "inflate:       code lengths ok\n"));
            state->have = 0;
            state->mode = CODELENS;
        case CODELENS:
            while (state->have < state->nlen + state->ndist) {
                for (;;) {
                    this_code = state->lencode[BITS(state->lenbits)];
                    if ((unsigned)(this_code.bits) <= bits) break;
                    PULLBYTE();
                }
                if (this_code.val < 16) {
                    NEEDBITS(this_code.bits);
                    DROPBITS(this_code.bits);
                    state->lens[state->have++] = this_code.val;
                }
                else {
                    if (this_code.val == 16) {
                        NEEDBITS(this_code.bits + 2);
                        DROPBITS(this_code.bits);
                        if (state->have == 0) {
                            strm->msg = (char *)"invalid bit length repeat";
                            state->mode = BAD;
                            break;
                        }
                        len = state->lens[state->have - 1];
                        copy = 3 + BITS(2);
                        DROPBITS(2);
                    }
                    else if (this_code.val == 17) {
                        NEEDBITS(this_code.bits + 3);
                        DROPBITS(this_code.bits);
                        len = 0;
                        copy = 3 + BITS(3);
                        DROPBITS(3);
                    }
                    else {
                        NEEDBITS(this_code.bits + 7);
                        DROPBITS(this_code.bits);
                        len = 0;
                        copy = 11 + BITS(7);
                        DROPBITS(7);
                    }
                    if (state->have + copy > state->nlen + state->ndist) {
                        strm->msg = (char *)"invalid bit length repeat";
                        state->mode = BAD;
                        break;
                    }
                    while (copy--)
                        state->lens[state->have++] = (unsigned short)len;
                }
            }

            /* build code tables */
            state->next = state->codes;
            state->lencode = (code const *)(state->next);
            state->lenbits = 9;
            ret = inflate_table(LENS, state->lens, state->nlen, &(state->next),
                                &(state->lenbits), state->work);
            if (ret) {
                strm->msg = (char *)"invalid literal/lengths set";
                state->mode = BAD;
                break;
            }
            state->distcode = (code const *)(state->next);
            state->distbits = 6;
            ret = inflate_table(DISTS, state->lens + state->nlen, state->ndist,
                            &(state->next), &(state->distbits), state->work);
            if (ret) {
                strm->msg = (char *)"invalid distances set";
                state->mode = BAD;
                break;
            }
            TRACEV((stderr, "inflate:       codes ok\n"));
            state->mode = LEN;
        case LEN:
            if (have >= 6 && left >= 258) {
                RESTORE();
                inflate_fast(strm, out);
                LOAD();
                break;
            }
            for (;;) {
                this_code = state->lencode[BITS(state->lenbits)];
                if ((unsigned)(this_code.bits) <= bits) break;
                PULLBYTE();
            }
            if (this_code.op && (this_code.op & 0xf0) == 0) {
                last = this_code;
                for (;;) {
                    this_code = state->lencode[last.val +
                            (BITS(last.bits + last.op) >> last.bits)];
                    if ((unsigned)(last.bits + this_code.bits) <= bits) break;
                    PULLBYTE();
                }
                DROPBITS(last.bits);
            }
            DROPBITS(this_code.bits);
            state->length = (unsigned)this_code.val;
            if ((int)(this_code.op) == 0) {
                TRACEVV((stderr, this_code.val >= 0x20 && this_code.val < 0x7f ?
                        "inflate:         literal '%c'\n" :
                        "inflate:         literal 0x%02x\n", this_code.val));
                state->mode = LIT;
                break;
            }
            if (this_code.op & 32) {
                TRACEVV((stderr, "inflate:         end of block\n"));
                state->mode = TYPE;
                break;
            }
            if (this_code.op & 64) {
                strm->msg = (char *)"invalid literal/length code";
                state->mode = BAD;
                break;
            }
            state->extra = (unsigned)(this_code.op) & 15;
            state->mode = LENEXT;
        case LENEXT:
            if (state->extra) {
                NEEDBITS(state->extra);
                state->length += BITS(state->extra);
                DROPBITS(state->extra);
            }
            TRACEVV((stderr, "inflate:         length %u\n", state->length));
            state->mode = DIST;
        case DIST:
            for (;;) {
                this_code = state->distcode[BITS(state->distbits)];
                if ((unsigned)(this_code.bits) <= bits) break;
                PULLBYTE();
            }
            if ((this_code.op & 0xf0) == 0) {
                last = this_code;
                for (;;) {
                    this_code = state->distcode[last.val +
                            (BITS(last.bits + last.op) >> last.bits)];
                    if ((unsigned)(last.bits + this_code.bits) <= bits) break;
                    PULLBYTE();
                }
                DROPBITS(last.bits);
            }
            DROPBITS(this_code.bits);
            if (this_code.op & 64) {
                strm->msg = (char *)"invalid distance code";
                state->mode = BAD;
                break;
            }
            state->offset = (unsigned)this_code.val;
            state->extra = (unsigned)(this_code.op) & 15;
            state->mode = DISTEXT;
        case DISTEXT:
            if (state->extra) {
                NEEDBITS(state->extra);
                state->offset += BITS(state->extra);
                DROPBITS(state->extra);
            }
            if (state->offset > state->whave + out - left) {
                strm->msg = (char *)"invalid distance too far back";
                state->mode = BAD;
                break;
            }
            TRACEVV((stderr, "inflate:         distance %u\n", state->offset));
            state->mode = MATCH;
        case MATCH:
            if (left == 0) goto inf_leave;
            copy = out - left;
            if (state->offset > copy) {         /* copy from window */
                copy = state->offset - copy;
                if (copy > state->write) {
                    copy -= state->write;
                    from = state->window + (state->wsize - copy);
                }
                else
                    from = state->window + (state->write - copy);
                if (copy > state->length) copy = state->length;
            }
            else {                              /* copy from output */
                from = put - state->offset;
                copy = state->length;
            }
            if (copy > left) copy = left;
            left -= copy;
            state->length -= copy;
            do {
                *put++ = *from++;
            } while (--copy);
            if (state->length == 0) state->mode = LEN;
            break;
        case LIT:
            if (left == 0) goto inf_leave;
            *put++ = (unsigned char)(state->length);
            left--;
            state->mode = LEN;
            break;
        case CHECK:
            if (state->wrap) {
                NEEDBITS(32);
                out -= left;
                strm->total_out += out;
                state->total += out;
                if (out)
                    strm->adler = state->check =
                        UPDATE(state->check, put - out, out);
                out = left;
                if ((
                     state->flags ? hold :
                     REVERSE(hold)) != state->check) {
                    strm->msg = (char *)"incorrect data check";
                    state->mode = BAD;
                    break;
                }
                INITBITS();
                TRACEV((stderr, "inflate:   check matches trailer\n"));
            }
            state->mode = LENGTH;
        case LENGTH:
            if (state->wrap && state->flags) {
                NEEDBITS(32);
                if (hold != (state->total & 0xffffffffUL)) {
                    strm->msg = (char *)"incorrect length check";
                    state->mode = BAD;
                    break;
                }
                INITBITS();
                TRACEV((stderr, "inflate:   length matches trailer\n"));
            }
            state->mode = DONE;
        case DONE:
            ret = Z_STREAM_END;
            goto inf_leave;
        case BAD:
            ret = Z_DATA_ERROR;
            goto inf_leave;
        case MEM:
            return Z_MEM_ERROR;
        case SYNC:
        default:
            return Z_STREAM_ERROR;
        }

  inf_leave:
    RESTORE();
    if (state->wsize || (state->mode < CHECK && out != strm->avail_out))
        if (updatewindow(strm, out)) {
            state->mode = MEM;
            return Z_MEM_ERROR;
        }
    in -= strm->avail_in;
    out -= strm->avail_out;
    strm->total_in += in;
    strm->total_out += out;
    state->total += out;
    if (state->wrap && out)
        strm->adler = state->check =
            UPDATE(state->check, strm->next_out - out, out);
    strm->data_type = state->bits + (state->last ? 64 : 0) +
                      (state->mode == TYPE ? 128 : 0);
    if (((in == 0 && out == 0) || flush == Z_FINISH) && ret == Z_OK)
        ret = Z_BUF_ERROR;
    return ret;
}


int
inflateEnd(z_streamp strm)
{
    struct inflate_state *state;
    if (strm == NULL || strm->state == NULL || strm->zfree == (free_func)0)
        return Z_STREAM_ERROR;
    state = (struct inflate_state *)strm->state;
    if (state->window != NULL) ZFREE(strm, state->window);
    ZFREE(strm, strm->state);
    strm->state = NULL;
    TRACEV((stderr, "inflate: end\n"));
    return Z_OK;
}


int
inflateSetDictionary(z_streamp strm, const uint8 *dictionary, uint32 dictLength)
{
    struct inflate_state *state;
    unsigned long id;

    /* check state */
    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    state = (struct inflate_state *)strm->state;
    if (state->mode != DICT) return Z_STREAM_ERROR;

    /* check for correct dictionary id */
    id = adler32(0L, NULL, 0);
    id = adler32(id, dictionary, dictLength);
    if (id != state->check) return Z_DATA_ERROR;

    /* copy dictionary to window */
    if (updatewindow(strm, strm->avail_out)) {
        state->mode = MEM;
        return Z_MEM_ERROR;
    }
    if (dictLength > state->wsize) {
        memcpy(state->window, dictionary + dictLength - state->wsize,
                state->wsize);
        state->whave = state->wsize;
    }
    else {
        memcpy(state->window + state->wsize - dictLength, dictionary,
                dictLength);
        state->whave = dictLength;
    }
    state->havedict = 1;
    TRACEV((stderr, "inflate:   dictionary set\n"));
    return Z_OK;
}


static unsigned
syncsearch(uint32 *have, unsigned char *buf, unsigned len)
{
    unsigned got;
    unsigned next;

    got = *have;
    next = 0;
    while (next < len && got < 4) {
        if ((int)(buf[next]) == (got < 2 ? 0 : 0xff))
            got++;
        else if (buf[next])
            got = 0;
        else
            got = 4 - got;
        next++;
    }
    *have = got;
    return next;
}


int
inflateSync(z_streamp strm)
{
    unsigned len;               /* number of bytes to look at or looked at */
    unsigned long in, out;      /* temporary to save total_in and total_out */
    unsigned char buf[4];       /* to restore bit buffer to byte string */
    struct inflate_state *state;

    /* check parameters */
    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    state = (struct inflate_state *)strm->state;
    if (strm->avail_in == 0 && state->bits < 8) return Z_BUF_ERROR;

    /* if first time, start search in bit buffer */
    if (state->mode != SYNC) {
        state->mode = SYNC;
        state->hold <<= state->bits & 7;
        state->bits -= state->bits & 7;
        len = 0;
        while (state->bits >= 8) {
            buf[len++] = (unsigned char)(state->hold);
            state->hold >>= 8;
            state->bits -= 8;
        }
        state->have = 0;
        syncsearch(&(state->have), buf, len);
    }

    /* search available input */
    len = syncsearch(&(state->have), strm->next_in, strm->avail_in);
    strm->avail_in -= len;
    strm->next_in += len;
    strm->total_in += len;

    /* return no joy or set up to restart inflate() on a new block */
    if (state->have != 4) return Z_DATA_ERROR;
    in = strm->total_in;  out = strm->total_out;
    inflateReset(strm);
    strm->total_in = in;  strm->total_out = out;
    state->mode = TYPE;
    return Z_OK;
}


int
inflateSyncPoint(z_streamp strm)
{
    struct inflate_state *state;

    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    state = (struct inflate_state *)strm->state;
    return state->mode == STORED && state->bits == 0;
}


int
inflateCopy(z_streamp dest, z_streamp source)
{
    struct inflate_state *state;
    struct inflate_state *copy;
    unsigned char *window;

    /* check input */
    if (dest == NULL || source == NULL || source->state == NULL ||
        source->zalloc == (alloc_func)0 || source->zfree == (free_func)0)
        return Z_STREAM_ERROR;
    state = (struct inflate_state *)source->state;

    /* allocate space */
    copy = (struct inflate_state *)
           ZALLOC(source, 1, sizeof(struct inflate_state));
    if (copy == NULL) return Z_MEM_ERROR;
    window = NULL;
    if (state->window != NULL) {
        window = (unsigned char *)
                 ZALLOC(source, 1U << state->wbits, sizeof(unsigned char));
        if (window == NULL) {
            ZFREE(source, copy);
            return Z_MEM_ERROR;
        }
    }

    /* copy state */
    *dest = *source;
    *copy = *state;
    copy->lencode = copy->codes + (state->lencode - state->codes);
    copy->distcode = copy->codes + (state->distcode - state->codes);
    copy->next = copy->codes + (state->next - state->codes);
    if (window != NULL)
        memcpy(window, state->window, 1U << state->wbits);
    copy->window = window;
    dest->state = (internal_state *)copy;
    return Z_OK;
}

} // namespace zlib
