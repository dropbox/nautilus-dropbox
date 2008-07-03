/*
 * nautilus-dropbox-command.c
 * Implements connection handling and C interface for the Dropbox command socket.
 *
 * This file is part of nautilus-dropbox.
 *
 * nautilus-dropbox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nautilus-dropbox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with nautilus-dropbox.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>

#include <string.h>

#include <glib.h>

#include "g-util.h"
#include "nautilus-dropbox-common.h"
#include "nautilus-dropbox.h"
#include "nautilus-dropbox-command.h"
#include "nautilus-dropbox-tray.h"
#include "nautilus-dropbox-hooks.h"

/* this is a tiny hack, necessitated by the fact that
   finish_file info command is in nautilus_dropbox,
   this can be cleaned up once the file_info_command isn't a special
   case anylonger
*/
gboolean nautilus_dropbox_finish_file_info_command(DropboxFileInfoCommandResponse *);

/* i put these up here so they can be modified simply,
   in the future might want other modules to register their
   own on_connect and on_disconnect hooks,
   **they return a boolean because they are called by the glib main loop**
*/

typedef struct {
  NautilusDropbox *cvs;
  guint connect_attempt;
} ConnectionAttempt;

static gchar chars_not_to_escape[256];

static gboolean
on_connect(NautilusDropbox *cvs) {
  debug("command client connection");

  cvs->ca.user_quit = FALSE;
  cvs->ca.dropbox_starting = FALSE;

  nautilus_dropbox_on_connect(cvs);
  nautilus_dropbox_tray_on_connect(cvs);

  return FALSE;
}

static gboolean
on_disconnect(NautilusDropbox *cvs) {
  debug("command client disconnected");

  nautilus_dropbox_on_disconnect(cvs);
  nautilus_dropbox_tray_on_disconnect(cvs);

  return FALSE;
}

static gboolean
connection_attempt(ConnectionAttempt *ca) {
  NautilusDropbox *cvs = ca->cvs;
  guint i = ca->connect_attempt;

  g_free(ca);

  /* if the user hasn't quit us and this is the third time trying to 
     reconnect, then dropbox is probably dead, we should start it
   */
  if (cvs->ca.user_quit == FALSE &&
      i > 3 &&
      cvs->ca.dropbox_starting == FALSE) {
    cvs->ca.dropbox_starting = TRUE;
    debug("couldn't connect to dropbox, auto-starting");
    nautilus_dropbox_common_start_dropbox(cvs, TRUE);
  }

  return FALSE;
}

gboolean
nautilus_dropbox_command_is_connected(NautilusDropbox *cvs) {
  gboolean command_connected;

  g_mutex_lock(cvs->command_connected_mutex);
  command_connected = cvs->command_connected;
  g_mutex_unlock(cvs->command_connected_mutex);

  return command_connected;
}

gchar *nautilus_dropbox_command_sanitize(const gchar *a) {
  /* this function escapes teh following utf-8 characters:
   * '\\', '\n', '\t'
   */
  return g_strescape(a, chars_not_to_escape);
}

gchar *nautilus_dropbox_command_desanitize(const gchar *a) {
  return g_strcompress(a);
}

gboolean
nautilus_dropbox_command_parse_arg(const gchar *line, GHashTable *return_table) {
  gchar **argval;
  guint len;
  gboolean retval;
  
  argval = g_strsplit(line, "\t", 0);
  len = g_strv_length(argval);

  /*  debug("parsed: (%d) %s", len, line); */
  
  if (len > 1) {
    int i;
    gchar **vals;
    
    vals = g_new(gchar *, len);
    vals[len - 1] = NULL;
    
    for (i = 1; argval[i] != NULL; i++) {
      vals[i-1] = nautilus_dropbox_command_desanitize(argval[i]);
      
    }
    
    g_hash_table_insert(return_table,
			nautilus_dropbox_command_desanitize(argval[0]),
			vals);
    retval = TRUE;
  }
  else {
    retval = FALSE;
  }

  g_strfreev(argval);
  return retval;
}

