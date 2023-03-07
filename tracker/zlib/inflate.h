/* inflate.h -- internal inflate state definition
 * Copyright (C) 1995-2003 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
 * Adapted for C++ and inclusion in Tracker.NewFS 2005 Michael Lotz
 */

namespace zlib {

#define GUNZIP

/* Possible inflate modes between inflate() calls */
typedef enum {
    HEAD,       /* i: waiting for magic header */
#ifdef GUNZIP
    FLAGS,      /* i: waiting for method and flags (gzip) */
    TIME,       /* i: waiting for modification time (gzip) */
    OS,         /* i: waiting for extra flags and operating system (gzip) */
    EXLEN,      /* i: waiting for extra length (gzip) */
    EXTRA,      /* i: waiting for extra bytes (gzip) */
    NAME,       /* i: waiting for end of file name (gzip) */
    COMMENT,    /* i: waiting for end of comment (gzip) */
    HCRC,       /* i: waiting for header crc (gzip) */
#endif
    DICTID,     /* i: waiting for dictionary check value */
    DICT,       /* waiting for inflateSetDictionary() call */
        TYPE,       /* i: waiting for type bits, including last-flag bit */
        TYPEDO,     /* i: same, but skip check to exit inflate on new block */
        STORED,     /* i: waiting for stored size (length and complement) */
        COPY,       /* i/o: waiting for input or output to copy stored block */
        TABLE,      /* i: waiting for dynamic block table lengths */
        LENLENS,    /* i: waiting for code length code lengths */
        CODELENS,   /* i: waiting for length/lit and distance code lengths */
            LEN,        /* i: waiting for length/lit code */
            LENEXT,     /* i: waiting for length extra bits */
            DIST,       /* i: waiting for distance code */
            DISTEXT,    /* i: waiting for distance extra bits */
            MATCH,      /* o: waiting for output space to copy string */
            LIT,        /* o: waiting for output space to write literal */
    CHECK,      /* i: waiting for 32-bit check value */
#ifdef GUNZIP
    LENGTH,     /* i: waiting for 32-bit length (gzip) */
#endif
    DONE,       /* finished check, done -- remain here until reset */
    BAD,        /* got a data error -- remain here until reset */
    MEM,        /* got an inflate() memory error -- remain here until reset */
    SYNC        /* looking for synchronization bytes to restart inflate() */
} inflate_mode;

/*
    State transitions between above modes -

    (most modes can go to the BAD or MEM mode -- not shown for clarity)

    Process header:
        HEAD -> (gzip) or (zlib)
        (gzip) -> FLAGS -> TIME -> OS -> EXLEN -> EXTRA -> NAME
        NAME -> COMMENT -> HCRC -> TYPE
        (zlib) -> DICTID or TYPE
        DICTID -> DICT -> TYPE
    Read deflate blocks:
            TYPE -> STORED or TABLE or LEN or CHECK
            STORED -> COPY -> TYPE
            TABLE -> LENLENS -> CODELENS -> LEN
    Read deflate codes:
                LEN -> LENEXT or LIT or TYPE
                LENEXT -> DIST -> DISTEXT -> MATCH -> LEN
                LIT -> LEN
    Process trailer:
        CHECK -> LENGTH -> DONE
 */

/* state maintained between inflate() calls.  Approximately 7K bytes. */
struct inflate_state {
	inflate_mode mode;			/* current inflate mode */
	int8 last;					/* true if processing last block */
	int8 wrap;					/* bit 0 true for zlib, bit 1 true for gzip */
	int8 havedict;				/* true if dictionary provided */
	int8 flags;					/* gzip header method and flags (0 if zlib) */
	uint32 check;				/* protected copy of check value */
	uint32 total;				/* protected copy of output count */
		/* sliding window */
	uint32 wbits;				/* log base 2 of requested window size */
	uint32 wsize;				/* window size or zero if not using window */
	uint32 whave;				/* valid bytes in the window */
	uint32 write;				/* window write index */
	uint8 *window;				/* allocated sliding window, if needed */
		/* bit accumulator */
	uint32 hold;				/* input bit accumulator */
	uint32 bits;				/* number of bits in "in" */
		/* for string and stored block copying */
	uint32 length;				/* literal or length of data to copy */
	uint32 offset;				/* distance back to copy string from */
		/* for table and code decoding */
	uint32 extra;				/* extra bits needed */
		/* fixed and dynamic code tables */
	code const *lencode;		/* starting table for length/literal codes */
	code const *distcode;		/* starting table for distance codes */
	uint32 lenbits;				/* index bits for lencode */
	uint32 distbits;			/* index bits for distcode */
		/* dynamic table building */
	uint32 ncode;				/* number of code length code lengths */
	uint32 nlen;				/* number of length code lengths */
	uint32 ndist;				/* number of distance code lengths */
	uint32 have;				/* number of code lengths in lens[] */
	code *next;					/* next available space in codes[] */
	uint16 lens[320];			/* temporary storage for code lengths */
	uint16 work[288];			/* work area for code table building */
	code codes[ENOUGH];			/* space for code tables */
};

} // namespace zlib
