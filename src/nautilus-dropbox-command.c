/*
 *
 *  nautilus-dropbox-command.c
 * 
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>

#include <string.h>

#include <glib.h>

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

static gboolean
on_connect(NautilusDropbox *cvs) {
  nautilus_dropbox_on_connect(cvs);
  nautilus_dropbox_tray_on_connect(cvs);

  return FALSE;
}

static gboolean
on_disconnect(NautilusDropbox *cvs) {
  nautilus_dropbox_on_disconnect(cvs);
  nautilus_dropbox_tray_on_disconnect(cvs);

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

static void
receive_args_until_done(GIOChannel *chan, GHashTable *return_table,
			GError **err) {
  GIOStatus iostat;
  GError *tmp_error = NULL;
  GString *line;

  line = g_string_new("");

  while (1) {
    GString *key_str, *val_str;
    char *tab_loc;
    
    /* clear the string */
    g_string_erase(line, 0, -1);
    
    /* get the string */
    iostat = g_io_channel_read_line_string(chan, line,
					   NULL, &tmp_error);
    if (iostat == G_IO_STATUS_ERROR || tmp_error != NULL) {
      g_string_free(line, TRUE);
      g_propagate_error(err, tmp_error);
      return;
    }
    else if (iostat == G_IO_STATUS_EOF) {
      g_string_free(line, TRUE);
      g_set_error(err,
		  g_quark_from_static_string("connection closed"),
		  0, "connection closed");
      return;
    }

    /* TODO: shouldn't loop forever, i.e. looping based on input, this will kill us */
    if (strncmp(line->str, "done\n", 5) == 0) {
      break;
    }
    else {
      tab_loc = strchr(line->str, '\t');

      if (tab_loc != NULL) {
	key_str = g_string_new_len(line->str, tab_loc - line->str);
	val_str = g_string_new_len(tab_loc+1, line->len - (tab_loc - line->str) - 2);
	
	g_hash_table_insert(return_table, key_str, val_str);
      }
      else {
	g_string_free(line, TRUE);
	g_set_error(err,
		    g_quark_from_static_string("invalid connection"),
		    0, "invalid connection");
	return;
      }
    }
  }

  g_string_free(line, TRUE);
  return;
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
  GString *line;

  g_assert(chan != NULL);
  g_assert(command_name != NULL);

#define WRITE_OR_DIE(s,l) {					\
    iostat = g_io_channel_write_chars(chan, s,l, &bytes_trans,	\
				      &tmp_error);		\
    if (iostat == G_IO_STATUS_ERROR || tmp_error != NULL) {	\
      g_propagate_error(err, tmp_error);                        \
      return NULL;                                              \
    }								\
  }
    
  /* send command to server */
  /* TODO: escape tabs and newlines */
  WRITE_OR_DIE(command_name, -1);
  WRITE_OR_DIE("\n", -1);

  if (args != NULL) {
    GString *key, *value;
    GHashTableIter iter;

    g_hash_table_iter_init(&iter, args);
    while (g_hash_table_iter_next(&iter, (gpointer) &key,
				  (gpointer) &value)) {
      WRITE_OR_DIE(key->str, key->len);
      WRITE_OR_DIE("\t", -1);
      WRITE_OR_DIE(value->str, value->len);
      WRITE_OR_DIE("\n", -1);
    }
  }

  WRITE_OR_DIE("done\n", -1);