static gboolean
receive_args_until_done(GIOChannel *chan, GHashTable *return_table,
			GError **err) {
  GIOStatus iostat;
  GError *tmp_error = NULL;
  guint numargs = 0;

  while (1) {
    gchar *line;
    gsize term_pos;

    /* if we are getting too many args, connection could be malicious */
    if (numargs >= 20) {
      g_set_error(err,
		  g_quark_from_static_string("malicious connection"),
		  0, "malicious connection");
      return FALSE;
    }
    
    /* get the string */
    iostat = g_io_channel_read_line(chan, &line, NULL,
				    &term_pos, &tmp_error);
    if (iostat == G_IO_STATUS_ERROR || tmp_error != NULL) {
      g_free(line);
      g_propagate_error(err, tmp_error);
      return FALSE;
    }
    else if (iostat == G_IO_STATUS_EOF) {
      g_free(line);
      g_set_error(err,
		  g_quark_from_static_string("connection closed"),
		  0, "connection closed");
      return FALSE;
    }

    *(line+term_pos) = '\0';

    if (strcmp("done", line) == 0) {
      g_free(line);
      break;
    }
    else {
      gboolean parse_result;

      parse_result = nautilus_dropbox_command_parse_arg(line, return_table);
      g_free(line);

      if (FALSE == parse_result) {
	g_set_error(err,
		    g_quark_from_static_string("parse error"),
		    0, "parse error");
	return FALSE;
      }
    }
    
    numargs += 1;
  }

  return TRUE;
}

/*
  sends a command to the dropbox server
  returns an hash of the return values

  in theory, this should disconnection errors
  but it doesn't matter right now, any error is a sufficient
  condition to disconnect
*/
static GHashTable *
send_command_to_db(GIOChannel *chan, const gchar *command_name,
		   GHashTable *args, GError **err) {
  GError *tmp_error = NULL;
  GIOStatus iostat;
  gsize bytes_trans;
  gchar *line;

  g_assert(chan != NULL);
  g_assert(command_name != NULL);
  
#define WRITE_OR_DIE_SANI(s,l) {					\
    gchar *sani_s;							\
    sani_s = nautilus_dropbox_command_sanitize(s);						\
    iostat = g_io_channel_write_chars(chan, sani_s,l, &bytes_trans,	\
				      &tmp_error);			\
    g_free(sani_s);							\
    if (iostat == G_IO_STATUS_ERROR || tmp_error != NULL) {		\
      g_propagate_error(err, tmp_error);				\
      return NULL;							\
    }									\
  }
  
#define WRITE_OR_DIE(s,l) {						\
    iostat = g_io_channel_write_chars(chan, s,l, &bytes_trans,		\
				      &tmp_error);			\
    if (iostat == G_IO_STATUS_ERROR || tmp_error != NULL) {		\
      g_propagate_error(err, tmp_error);				\
      return NULL;							\
    }									\
  }
  
  /* send command to server */
  WRITE_OR_DIE_SANI(command_name, -1);
  WRITE_OR_DIE("\n", -1);

  if (args != NULL) {
    gchar *key;
    gchar **value;
    GHashTableIter iter;

    g_hash_table_iter_init(&iter, args);
    while (g_hash_table_iter_next(&iter, (gpointer) &key,
				  (gpointer) &value)) {
      int i;
      WRITE_OR_DIE_SANI(key, -1);
      for (i = 0; value[i] != NULL; i++) {
	WRITE_OR_DIE("\t", -1);
	WRITE_OR_DIE_SANI(value[i], -1);
      }
      WRITE_OR_DIE("\n", -1);
    }
  }

  WRITE_OR_DIE("done\n", -1);

#undef WRITE_OR_DIE
#undef WRITE_OR_DIE_SANI

  g_io_channel_flush(chan, &tmp_error);
  if (tmp_error != NULL) {
    g_propagate_error(err, tmp_error);
    return NULL;
  }

  /* now we have to read the data */
  iostat = g_io_channel_read_line(chan, &line, NULL,
				  NULL, &tmp_error);
  if (iostat == G_IO_STATUS_ERROR || tmp_error != NULL) {
    if (line != NULL) g_free(line);
    g_propagate_error(err, tmp_error);
    return NULL;
  }
  else if (iostat == G_IO_STATUS_EOF) {
    if (line != NULL) g_free(line);
    g_set_error(err,
		g_quark_from_static_string("dropbox command connection closed"),
		0,
		"dropbox command connection closed");
    return NULL;
  }

  /* if the response was okay */
  if (strncmp(line, "ok\n", 3) == 0) {
    GHashTable *return_table = 
      g_hash_table_new_full((GHashFunc) g_str_hash,
			    (GEqualFunc) g_str_equal,
			    (GDestroyNotify) g_free,
			    (GDestroyNotify) g_strfreev);
    
    g_free(line);

    receive_args_until_done(chan, return_table, &tmp_error);
    if (tmp_error != NULL) {
      g_hash_table_destroy(return_table);
      g_propagate_error(err, tmp_error);
      return NULL;
    }
      
    return return_table;
  }
  /* otherwise */
  else {
    g_free(line);

    /* read errors off until we get done */
    do {
      /* clear string */
      iostat = g_io_channel_read_line(chan, &line, NULL,
				      NULL, &tmp_error);
      if (iostat == G_IO_STATUS_ERROR ||
	  tmp_error != NULL) {
	g_free(line);
	g_propagate_error(err, tmp_error);
	return NULL;
      }
      else if (iostat == G_IO_STATUS_EOF) {
	g_free(line);
	g_set_error(err,
		    g_quark_from_static_string("dropbox command connection closed"),
		    0,
		    "dropbox command connection closed");
	return NULL;
      }

      /* we got our line */
    } while (strncmp(line, "done\n", 5) != 0);

    g_free(line);
    return NULL;
  }
}

