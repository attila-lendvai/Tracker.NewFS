/* deflate.h -- internal compression state
 * Copyright (C) 1995-2002 Jean-loup Gailly
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
 * Adapted for C++ and inclusion in Tracker.NewFS 2005 Michael Lotz
 */

#ifndef DEFLATE_H
#define DEFLATE_H

#include "zutil.h"

namespace zlib {

/* define NO_GZIP when compiling if you want to disable gzip header and
   trailer creation by deflate().  NO_GZIP would be used to avoid linking in
   the crc code when it is not needed.  For shared libraries, gzip encoding
   should be left enabled. */
#ifndef NO_GZIP
#  define GZIP
#endif

/* ===========================================================================
 * Internal compression state.
 */

#define LENGTH_CODES 29
/* number of length codes, not counting the special END_BLOCK code */

#define LITERALS  256
/* number of literal bytes 0..255 */

#define L_CODES (LITERALS+1+LENGTH_CODES)
/* number of Literal or Length codes, including the END_BLOCK code */

#define D_CODES   30
/* number of distance codes */

#define BL_CODES  19
/* number of codes used to transfer the bit lengths */

#define HEAP_SIZE (2*L_CODES+1)
/* maximum heap size */

#define MAX_BITS 15
/* All codes must not exceed MAX_BITS bits */

#define INIT_STATE	42
#define BUSY_STATE   113
#define FINISH_STATE 666
/* Stream status */

/* Data structure describing a single value and its code string. */
typedef struct ct_data_s {
	union {
		uint16	freq;		/* frequency count */
		uint16	code;		/* bit string */
	} fc;
	union {
		uint16	dad;		/* father node in Huffman tree */
		uint16	len;		/* length of bit string */
	} dl;
} ct_data;

#define Freq fc.freq
#define Code fc.code
#define Dad  dl.dad
#define Len  dl.len

typedef struct static_tree_desc_s  static_tree_desc;

typedef struct tree_desc_s {
	ct_data				*dyn_tree;	/* the dynamic tree */
	int					max_code;	/* largest code with non zero frequency */
	static_tree_desc	*stat_desc;	/* the corresponding static tree */
} tree_desc;

typedef uint16 Pos;
typedef uint32 IPos;

/* A Pos is an index in the character window. We use short instead of int to
 * save space in the various tables. IPos is used only for parameter passing.
 */

typedef struct internal_state {
	z_streamp	strm;				/* pointer back to this zlib stream */
	int			status;				/* as the name implies */
	uint8		*pending_buf;		/* output still pending */
	uint32		pending_buf_size;	/* size of pending_buf */
	uint8		*pending_out;		/* next pending byte to output to the stream */
	int			pending;			/* nb of bytes in the pending buffer */
	int			wrap;				/* bit 0 true for zlib, bit 1 true for gzip */
	uint8		data_type;			/* UNKNOWN, BINARY or ASCII */
	uint8		method;				/* STORED (for zip only) or DEFLATED */
	int			last_flush;			/* value of flush param for previous deflate call */

	uint32		w_size;				/* LZ77 window size (32K by default) */
	uint32		w_bits;				/* log2(w_size)  (8..16) */
	uint32		w_mask;				/* w_size - 1 */

	uint8		*window;
	uint32		window_size;
	Pos			*prev;
	Pos			*head;				/* Heads of the hash chains or NIL. */

	uint32		ins_h;				/* hash index of string to be inserted */
	uint32		hash_size;			/* number of elements in hash table */
	uint32		hash_bits;			/* log2(hash_size) */
	uint32		hash_mask;			/* hash_size-1 */

	uint32		hash_shift;
	int32		block_start;

	uint32		match_length;		/* length of best match */
	IPos		prev_match;			/* previous match */
	int32		match_available;	/* set if previous match exists */
	uint32		strstart;			/* start of string to insert */
	uint32		match_start;		/* start of matching string */
	uint32		lookahead;			/* number of valid bytes ahead in window */

	uint32		prev_length;
	uint32		max_chain_length;
	uint32		max_lazy_match;

#define max_insert_length  max_lazy_match

	int32		level;				/* compression level (1..9) */
	int32		strategy;			/* favor or force Huffman coding*/

	uint32		good_match;
	int32		nice_match;

	struct ct_data_s	dyn_ltree[HEAP_SIZE];	/* literal and length tree */
	struct ct_data_s	dyn_dtree[2*D_CODES+1];	/* distance tree */
	struct ct_data_s	bl_tree[2*BL_CODES+1];	/* Huffman tree for bit lengths */

	struct tree_desc_s	l_desc;					/* desc. for literal tree */
	struct tree_desc_s	d_desc;					/* desc. for distance tree */
	struct tree_desc_s	bl_desc;				/* desc. for bit length tree */

	uint16		bl_count[MAX_BITS+1];

	int32		heap[2*L_CODES+1];	/* heap used to build the Huffman trees */
	int32		heap_len;			/* number of elements in the heap */
	int32		heap_max;			/* element of largest frequency */

	uint8		depth[2*L_CODES+1];

	uint8		*l_buf;				/* buffer for literals or lengths */

	uint32		lit_bufsize;
	uint32		last_lit;			/* running index in l_buf */

	uint16		*d_buf;

	uint32		opt_len;			/* bit length of current block with optimal trees */
	uint32		static_len;			/* bit length of current block with static trees */
	uint32		matches;			/* number of string matches in current block */
	int32		last_eob_len;		/* bit length of EOB code for last block */

	uint16		bi_buf;
	int32		bi_valid;
} deflate_state;

/* Output a byte on the stream.
 * IN assertion: there is enough room in pending_buf.
 */
#define put_byte(s, c) {s->pending_buf[s->pending++] = (c);}


#define MIN_LOOKAHEAD (MAX_MATCH+MIN_MATCH+1)
/* Minimum amount of lookahead, except at the end of the input file.
 * See deflate.c for comments about the MIN_MATCH+1.
 */

#define MAX_DIST(s)  ((s)->w_size-MIN_LOOKAHEAD)
/* In order to simplify the code, particularly on 16 bit machines, match
 * distances are limited to MAX_DIST instead of WSIZE.
 */

void _tr_init(deflate_state *s);
int32 _tr_tally(deflate_state *s, uint32 dist, uint32 lc);
void _tr_flush_block(deflate_state *s, uint8 *buf, uint32 stored_len, int32 eof);
void _tr_align(deflate_state *s);
void _tr_stored_block(deflate_state *s, uint8 *buf, uint32 stored_len, int32 eof);

#define d_code(dist) \
	((dist) < 256 ? _dist_code[dist] : _dist_code[256 + ((dist) >> 7)])

extern const uint8 _length_code[];
extern const uint8 _dist_code[];

# define _tr_tally_lit(s, c, flush) \
{ \
	uint8 cc = (c); \
	s->d_buf[s->last_lit] = 0; \
	s->l_buf[s->last_lit++] = cc; \
	s->dyn_ltree[cc].Freq++; \
	flush = (s->last_lit == s->lit_bufsize-1); \
}

# define _tr_tally_dist(s, distance, length, flush) \
{ \
	uint8 len = (length); \
	uint16 dist = (distance); \
	s->d_buf[s->last_lit] = dist; \
	s->l_buf[s->last_lit++] = len; \
	dist--; \
	s->dyn_ltree[_length_code[len]+LITERALS+1].Freq++; \
	s->dyn_dtree[d_code(dist)].Freq++; \
	flush = (s->last_lit == s->lit_bufsize-1); \
}

} // namespace zlib

#endif /* DEFLATE_H */
