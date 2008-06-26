/*
 *
 *  nautilus-dropbox-hooks.c
 * 
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libnautilus-extension/nautilus-file-info.h>

#include "g-util.h"
#include "async-io-coroutine.h"
#include "nautilus-dropbox-common.h"
#include "nautilus-dropbox.h"
#include "nautilus-dropbox-hooks.h"
#include "nautilus-dropbox-command.h"

static gboolean
try_to_connect(NautilusDropbox *cvs);

static void
handle_copy_to_clipboard(NautilusDropbox *cvs, GHashTable *args) {
  GtkClipboard *clip;
  gchar *text;

  if ((text = g_hash_table_lookup(args, "text")) != NULL) {
    clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clip, text, -1);
  }
  
  return;
}

static void
handle_shell_touch(NautilusDropbox *cvs, GHashTable *args) {
  gchar *path;

  //  debug_enter();

  if ((path = g_hash_table_lookup(args, "path")) != NULL) {
    GList *li;
  
    for (li = cvs->file_store; li != NULL; li = g_list_next(li)) {
      if (strcmp(g_filename_from_uri(nautilus_file_info_get_uri(NAUTILUS_FILE_INFO(li->data)),
				     NULL, NULL),
		 path) == 0) {
	/* found it */
	nautilus_file_info_invalidate_extension_info(NAUTILUS_FILE_INFO(li->data));
	break;
      }
    }
  }

  return;
}

static void
handle_launch_url(NautilusDropbox *cvs, GHashTable *args) {
  gchar *url;
  
  //  debug_enter();

  if ((url = g_hash_table_lookup(args, "url")) != NULL) {
    gchar *command_line;

    command_line = g_strdup_printf("gnome-open %s", url);

    if (!nautilus_dropbox_common_execute_command_line(command_line)) {
      /* TODO: popup some notice that says we couldn't open the window */
    }

    g_free(command_line);
  }
}

static void
handle_launch_folder(NautilusDropbox *cvs, GHashTable *args) {
  gchar *path;

  if ((path = g_hash_table_lookup(args, "path")) != NULL) {
    gchar *command_line, *escaped_string;

    escaped_string = g_strescape(path, NULL);
    command_line = g_strdup_printf("nautilus \"%s\"", escaped_string);

    nautilus_dropbox_common_execute_command_line(command_line);

    g_free(escaped_string);
    g_free(command_line);
  }
}

static gboolean
handle_hook_server_input(GIOChannel *chan,
			 GIOCondition cond,
			 NautilusDropbox *cvs) {

  if (cond == G_IO_ERR || cond == G_IO_HUP) {
    return FALSE;
  }

  /* we have some sweet macros defined that allow us to write this
     async event handler like a microthread yeahh, watch out for context */
  CRBEGIN(cvs->hookserv.hhsi.line);
  while (1) {
    cvs->hookserv.hhsi.command_args =
      g_hash_table_new_full((GHashFunc) g_str_hash,
			    (GEqualFunc) g_str_equal,
			    g_free, g_free);

    
    /* read the command name */
    /* TODO: unescape tabs and newlines */
    CRREADLINE(cvs->hookserv.hhsi.line, chan, cvs->hookserv.hhsi.command_name);

    /* now read each arg line until we receive "done" */
    /* TODO: NO THIS IS BAD WE SHOULD NOT LOOP BASED ON OUR INPUT */
    while (1) {
      gchar *line;
      CRREADLINE(cvs->hookserv.hhsi.line, chan, line);

      if (strcmp("done", line) == 0) {
	g_free(line);
	break;
      }
      else {
	char *tab_loc;
	gchar *key_str, *val_str;
	tab_loc = strchr(line, '\t');
	
	/* TODO: unescape tabs and newlines */
	if (tab_loc != NULL)  {
	  key_str = g_strndup(line, tab_loc - line);
	  val_str = g_strdup(tab_loc+1);
	  
	  g_hash_table_insert(cvs->hookserv.hhsi.command_args, key_str, val_str);
	  g_free(line);
	}
	else {
	  /* if the input is invalid, then we should stop this connection */
	  CRHALT;
	}
      }
    }

    /*debug("got a hook: %s", cvs->hookserv.hhsi.command_name); */
    
    {
      DropboxUpdateHook dbuh;
      dbuh = (DropboxUpdateHook)
	g_hash_table_lookup(cvs->dispatch_table,
			    cvs->hookserv.hhsi.command_name);
      if (dbuh != NULL) {
	dbuh(cvs, cvs->hookserv.hhsi.command_args);
      }
    }
    
    g_free(cvs->hookserv.hhsi.command_name);
    g_hash_table_unref(cvs->hookserv.hhsi.command_args);
    cvs->hookserv.hhsi.command_name = NULL;
    cvs->hookserv.hhsi.command_args = NULL;
  }
  CREND;
}

static void
watch_killer(NautilusDropbox *cvs) {
  debug("hook client disconnected");

  g_mutex_lock(cvs->hookserv.connected_mutex);
  cvs->hookserv.connected = FALSE;
  g_cond_signal(cvs->hookserv.connected_cond);
  g_mutex_unlock(cvs->hookserv.connected_mutex);

  nautilus_dropbox_command_force_reconnect(cvs);

  /* we basically just have to free the memory allocated in the
     handle_hook_server_init ctx */

  if (cvs->hookserv.hhsi.command_name != NULL) {
    g_free(cvs->hookserv.hhsi.command_name);
    cvs->hookserv.hhsi.command_name = NULL;
  }

  if (cvs->hookserv.hhsi.command_args != NULL) {
    g_hash_table_unref(cvs->hookserv.hhsi.command_args);
    cvs->hookserv.hhsi.command_args = NULL;
  }

  g_io_channel_unref(cvs->hookserv.chan);
  cvs->hookserv.chan = NULL;
  cvs->hookserv.event_source = 0;
  cvs->hookserv.socket = 0;

  /* lol we also have to start a new connection */
  try_to_connect(cvs);
}