static void
do_file_info_command(GIOChannel *chan, DropboxFileInfoCommand *dfic,
		     GError **gerr) {
  /* we need to send two requests to dropbox:
     file status, and context options */
  GError *tmp_gerr = NULL;
  DropboxFileInfoCommandResponse *dficr;
  GHashTable *file_status_response, *context_options_response, *args;
  gchar *filename;

  /* TODO: might be thread-unsafe */
  filename = g_filename_from_uri(nautilus_file_info_get_uri(dfic->file),
				 NULL, NULL);

  args = g_hash_table_new_full((GHashFunc) g_str_hash,
			       (GEqualFunc) g_str_equal,
			       (GDestroyNotify) g_free,
			       (GDestroyNotify) g_strfreev);
  {
    gchar **path_arg;
    path_arg = g_new(gchar *, 2);
    path_arg[0] = g_strdup(filename);
    path_arg[1] = NULL;
    g_hash_table_insert(args, g_strdup("path"), path_arg);
  }

  
  /* send status command to server */
  file_status_response = send_command_to_db(chan, "icon_overlay_file_status",
					    args, &tmp_gerr);
  g_hash_table_unref(args);
  args = NULL;
  if (tmp_gerr != NULL) {
    g_free(filename);
    g_assert(file_status_response == NULL);
    g_propagate_error(gerr, tmp_gerr);
    return;
  }

  args = g_hash_table_new_full((GHashFunc) g_str_hash,
			       (GEqualFunc) g_str_equal,
			       (GDestroyNotify) g_free,
			       (GDestroyNotify) g_strfreev);
  {
    gchar **paths_arg;
    paths_arg = g_new(gchar *, 2);
    paths_arg[0] = g_strdup(filename);
    paths_arg[1] = NULL;
    g_hash_table_insert(args, g_strdup("paths"), paths_arg);
  }

  g_free(filename);

  /* send context options command to server */
  context_options_response =
    send_command_to_db(chan, "icon_overlay_context_options",
		       args, &tmp_gerr);
  g_hash_table_unref(args);
  args = NULL;
  if (tmp_gerr != NULL) {
    if (file_status_response != NULL)
      g_hash_table_destroy(file_status_response);
    g_assert(context_options_response == NULL);
    g_propagate_error(gerr, tmp_gerr);
    return;
  }
  
  /* great server responded perfectly,
     now let's get this request done */
  dficr = g_new0(DropboxFileInfoCommandResponse, 1);
  dficr->dfic = dfic;
  dficr->file_status_response = file_status_response;
  dficr->context_options_response = context_options_response;
  g_idle_add((GSourceFunc) nautilus_dropbox_finish_file_info_command, dficr);
  
  return;
}

static gboolean
finish_general_command(DropboxGeneralCommandResponse *dgcr) {
  if (dgcr->dgc->handler != NULL) {
    dgcr->dgc->handler(dgcr->response, dgcr->dgc->handler_ud);
  }
  
  if (dgcr->response != NULL) {
    g_hash_table_destroy(dgcr->response);
  }

  g_free(dgcr->dgc->command_name);
  if (dgcr->dgc->command_args != NULL) {
    g_hash_table_unref(dgcr->dgc->command_args);
  }
  g_free(dgcr->dgc);
  g_free(dgcr);
  
  return FALSE;
}

