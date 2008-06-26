#ifndef G_UTIL_H
#define G_UTIL_H

#include <glib.h>
#include <glib/gprintf.h>

G_BEGIN_DECLS

#define debug_enter() {g_print("Entering "); g_print(__FUNCTION__); g_printf("\n");}
#define debug(format, ...) {g_print(__FUNCTION__); g_print(": "); \
    g_printf(format, ## __VA_ARGS__); g_print("\n");}
#define debug_return(v) {g_print("Exiting "); g_print(__FUNCTION__); g_printf("\n"); return v;}

gboolean
g_util_execute_command_line(const gchar *command_line);

void
g_util_destroy_string(gpointer data);

guint g_util_dependable_io_read_watch(GIOChannel *chan,
				      gint priority, 
				      GIOFunc func,
				      gpointer ud,
				      GDestroyNotify notify);

G_END_DECLS

#endif
