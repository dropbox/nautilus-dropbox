/*
 *
 * g-util.c nautilus-dropbox independent, gtk/glib/gdk dependendent util functions
 *
 */

#include <glib.h>
#include <glib/gprintf.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "g-util.h"

gboolean
g_util_execute_command_line(const gchar *command_line) {
  gint argc;
  gchar **argv;
  
  g_shell_parse_argv(command_line, &argc, &argv, NULL);
  
  return gdk_spawn_on_screen(gdk_screen_get_default(),
			     NULL, argv, NULL,
			     G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
			     G_SPAWN_STDERR_TO_DEV_NULL,
			     NULL, NULL,
			     NULL, NULL);
}

void
g_util_destroy_string(gpointer data) {
  g_string_free((GString *) data, TRUE);
  return;
}

typedef struct {
  guint *mark;
  guint ev_id_watch;
  gpointer ud;
  GIOFunc func;
  GDestroyNotify notify;
} DIORead;

typedef struct {
  guint *mark;
  guint ev_id_read;
} DIOWatch;

static void free_dior(DIORead *dior) {
  if (*dior->mark != 0) {
    *dior->mark = 0;
    g_source_remove(dior->ev_id_watch);
  }
  else {
    g_free(dior->mark);
  }

  dior->notify(dior->ud);

  g_free(dior);
}

static void free_diow(DIOWatch *diow) {
  if (*diow->mark != 0) {
    *diow->mark = 0;
    g_source_remove(diow->ev_id_read);
  }
  else {
    g_free(diow->mark);
  }

  g_free(diow);
}

static gboolean
handle_bad_io(GIOChannel *chan,  GIOCondition cond,
	      DIOWatch *diow) {
  return FALSE;
}

static gboolean
handle_io(GIOChannel *chan,
	  GIOCondition cond,
	  DIORead *dior) {
  return dior->func(chan, cond, dior->ud);
}

guint g_util_dependable_io_read_watch(GIOChannel *chan,
				      gint priority, 
				      GIOFunc func,
				      gpointer ud,
				      GDestroyNotify notify) {
  DIORead *dior = g_new(DIORead, 1);
  DIOWatch *diow = g_new(DIOWatch, 1);

  /* initialize the mark to zero */
  *(dior->mark = diow->mark = g_new(guint, 1)) = 1;
  dior->func = func;
  dior->ud = ud;
  dior->notify = notify;
  
  diow->ev_id_read =
    g_io_add_watch_full(chan, priority,
			G_IO_IN | G_IO_PRI,
			(GIOFunc) handle_io, dior, 
			(GDestroyNotify) free_dior);
  dior->ev_id_watch =
    g_io_add_watch_full(chan, priority + 100,
			G_IO_HUP | G_IO_ERR | G_IO_NVAL,
			(GIOFunc) handle_bad_io, diow,
			(GDestroyNotify) free_diow);
  
  return diow->ev_id_read;
}