static void
do_general_command(GIOChannel *chan, DropboxGeneralCommand *dcac,
		   GError **gerr) {
  GError *tmp_gerr = NULL;
  GHashTable *response;

  /* send status command to server */
  response = send_command_to_db(chan, dcac->command_name,
				dcac->command_args, &tmp_gerr);
  if (tmp_gerr != NULL) {
    g_assert(response == NULL);
    g_propagate_error(gerr, tmp_gerr);
    return;
  }

  /* debug general responses */
  if (FALSE) {
    GHashTableIter iter;
    gchar *key, *val;
    debug("general command: %s", dcac->command_name);

    g_hash_table_iter_init(&iter, dcac->command_args);
    while (g_hash_table_iter_next(&iter,
				  (gpointer) &key,
				  (gpointer) &val)) {
      debug("arg: %s, val: %s", key, val);
    }


    if (response != NULL) {
      g_hash_table_iter_init(&iter, response);
      while (g_hash_table_iter_next(&iter,
				    (gpointer) &key,
				    (gpointer) &val)) {
	debug("response: %s, val: %s", key, val);
      }
    }
    else {
      debug("response was null");
    }
  }
  
  /* great, the server did the command perfectly,
     now call the handler with the response */
  {
    DropboxGeneralCommandResponse *dgcr = g_new0(DropboxGeneralCommandResponse, 1);
    dgcr->dgc = dcac;
    dgcr->response = response;
    g_idle_add((GSourceFunc) finish_general_command, dgcr);
  }
  
  return;
}

static gboolean
check_connection(GIOChannel *chan) {
  gchar fake_buf[4096];
  gsize bytes_read;
  GError *tmp_error = NULL;
  GIOFlags flags;
  GIOStatus iostat;

  flags = g_io_channel_get_flags(chan);
  
  /* set non-blocking */
  g_io_channel_set_flags(chan, flags | G_IO_FLAG_NONBLOCK,
			 &tmp_error);
  if (tmp_error != NULL) {
    g_error_free(tmp_error);
    return FALSE;
  }
  
  iostat = g_io_channel_read_chars(chan, fake_buf,
				   sizeof(fake_buf), 
				   &bytes_read, &tmp_error);
  g_io_channel_set_flags(chan, flags, NULL);
  if (tmp_error != NULL) {
    g_error_free(tmp_error);
    return FALSE;
  }

  /* this makes us disconnect from bad servers
     (those that send us information without us asking for it) */
  if (iostat == G_IO_STATUS_AGAIN && bytes_read == 0) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}

static gpointer
nautilus_dropbox_command_thread(gpointer data);

static void
end_request(DropboxCommand *dc) {
  if ((gpointer (*)(gpointer data)) dc != &nautilus_dropbox_command_thread) {
    switch (dc->request_type) {
    case GET_FILE_INFO: {
      DropboxFileInfoCommand *dfic = (DropboxFileInfoCommand *) dc;
      DropboxFileInfoCommandResponse *dficr = g_new0(DropboxFileInfoCommandResponse, 1);
      dficr->dfic = dfic;
      dficr->file_status_response = NULL;
      dficr->context_options_response = NULL;
      g_idle_add((GSourceFunc) nautilus_dropbox_finish_file_info_command, dficr);
    }
      break;
    case GENERAL_COMMAND: {
      DropboxGeneralCommand *dgc = (DropboxGeneralCommand *) dc;
      DropboxGeneralCommandResponse *dgcr = g_new0(DropboxGeneralCommandResponse, 1);
      dgcr->dgc = dgc;
      dgcr->response = NULL;
      g_idle_add((GSourceFunc) finish_general_command, dgcr);
    }
      break;
    default: 
      g_assert(FALSE);
      break;
    }
  }
}