#undef WRITE_OR_DIE

  iostat = g_io_channel_flush(chan, &tmp_error);
  if (iostat == G_IO_STATUS_ERROR || tmp_error != NULL) {
    g_propagate_error(err, tmp_error);
    return NULL;
  }

  /* now we have to read the data */
  /* TODO: unescape tabs and newlines */
  line = g_string_new("");
  iostat = g_io_channel_read_line_string(chan, line,
					 NULL, &tmp_error);
  if (iostat == G_IO_STATUS_ERROR || tmp_error != NULL) {
    g_string_free(line, TRUE);
    g_propagate_error(err, tmp_error);
    return NULL;
  }
  else if (iostat == G_IO_STATUS_EOF) {
    g_string_free(line, TRUE);
    g_set_error(err,
		g_quark_from_static_string("dropbox command connection closed"),
		0,
		"dropbox command connection closed");
    return NULL;
  }

  /* if the response was okay */
  if (strncmp(line->str, "ok\n", 3) == 0) {
    GHashTable *return_table = 
      g_hash_table_new_full((GHashFunc) g_string_hash, (GEqualFunc) g_string_equal,
			    nautilus_dropbox_common_destroy_string,
			    nautilus_dropbox_common_destroy_string);
        
    g_string_free(line, TRUE);

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
    /* read errors off until we get done */
    do {
      /* clear string */
      g_string_erase(line, 0, -1);

      iostat = g_io_channel_read_line_string(chan, line,
					     NULL, &tmp_error);
      if (iostat == G_IO_STATUS_ERROR ||
	  tmp_error != NULL) {
	g_string_free(line, TRUE);
	g_propagate_error(err, tmp_error);
	return NULL;
      }
      else if (iostat == G_IO_STATUS_EOF) {
	g_string_free(line, TRUE);
	g_set_error(err,
		    g_quark_from_static_string("dropbox command connection closed"),
		    0,
		    "dropbox command connection closed");
	return NULL;
      }

      /* we got our line */
    } while (strncmp(line->str, "done", 4) != 0);

    g_string_free(line, TRUE);
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

  args = g_hash_table_new_full((GHashFunc) g_string_hash,
			       (GEqualFunc) g_string_equal,
			       nautilus_dropbox_common_destroy_string,
			       nautilus_dropbox_common_destroy_string);
  {
    GString *key, *value;
    key = g_string_new("path");
    value = g_string_new(filename);

    g_hash_table_insert(args, key, value);
  }
  
  /* send status command to server */
  file_status_response = send_command_to_db(chan, "icon_overlay_file_status",
					    args, &tmp_gerr);
  g_hash_table_destroy(args);
  if (tmp_gerr != NULL) {
    g_assert(file_status_response == NULL);
    g_propagate_error(gerr, tmp_gerr);
    return;
  }

  args = g_hash_table_new_full((GHashFunc) g_string_hash,
			       (GEqualFunc) g_string_equal,
			       nautilus_dropbox_common_destroy_string,
			       nautilus_dropbox_common_destroy_string);

  {
    GString *key, *value;
    key = g_string_new("paths");
    value = g_string_new(filename);
    g_hash_table_insert(args, key, value);
  }
  
  /* send context options command to server */
  context_options_response =
    send_command_to_db(chan, "icon_overlay_context_options",
		       args, &tmp_gerr);
  g_hash_table_destroy(args);
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
    g_hash_table_destroy(dgcr->dgc->command_args);
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
    GString *key, *val;
    debug("general command: %s", dcac->command_name);

    g_hash_table_iter_init(&iter, dcac->command_args);
    while (g_hash_table_iter_next(&iter,
				  (gpointer) &key,
				  (gpointer) &val)) {
      debug("arg: %s, val: %s", key->str, val->str);
    }


    if (response != NULL) {
      g_hash_table_iter_init(&iter, response);
      while (g_hash_table_iter_next(&iter,
				    (gpointer) &key,
				    (gpointer) &val)) {
	debug("response: %s, val: %s", key->str, val->str);
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

    sock = socket(PF_UNIX, SOCK_STREAM, 0);

    do {
      /* first we have to connect to the dropbox command server */
      if (-1 == connect(sock, (struct sockaddr *) &addr, addr_len))
	g_usleep(G_USEC_PER_SEC);
      else
	break;
    } while (1);

    /* connected */
    chan = g_io_channel_unix_new(sock);
    g_io_channel_set_close_on_unref(chan, TRUE);
    g_io_channel_set_line_term(chan, "\n", -1);

    nautilus_dropbox_hooks_wait_until_connected(cvs, TRUE);

    /* send init message */
    /* TODO: maybe this should be in the on_connect handler? */
    {
      GHashTable *response;
      response = send_command_to_db(chan, "icon_overlay_init",
				    NULL, &gerr);
      if (gerr != NULL) {
	/* sending command failed, destroy channel */
	debug("sending init command failed");
	
	g_error_free(gerr);
	g_io_channel_unref(chan);
	continue;
      }
      
      /* TODO: do we care if dropbox failed to init? */
      if (response != NULL) {
	g_hash_table_destroy(response);
      }
    }

    g_mutex_lock(cvs->command_connected_mutex);
    cvs->command_connected = TRUE;
    g_mutex_unlock(cvs->command_connected_mutex);

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
	/* if this request couldn't complete */
	g_async_queue_push(cvs->command_queue, dc);
	g_error_free(gerr);
      BADCONNECTION:
	g_io_channel_unref(chan);

	g_mutex_lock(cvs->command_connected_mutex);
	cvs->command_connected = FALSE;
	g_mutex_unlock(cvs->command_connected_mutex);
	
	/* call the disconnect handler */
	g_idle_add((GSourceFunc) on_disconnect, cvs);

	break;
      }
    }
  }
  
  return NULL;
}

void
nautilus_dropbox_command_request(NautilusDropbox *cvs, DropboxCommand *dc) {
  g_async_queue_push(cvs->command_queue, dc);
}

void
nautilus_dropbox_command_setup(NautilusDropbox *cvs) {
  cvs->command_queue = g_async_queue_new();

  cvs->command_connected_mutex = g_mutex_new();
  cvs->command_connected = FALSE;
}

void
nautilus_dropbox_command_start(NautilusDropbox *cvs) {
  /* setup the connect to the command server */
  g_thread_create(nautilus_dropbox_command_thread, cvs, FALSE, NULL);
}
