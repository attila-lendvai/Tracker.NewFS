/* inftrees.h -- header to use inftrees.c
 * Copyright (C) 1995-2003 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

namespace zlib {

typedef struct {
    uint8 op;           /* operation, extra bits, table bits */
    uint8 bits;         /* bits in this part of the code */
    uint16 val;         /* offset in table or code value */
} code;

#define ENOUGH 1440
#define MAXD 154

/* Type of code to build for inftable() */
typedef enum {
    CODES,
    LENS,
    DISTS
} codetype;

extern int inflate_table(codetype type, uint16 *lens,
                             uint32 codes, code **table,
                             uint32 *bits, uint16 *work);

} // namespace zlib