static gpointer
nautilus_dropbox_command_thread(gpointer data) {
  NautilusDropbox *cvs;
  struct sockaddr_un addr;
  socklen_t addr_len;

  cvs = NAUTILUS_DROPBOX(data);

  /* intialize address structure */
  addr.sun_family = AF_UNIX;
  g_snprintf(addr.sun_path,
	     sizeof(addr.sun_path),
	     "%s/.dropbox/command_socket",
	     g_get_home_dir());
  addr_len = sizeof(addr) - sizeof(addr.sun_path) + strlen(addr.sun_path);

  while (1) {
    GIOChannel *chan = NULL;
    GError *gerr = NULL;
    int sock;
    int i;

    sock = socket(PF_UNIX, SOCK_STREAM, 0);


    for (i = 1; ;i++) {
      /* first we have to connect to the dropbox command server */
      if (-1 == connect(sock, (struct sockaddr *) &addr, addr_len)) {
	ConnectionAttempt *ca = g_new(ConnectionAttempt, 1);
	ca->cvs = cvs;
	ca->connect_attempt = i;
	g_idle_add((GSourceFunc) connection_attempt, ca);
	g_usleep(G_USEC_PER_SEC);
      }
      else break;
    }

    /* connected */
    chan = g_io_channel_unix_new(sock);
    g_io_channel_set_close_on_unref(chan, TRUE);
    g_io_channel_set_line_term(chan, "\n", -1);

    nautilus_dropbox_hooks_wait_until_connected(cvs, TRUE);
    
#define SET_CONNECTED_STATE(s)     {			\
      g_mutex_lock(cvs->command_connected_mutex);	\
      cvs->command_connected = s;			\
      g_mutex_unlock(cvs->command_connected_mutex);	\
    }
    
    SET_CONNECTED_STATE(TRUE);

    g_idle_add((GSourceFunc) on_connect, cvs);

    while (1) {
      DropboxCommand *dc;
      
      while (1) {
	GTimeVal gtv;

	g_get_current_time(&gtv);
	g_time_val_add(&gtv, G_USEC_PER_SEC / 10);
	/* get a request from nautilus */
	dc = g_async_queue_timed_pop(cvs->command_queue, &gtv);
	if (dc != NULL) {
	  break;
	}
	else {
	  if (check_connection(chan) == FALSE) {
	    goto BADCONNECTION;
	  }
	}
      }

      /* why this insures lexical module safety should be obvious,
	 (this function is static)
       */
      if ((gpointer (*)(gpointer data)) dc == &nautilus_dropbox_command_thread) {
	debug("got a reset request");
	goto BADCONNECTION;
      }

      switch (dc->request_type) {
      case GET_FILE_INFO: {
	do_file_info_command(chan, (DropboxFileInfoCommand *) dc, &gerr);
      }
	break;
      case GENERAL_COMMAND: {
	do_general_command(chan, (DropboxGeneralCommand *) dc, &gerr);
      }
	break;
      default: 
	g_assert(FALSE);
	break;
      }
      
      if (gerr != NULL) {
	/* mark this request as never to be completed */
	end_request(dc);
	
	debug("command error: %s", gerr->message);
	
	g_error_free(gerr);
      BADCONNECTION:
	/* grab all the rest of the data off the async queue and mark it
	   never to be completed, who knows how long we'll be disconnected */
	while ((dc = g_async_queue_try_pop(cvs->command_queue)) != NULL) {
	  end_request(dc);
	}

	g_io_channel_unref(chan);

	SET_CONNECTED_STATE(FALSE);

	/* and force the hook connection to restart */
	g_idle_add((GSourceFunc) nautilus_dropbox_hooks_force_reconnect, cvs);
	/* call the disconnect handler */
	g_idle_add((GSourceFunc) on_disconnect, cvs);

	break;
      }
    }
    
#undef SET_CONNECTED_STATE
  }
  
  return NULL;
}

/* thread safe, i.e. can be called from any thread */
void nautilus_dropbox_command_force_reconnect(NautilusDropbox *cvs) {
  if (nautilus_dropbox_command_is_connected(cvs) == TRUE) {
    debug("forcing command to reconnect");
    nautilus_dropbox_command_request(cvs, (DropboxCommand *) &nautilus_dropbox_command_thread);
  }
}

/* can be called from any thread */
void
nautilus_dropbox_command_request(NautilusDropbox *cvs, DropboxCommand *dc) {
  g_async_queue_push(cvs->command_queue, dc);
}

/* should only be called once on initialization */
void
nautilus_dropbox_command_setup(NautilusDropbox *cvs) {
  cvs->command_queue = g_async_queue_new();

  cvs->command_connected_mutex = g_mutex_new();
  cvs->command_connected = FALSE;

  /* build list of characters to not escape */
  {
    guchar c;
    unsigned int i = 0;

    for (c = 0x01; c <= 0x1f; c++) {
      if (c != '\n' && c != '\t') {
	chars_not_to_escape[i++] = c;
      }
    }
    for (c = 0x7f; c != 0x0; c++) {
      chars_not_to_escape[i++] = c;
    }
    chars_not_to_escape[i++] = '\0';
  }
}

/* should only be called once on initialization */
void
nautilus_dropbox_command_start(NautilusDropbox *cvs) {
  /* setup the connect to the command server */
  g_thread_create(nautilus_dropbox_command_thread, cvs, FALSE, NULL);
}