static gboolean
try_to_connect(NautilusDropbox *cvs) {
  /* create socket */
  cvs->hookserv.socket = socket(PF_UNIX, SOCK_STREAM, 0);
  
  /* connect to server, might fail of course */
  {
    int err;
    struct sockaddr_un addr;
    socklen_t addr_len;
    
    /* intialize address structure */
    addr.sun_family = AF_UNIX;
    g_snprintf(addr.sun_path,
	       sizeof(addr.sun_path),
	       "%s/.dropbox/iface_socket",
	       g_get_home_dir());
    addr_len = sizeof(addr) - sizeof(addr.sun_path) + strlen(addr.sun_path);

    err = connect(cvs->hookserv.socket, (struct sockaddr *) &addr,
		  addr_len);

    /* if there was an error we have to try again later */
    if (err == -1) {
      close(cvs->hookserv.socket);
      g_timeout_add_seconds(1, (GSourceFunc) try_to_connect, cvs);
      return FALSE;
    }
  }

  debug("hook client connected");

  g_mutex_lock(cvs->hookserv.connected_mutex);
  cvs->hookserv.connected = TRUE;
  g_cond_signal(cvs->hookserv.connected_cond);
  g_mutex_unlock(cvs->hookserv.connected_mutex);

  /* great we connected!, let's create the channel and wait on it */
  cvs->hookserv.chan = g_io_channel_unix_new(cvs->hookserv.socket);
  g_io_channel_set_line_term(cvs->hookserv.chan, "\n", -1);
  g_io_channel_set_close_on_unref(cvs->hookserv.chan, TRUE);

  /* set non-blocking ;) */
  {
    GIOFlags flags;
    GError *gerr = NULL;
    
    flags = g_io_channel_get_flags(cvs->hookserv.chan);
    g_io_channel_set_flags(cvs->hookserv.chan, flags | G_IO_FLAG_NONBLOCK,
			   &gerr);
    if (gerr != NULL) {
      g_io_channel_unref(cvs->hookserv.chan);
      g_error_free(gerr);
      g_timeout_add_seconds(1, (GSourceFunc) try_to_connect, cvs);
      return FALSE;
    }
  }

  /* this is fun, async io watcher */
  cvs->hookserv.hhsi.line = 0;
  cvs->hookserv.hhsi.command_args = NULL;
  cvs->hookserv.hhsi.command_name = NULL;
  cvs->hookserv.event_source = 
    g_io_add_watch_full(cvs->hookserv.chan, G_PRIORITY_DEFAULT,
			G_IO_IN | G_IO_ERR | G_IO_HUP,
			(GIOFunc) handle_hook_server_input, cvs,
			(GDestroyNotify) watch_killer);
  
  return FALSE;
}

/* should only be called in glib main loop */
/* returns a gboolean because it is a GSourceFunc */
gboolean nautilus_dropbox_hooks_force_reconnect(NautilusDropbox *cvs) {
  if (cvs->hookserv.connected == FALSE) {
    return FALSE;
  }

  debug("forcing hook to reconnect");

  g_assert(cvs->hookserv.event_source >= 0);
  
  if (cvs->hookserv.event_source > 0) {
    g_source_remove(cvs->hookserv.event_source);
  }
  else if (cvs->hookserv.event_source == 0) {
    debug("event source was zero!!!!!");
  }
  else {
    g_assert_not_reached();
  }
	 
  return FALSE;
}

  /* can be called from any thread*/
void
nautilus_dropbox_hooks_wait_until_connected(NautilusDropbox *cvs, gboolean val) {
  /* now we have to wait until the hook client gets connected */
  g_mutex_lock(cvs->hookserv.connected_mutex);
  while (cvs->hookserv.connected == !val) {
    g_cond_wait (cvs->hookserv.connected_cond,
		 cvs->hookserv.connected_mutex);
  }
  g_mutex_unlock(cvs->hookserv.connected_mutex);
}

void
nautilus_dropbox_hooks_setup(NautilusDropbox *cvs) {
  cvs->hookserv.connected_mutex = g_mutex_new();
  cvs->hookserv.connected_cond = g_cond_new();
  cvs->hookserv.connected = FALSE;

  /* register some hooks, other modules are free
     to register their own hooks */
  g_hash_table_insert(cvs->dispatch_table, "shell_touch",
		      (DropboxUpdateHook) handle_shell_touch);
  g_hash_table_insert(cvs->dispatch_table, "copy_to_clipboard",
		      (DropboxUpdateHook) handle_copy_to_clipboard);
  g_hash_table_insert(cvs->dispatch_table, "launch_folder",
		      (DropboxUpdateHook) handle_launch_folder);
  g_hash_table_insert(cvs->dispatch_table, "launch_url",
		      (DropboxUpdateHook) handle_launch_url);
}

void
nautilus_dropbox_hooks_start(NautilusDropbox *cvs) {
  try_to_connect(cvs);
}
