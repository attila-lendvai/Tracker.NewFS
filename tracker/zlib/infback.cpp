/* infback.c -- inflate using a call-back interface
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


int
inflateBackInit_(z_stream *strm, int windowBits, unsigned char *window,
	const char *version, int stream_size)
{
    struct inflate_state *state;

    if (version == NULL || version[0] != ZLIB_VERSION[0] ||
        stream_size != (int)(sizeof(z_stream)))
        return Z_VERSION_ERROR;
    if (strm == NULL || window == NULL ||
        windowBits < 8 || windowBits > 15)
        return Z_STREAM_ERROR;
    strm->msg = NULL;                 /* in case we return an error */
    if (strm->zalloc == NULL) {
        strm->zalloc = (alloc_func)zcalloc;
        strm->opaque = NULL;
    }
    if (strm->zfree == (free_func)0) strm->zfree = zcfree;
    state = (struct inflate_state *)ZALLOC(strm, 1,
                                               sizeof(struct inflate_state));
    if (state == NULL) return Z_MEM_ERROR;
    TRACEV((stderr, "inflate: allocated\n"));
    strm->state = (internal_state *)state;
    state->wbits = windowBits;
    state->wsize = 1U << windowBits;
    state->window = window;
    state->write = 0;
    state->whave = 0;
    return Z_OK;
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

#define PULL() \
    do { \
        if (have == 0) { \
            have = in(in_desc, &next); \
            if (have == 0) { \
                next = NULL; \
                ret = Z_BUF_ERROR; \
                goto inf_leave; \
            } \
        } \
    } while (0)

#define PULLBYTE() \
    do { \
        PULL(); \
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

#define ROOM() \
    do { \
        if (left == 0) { \
            put = state->window; \
            left = state->wsize; \
            state->whave = left; \
            if (out(out_desc, put, left)) { \
                ret = Z_BUF_ERROR; \
                goto inf_leave; \
            } \
        } \
    } while (0)


int
inflateBack(z_stream *strm, in_func in, void *in_desc, out_func out,
	void *out_desc)
{
    struct inflate_state *state;
    unsigned char *next;    /* next input */
    unsigned char *put;     /* next output */
    unsigned have, left;        /* available input and output */
    unsigned long hold;         /* bit buffer */
    unsigned bits;              /* bits in bit buffer */
    unsigned copy;              /* number of stored or match bytes to copy */
    unsigned char *from;    /* where to copy match bytes from */
    code this_code;             /* current decoding table entry */
    code last;                  /* parent table entry */
    unsigned len;               /* length to copy for repeats, bits to drop */
    int ret;                    /* return code */
    static const unsigned short order[19] = /* permutation of code lengths */
        {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

    /* Check that the strm exists and that the state was initialized */
    if (strm == NULL || strm->state == NULL)
        return Z_STREAM_ERROR;
    state = (struct inflate_state *)strm->state;

    /* Reset the state */
    strm->msg = NULL;
    state->mode = TYPE;
    state->last = 0;
    state->whave = 0;
    next = strm->next_in;
    have = next != NULL ? strm->avail_in : 0;
    hold = 0;
    bits = 0;
    put = state->window;
    left = state->wsize;

    /* Inflate until end of block marked as last */
    for (;;)
        switch (state->mode) {
        case TYPE:
            /* determine and dispatch block type */
            if (state->last) {
                BYTEBITS();
                state->mode = DONE;
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
            /* get and verify stored block length */
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

            /* copy stored block from input to output */
            while (state->length != 0) {
                copy = state->length;
                PULL();
                ROOM();
                if (copy > have) copy = have;
                if (copy > left) copy = left;
                memcpy(put, next, copy);
                have -= copy;
                next += copy;
                left -= copy;
                put += copy;
                state->length -= copy;
            }
            TRACEV((stderr, "inflate:       stored end\n"));
            state->mode = TYPE;
            break;

        case TABLE:
            /* get dynamic table entries descriptor */
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

            /* get code length code lengths (not a typo) */
            state->have = 0;
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

            /* get length and distance code code lengths */
            state->have = 0;
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
                        len = (unsigned)(state->lens[state->have - 1]);
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
            /* use inflate_fast() if we have enough input and output */
            if (have >= 6 && left >= 258) {
                RESTORE();
                if (state->whave < state->wsize)
                    state->whave = state->wsize - left;
                inflate_fast(strm, state->wsize);
                LOAD();
                break;
            }

            /* get a literal, length, or end-of-block code */
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

            /* process literal */
            if (this_code.op == 0) {
                TRACEVV((stderr, this_code.val >= 0x20 && this_code.val < 0x7f ?
                        "inflate:         literal '%c'\n" :
                        "inflate:         literal 0x%02x\n", this_code.val));
                ROOM();
                *put++ = (unsigned char)(state->length);
                left--;
                state->mode = LEN;
                break;
            }

            /* process end of block */
            if (this_code.op & 32) {
                TRACEVV((stderr, "inflate:         end of block\n"));
                state->mode = TYPE;
                break;
            }

            /* invalid code */
            if (this_code.op & 64) {
                strm->msg = (char *)"invalid literal/length code";
                state->mode = BAD;
                break;
            }

            /* length code -- get extra bits, if any */
            state->extra = (unsigned)(this_code.op) & 15;
            if (state->extra != 0) {
                NEEDBITS(state->extra);
                state->length += BITS(state->extra);
                DROPBITS(state->extra);
            }
            TRACEVV((stderr, "inflate:         length %u\n", state->length));

            /* get distance code */
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

            /* get distance extra bits, if any */
            state->extra = (unsigned)(this_code.op) & 15;
            if (state->extra != 0) {
                NEEDBITS(state->extra);
                state->offset += BITS(state->extra);
                DROPBITS(state->extra);
            }
            if (state->offset > state->wsize - (state->whave < state->wsize ?
                                                left : 0)) {
                strm->msg = (char *)"invalid distance too far back";
                state->mode = BAD;
                break;
            }
            TRACEVV((stderr, "inflate:         distance %u\n", state->offset));

            /* copy match from window to output */
            do {
                ROOM();
                copy = state->wsize - state->offset;
                if (copy < left) {
                    from = put + copy;
                    copy = left - copy;
                }
                else {
                    from = put - state->offset;
                    copy = left;
                }
                if (copy > state->length) copy = state->length;
                state->length -= copy;
                left -= copy;
                do {
                    *put++ = *from++;
                } while (--copy);
            } while (state->length != 0);
            break;

        case DONE:
            /* inflate stream terminated properly -- write leftover output */
            ret = Z_STREAM_END;
            if (left < state->wsize) {
                if (out(out_desc, state->window, state->wsize - left))
                    ret = Z_BUF_ERROR;
            }
            goto inf_leave;

        case BAD:
            ret = Z_DATA_ERROR;
            goto inf_leave;

        default:                /* can't happen, but makes compilers happy */
            ret = Z_STREAM_ERROR;
            goto inf_leave;
        }

    /* Return unused input */
  inf_leave:
    strm->next_in = next;
    strm->avail_in = have;
    return ret;
}


int
inflateBackEnd(z_stream *strm)
{
    if (strm == NULL || strm->state == NULL || strm->zfree == (free_func)0)
        return Z_STREAM_ERROR;
    ZFREE(strm, strm->state);
    strm->state = NULL;
    TRACEV((stderr, "inflate: end\n"));
    return Z_OK;
}

} // namespace zlib
