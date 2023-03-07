/* zlib.h -- interface of the 'zlib' general purpose compression library
  version 1.2.1, November 17th, 2003

  Copyright (C) 1995-2003 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu


  The data format used by the zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the files http://www.ietf.org/rfc/rfc1950.txt
  (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).
*/

/*
 * Adapted for C++ and inclusion in Tracker.NewFS 2005 Michael Lotz
 */

#ifndef ZLIB_H
#define ZLIB_H

#include <SupportDefs.h>
#include "zconf.h"

namespace zlib {

#define ZLIB_VERSION "1.2.1"
#define ZLIB_VERNUM 0x1210

typedef void *(*alloc_func)(void *opaque, uint32 items, uint32 size);
typedef void (*free_func)(void *opaque, void *address);

struct internal_state;

typedef struct z_stream_s {
	uint8			*next_in;	/* next input byte */
	uint32			avail_in;	/* number of bytes available at next_in */
	uint32			total_in;	/* total nb of input bytes read so far */

	uint8			*next_out;	/* next output byte should be put there */
	uint32			avail_out;	/* remaining free space at next_out */
	uint32			total_out;	/* total nb of bytes output so far */

	char			*msg;		/* last error message, NULL if no error */
	internal_state	*state;		/* not visible by applications */

	alloc_func		zalloc;		/* used to allocate the internal state */
	free_func		zfree;		/* used to free the internal state */
	void			*opaque;	/* private data object passed to zalloc and zfree */

	int8			data_type;  /* best guess about the data type: ascii or binary */
	uint32			adler;      /* adler32 value of the uncompressed data */
	uint32			reserved;   /* reserved for future use */
} z_stream;

typedef z_stream *z_streamp;

#define Z_NO_FLUSH      0
#define Z_PARTIAL_FLUSH 1 /* will be removed, use Z_SYNC_FLUSH instead */
#define Z_SYNC_FLUSH    2
#define Z_FULL_FLUSH    3
#define Z_FINISH        4
#define Z_BLOCK         5

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)

#define Z_NO_COMPRESSION         0
#define Z_BEST_SPEED             1
#define Z_BEST_COMPRESSION       9
#define Z_DEFAULT_COMPRESSION  (-1)

#define Z_FILTERED            1
#define Z_HUFFMAN_ONLY        2
#define Z_RLE                 3
#define Z_DEFAULT_STRATEGY    0

#define Z_BINARY   0
#define Z_ASCII    1
#define Z_UNKNOWN  2

#define Z_DEFLATED   8

const char *zlibVersion(void);
int deflate(z_streamp strm, int flush);
int deflateEnd(z_streamp strm);
int inflate(z_streamp strm, int flush);
int inflateEnd(z_streamp strm);
int deflateSetDictionary(z_streamp strm, const uint8 *dictionary,
	uint32 dictLength);
int deflateCopy(z_streamp dest, z_streamp source);
int deflateReset(z_streamp strm);
int deflateParams(z_streamp strm, int level, int strategy);
uint32 deflateBound(z_streamp strm, uint32 sourceLen);
int deflatePrime(z_streamp strm, int bits, int value);
int inflateSetDictionary(z_streamp strm, const uint8 *dictionary,
	uint32 dictLength);
int inflateSync(z_streamp strm);
int inflateCopy(z_streamp dest, z_streamp source);
int inflateReset(z_streamp strm);

typedef unsigned (*in_func)(void *, unsigned char **);
typedef int (*out_func)(void *, unsigned char *, unsigned);

int inflateBack(z_stream *strm, in_func in, void *in_desc, out_func out,
	void *out_desc);
int inflateBackEnd(z_stream *strm);
uint32 zlibCompileFlags(void);
int compress(uint8 *dest, uint32 *destLen, const uint8 *source,
	uint32 sourceLen);
int compress2(uint8 *dest, uint32 *destLen, const uint8 *source,
	uint32 sourceLen, int level);
uint32 compressBound(uint32 sourceLen);
int uncompress(uint8 *dest, uint32 *destLen, const uint8 *source,
	uint32 sourceLen);

typedef void *gzFile;

gzFile gzopen(const char *path, const char *mode);
gzFile gzdopen(int fd, const char *mode);
int gzsetparams(gzFile file, int level, int strategy);
int gzread(gzFile file, void * buf, unsigned len);
int gzwrite(gzFile file, const void *buf, unsigned len);
int gzprintf(gzFile file, const char *format, ...);
int gzputs(gzFile file, const char *s);
char *gzgets(gzFile file, char *buf, int len);
int gzputc(gzFile file, int c);
int gzgetc(gzFile file);
int gzungetc(int c, gzFile file);
int gzflush(gzFile file, int flush);
off_t gzseek(gzFile file, off_t offset, int whence);
int gzrewind(gzFile file);
off_t gztell(gzFile file);
int gzeof(gzFile file);
int gzclose(gzFile file);
const char *gzerror(gzFile file, int *errnum);
void gzclearerr(gzFile file);

uint32 adler32(uint32 adler, const uint8 *buf, uint32 len);
uint32 crc32(uint32 crc, const uint8 *buf, uint32 len);

int deflateInit_(z_streamp strm, int level, const char *version,
	int stream_size);
int inflateInit_(z_streamp strm, const char *version, int stream_size);
int deflateInit2_(z_streamp strm, int level, int method, int windowBits,
	int memLevel, int strategy, const char *version, int stream_size);
int inflateInit2_(z_streamp strm, int windowBits, const char *version,
	int stream_size);
int inflateBackInit_(z_stream *strm, int windowBits, unsigned char *window,
	const char *version, int stream_size);

#define deflateInit(strm, level) \
        deflateInit_((strm), (level),       ZLIB_VERSION, sizeof(z_stream))
#define inflateInit(strm) \
        inflateInit_((strm),                ZLIB_VERSION, sizeof(z_stream))
#define deflateInit2(strm, level, method, windowBits, memLevel, strategy) \
        deflateInit2_((strm),(level),(method),(windowBits),(memLevel),\
                      (strategy),           ZLIB_VERSION, sizeof(z_stream))
#define inflateInit2(strm, windowBits) \
        inflateInit2_((strm), (windowBits), ZLIB_VERSION, sizeof(z_stream))
#define inflateBackInit(strm, windowBits, window) \
        inflateBackInit_((strm), (windowBits), (window), \
        ZLIB_VERSION, sizeof(z_stream))

const char *zError(int err);
int inflateSyncPoint(z_streamp z);
const uint32 *get_crc_table(void);

#define ZASSERT(x, y)	/* empty */
#define ZTRACE(x)		/* empty */
#define TRACEV(x)		/* empty */
#define TRACEVV(x)		/* empty */
#define TRACECV(x, y)	/* empty */

} // namespace zlib

#endif /* ZLIB_H */
