/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in lau.h
*/


#ifndef lzio_h
#define lzio_h

#include "lau.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : lauZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define lauZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define lauZ_buffer(buff)	((buff)->buffer)
#define lauZ_sizebuffer(buff)	((buff)->buffsize)
#define lauZ_bufflen(buff)	((buff)->n)

#define lauZ_buffremove(buff,i)	((buff)->n -= cast_sizet(i))
#define lauZ_resetbuffer(buff) ((buff)->n = 0)


#define lauZ_resizebuffer(L, buff, size) \
	((buff)->buffer = lauM_reallocvchar(L, (buff)->buffer, \
				(buff)->buffsize, size), \
	(buff)->buffsize = size)

#define lauZ_freebuffer(L, buff)	lauZ_resizebuffer(L, buff, 0)


LAUI_FUNC void lauZ_init (lau_State *L, ZIO *z, lau_Reader reader,
                                        void *data);
LAUI_FUNC size_t lauZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */

LAUI_FUNC const void *lauZ_getaddr (ZIO* z, size_t n);


/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  lau_Reader reader;		/* reader function */
  void *data;			/* additional data */
  lau_State *L;			/* Lau state (for reader) */
};


LAUI_FUNC int lauZ_fill (ZIO *z);

#endif
