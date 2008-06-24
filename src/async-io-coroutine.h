#ifndef ASYNC_IO_COROUTINE_H
#define ASYNC_IO_COROUTINE_H

#include <glib.h>

G_BEGIN_DECLS

#define CRBEGIN(pos) switch (pos) { case 0:
#define CREND } return FALSE
#define CRYIELD(pos) do { pos = __LINE__; return TRUE; case __LINE__:;} while (0)
#define CRHALT return FALSE  

#define CRREADLINE(pos, chan, where)                             \
  while (1) {							\
    gchar *__line;						\
    gsize __line_length, __newline_pos;				\
    GIOStatus __iostat;							\
    									\
    __iostat = g_io_channel_read_line(chan, &__line,			\
				      &__line_length,			\
				      &__newline_pos, NULL);		\
    if (__iostat == G_IO_STATUS_AGAIN) {				\
      CRYIELD(pos);                                                \
    }									\
    else if (__iostat == G_IO_STATUS_NORMAL) {				\
      *(__line + __newline_pos) = '\0';				\
      where = __line;						\
      break;							\
    }								\
    else if (__iostat == G_IO_STATUS_EOF ||			\
	     __iostat == G_IO_STATUS_ERROR) {			\
      CRHALT;							\
    }								\
    else {							\
      g_assert_not_reached();					\
    }								\
  }

G_END_DECLS

#endif
