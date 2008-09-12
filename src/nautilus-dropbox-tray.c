/*
 * Copyright 2008 Evenflow, Inc.
 *
 * nautilus-dropbox-tray.c
 * Tray icon manager code for nautilus-dropbox
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib-object.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <libnotify/notify.h>

#include "g-util.h"
#include "dropbox-command-client.h"
#include "nautilus-dropbox-hooks.h"
#include "dropbox-client.h"
#include "nautilus-dropbox-common.h"
#include "async-http-downloader.h"

#include "nautilus-dropbox-tray.h"

typedef struct {
  NautilusDropboxTray *ndt;
  gchar *command;
} MenuItemData;

static void
nautilus_dropbox_tray_start_dropbox_transfer(NautilusDropboxTray *ndt);

static void
launch_folder(NautilusDropboxTray *ndt,
		              const gchar *folder_path) {
  gchar *command_line, *escaped_string;
  gchar *msg;
  
  escaped_string = g_strescape(folder_path, NULL);
  command_line = g_strdup_printf("nautilus \"%s\"", escaped_string);
  msg = g_strdup_printf("Couldn't start '%s'. Is nautilus in your PATH?",
			command_line);
  
  nautilus_dropbox_common_launch_command_with_error(ndt, command_line,
						    "Couldn't open folder", msg);
  
  g_free(msg);
  g_free(escaped_string);
  g_free(command_line);
}

static void
menu_refresh(NautilusDropboxTray *ndt) {
  gboolean visible;

  /* first see if the menu is popped up and visible */
  g_object_get(G_OBJECT(ndt->context_menu), "visible", &visible, NULL);

  if (visible) {
    gtk_widget_show_all(GTK_WIDGET(ndt->context_menu));
  }
}

static void
gtk_container_remove_all(GtkContainer *c) {
  GList *li;
  
  for (li = gtk_container_get_children(c); li != NULL;
       li = g_list_next(li)) {
    gtk_container_remove(c, li->data);
  }
}

static void
activate_start_dropbox(GtkMenuItem *mi,
		       NautilusDropboxTray *ndt) {
  if (!nautilus_dropbox_common_start_dropbox()) {
    nautilus_dropbox_tray_start_dropbox_transfer(ndt);
  }
}

static void
install_start_dropbox_menu(NautilusDropboxTray *ndt) {
  gtk_status_icon_set_tooltip(ndt->status_icon, "Dropbox");

  /* install start dropbox menu */
  gtk_container_remove_all(GTK_CONTAINER(ndt->context_menu));
  
  {
    GtkWidget *item;
    item = gtk_menu_item_new_with_label("Start Dropbox");
    gtk_menu_shell_append(GTK_MENU_SHELL(ndt->context_menu), item);
    g_signal_connect(G_OBJECT(item), "activate",
		     G_CALLBACK(activate_start_dropbox), ndt);
  }
  
  menu_refresh(ndt);
}

void menu_item_data_destroy(MenuItemData *mid,
			    GClosure *closure) {
  g_free(mid->command);
  g_free(mid);
}

void notify_closed_cb(NotifyNotification *nn, gpointer ud) {
  g_object_unref(G_OBJECT(nn));
}

void notifycb(NotifyNotification *nn, gchar *label, gpointer *mud) {
  if (mud[0] != NULL) {
    ((DropboxTrayBubbleActionCB)mud[0])(mud[1]);
  }
}

void notifyfreefunc(gpointer *mud) {
  if (mud[2] != NULL) {
    ((GFreeFunc)mud[2])(mud[1]);
  }
  g_free(mud);
}

/**
 * nautilus_dropbox_tray_bubble
 * @ndt: DropboxTray structure
 * @caption: caption for the bubble
 * @message: message for the bubble
 * @cb: optional callback when the bubble is clicked
 * @cb_desc: optional description of the what the callback does
 * @ud: optional user data for the callback
 * @free_func: optional function to call when the bubble is destroyed
 * @gerr: optional pointer to set an error
 * 
 * Bubbles a message above the Dropbox tray icon. Returns false and sets
 * gerr if an error occured.
 */
gboolean
nautilus_dropbox_tray_bubble(NautilusDropboxTray *ndt,
			     const gchar *caption,
			     const gchar *message,
			     DropboxTrayBubbleActionCB cb,
			     const gchar *cb_desc,
			     gpointer ud,
			     GFreeFunc free_func,
			     GError **gerr) {
  gboolean toret;
  NotifyNotification *n;
  gpointer *mud;

  /* can't do this if libnotify couldn't initialize */
  if (!ndt->notify_inited) {
    return TRUE;
  }
  
#if GTK_CHECK_VERSION(2, 9, 2)
  n = notify_notification_new_with_status_icon(caption, message,
					       NULL, ndt->status_icon);
#else
  n = notify_notification_new(caption, message,
			      NULL, ndt->status_icon);
  {
      GdkRectangle area;
      gtk_status_icon_get_geometry(ndt->status_icon,
				   NULL, &area, NULL);
      
      notify_notification_set_hint_int32 (n, "x", area.x+area.width/2);
      notify_notification_set_hint_int32 (n, "y", area.y+area.height - 5);
  }
#endif

  mud = g_new(gpointer, 3);
  mud[0] = cb;
  mud[1] = ud;
  mud[2] = free_func;
  notify_notification_add_action(n, "default", cb_desc == NULL ? "default" : cb_desc,
				 (NotifyActionCallback) notifycb, mud,
				 (GFreeFunc) notifyfreefunc);

  g_signal_connect(n, "closed", G_CALLBACK(notify_closed_cb), NULL);

  if ((toret = notify_notification_show(n, gerr)) == FALSE) {
    debug("couldn't show notification: %s", (*gerr)->message);
    g_free(mud);
  }

  return toret;
}

void bubble_clicked_cb(gpointer *ud) {
  debug_enter();
  
  if (!g_file_test((const gchar *) ud[1], G_FILE_TEST_EXISTS)) {
    return;
  }
   
  if (g_file_test((const gchar *) ud[1], G_FILE_TEST_IS_DIR)) {
    launch_folder((NautilusDropboxTray *) ud[0],
		  (const gchar *) ud[1]);
  }
  else {
    gchar *base;
    base = g_path_get_dirname(ud[1]);
    launch_folder((NautilusDropboxTray *) ud[0],
		  (const gchar *) base);
    g_free(base);
  }
  
}

void bubble_clicked_callback_cb(gpointer *ud) {
  debug_enter();
  
  dropbox_command_client_send_simple_command(&(((NautilusDropboxTray *) ud[0])->dc->dcc),
					     ud[1]);
}
void bubble_clicked_free(gpointer *ud) {
  g_free(ud[1]);
  g_free(ud);
}

static void
handle_bubble(GHashTable *args, NautilusDropboxTray *ndt) {
  gchar **message, **caption;
    
  message = g_hash_table_lookup(args, "message");
  caption = g_hash_table_lookup(args, "caption");
  
  if (message != NULL && caption != NULL) {
    gchar **location, **callback;
    
    location = g_hash_table_lookup(args, "location");
    callback = g_hash_table_lookup(args, "callback");
    if (location != NULL) {
      gpointer *ud;
      ud = g_new(gpointer, 2);
      ud[0] = ndt;
      ud[1] = g_strdup(location[0]);
      nautilus_dropbox_tray_bubble(ndt, caption[0], message[0],
				   (DropboxTrayBubbleActionCB) bubble_clicked_cb,
				   "Show Changed Files",
				   ud,
				   (GFreeFunc) bubble_clicked_free, NULL);     
    }
    else if (callback != NULL) {
      gpointer *ud;
      ud = g_new(gpointer, 2);
      ud[0] = ndt;
      ud[1] = g_strdup(callback[0]);
      nautilus_dropbox_tray_bubble(ndt, caption[0], message[0],
				   (DropboxTrayBubbleActionCB) bubble_clicked_callback_cb,
				   NULL, ud,
				   (GFreeFunc) bubble_clicked_free, NULL);     
    }
    else {
      nautilus_dropbox_tray_bubble(ndt, caption[0], message[0],
				   NULL,NULL,NULL,NULL,NULL);     
    }
  }
}

static void
handle_highlight_file(GHashTable *args, NautilusDropboxTray *ndt) {
  gchar **path;

  if ((path = g_hash_table_lookup(args, "path")) != NULL) {
    /* need to get dirname */
    gchar *dir;
    dir = g_path_get_dirname(path[0]);
    launch_folder(ndt, dir);
    g_free(dir);
  }
}

static void
handle_launch_folder(GHashTable *args, NautilusDropboxTray *ndt) {
  gchar **path;

  if ((path = g_hash_table_lookup(args, "path")) != NULL) {
    launch_folder(ndt, path[0]);
  }
}

static void 
popup(GtkStatusIcon *status_icon,
      guint button,
      guint activate_time,
      NautilusDropboxTray *ndt) {

  gtk_widget_show_all(GTK_WIDGET(ndt->context_menu));

  gtk_menu_popup(GTK_MENU(ndt->context_menu),
		 NULL,
		 NULL,
		 gtk_status_icon_position_menu,
		 ndt->status_icon,
		 button,
		 activate_time);
}

typedef struct {
  NautilusDropboxTray *ndt;
  GtkLabel *percent_done_label;
  gint filesize;
  GIOChannel *tmpfilechan;
  gchar *tmpfilename;
  gint bytes_downloaded;
  guint ev_id;
  gboolean user_cancelled;
  gboolean download_finished;
} HttpDownloadCtx;

static void
fail_dropbox_download(NautilusDropboxTray *ndt, const gchar *msg) {
  install_start_dropbox_menu(ndt);

  nautilus_dropbox_tray_bubble(ndt, "Couldn't download Dropbox",
			       msg == NULL
			       ? "Failed to download Dropbox, Are you connected "
			       " to the internet? Are your proxy settings correct?"
			       : msg, NULL, NULL, NULL, NULL, NULL);
}

static void
handle_tar_dying(GPid pid, gint status, gpointer *ud) {
  if (status == 0) {
    g_unlink(ud[0]);
    nautilus_dropbox_common_start_dropbox();
  }
  else {
    gchar *msg;
    msg = g_strdup_printf("The Dropbox archive located at \"%s\" failed to unpack.",
			  (gchar *) ud[0]);
    nautilus_dropbox_tray_bubble(&(((NautilusDropbox *) ud[1])->ndt),
				 "Couldn't download Dropbox.",
				 msg, NULL, NULL, NULL, NULL, NULL);
    g_free(msg);
  }

  /* delete tmp file */
  g_spawn_close_pid(pid);
  g_free(ud[0]);
  g_free(ud);
}

static gboolean
handle_incoming_http_data(GIOChannel *chan,
			  GIOCondition cond,
			  HttpDownloadCtx *ctx) {
  GIOStatus iostat;
  gchar buf[4096];
  gsize bytes_read;

  while ((iostat = g_io_channel_read_chars(chan, buf, 4096,
					   &bytes_read, NULL)) ==
	 G_IO_STATUS_NORMAL) {
    ctx->bytes_downloaded += bytes_read;
    
    /* TODO: this blocks, should put buffesr to write on a queue
       that gets read whenever ctx->tmpfilechan is ready to write,
       we should be okay for now since it shouldn't block except
       only in EXTREME circumstances */
    if (g_io_channel_write_chars(ctx->tmpfilechan, buf,
				 bytes_read, NULL, NULL) != G_IO_STATUS_NORMAL) {
      /* TODO: error condition, ignore for now */
      return FALSE;
    }
  }
  
  /* now update the gtk label */
  if (ctx->filesize != -1) {
    gchar *percent_done;
    
    percent_done =
      g_strdup_printf("Downloading Dropbox... %d%% Done",
		      ctx->bytes_downloaded * 100 / ctx->filesize);
    
    gtk_label_set_text(GTK_LABEL(ctx->percent_done_label), percent_done);
    g_free(percent_done);
  }
  else {
    switch (ctx->bytes_downloaded % 4) {
    case 0:
      gtk_label_set_text(GTK_LABEL(ctx->percent_done_label), "Downloading Dropbox");
      break;
    case 1:
      gtk_label_set_text(GTK_LABEL(ctx->percent_done_label), "Downloading Dropbox.");
      break;
    case 2:
      gtk_label_set_text(GTK_LABEL(ctx->percent_done_label), "Downloading Dropbox..");
      break;
    default:
      gtk_label_set_text(GTK_LABEL(ctx->percent_done_label), "Downloading Dropbox...");
      break;
    }
  }
  
  switch (iostat) {
  case G_IO_STATUS_EOF: {
    GPid pid;
    gchar **argv;
    /* completed download, untar the archive and run */
    ctx->download_finished = TRUE;
    
    argv = g_new(gchar *, 6);
    argv[0] = g_strdup("tar");
    argv[1] = g_strdup("-C");
    argv[2] = g_strdup(g_get_home_dir());
    argv[3] = g_strdup("-xzf");
    argv[4] = g_strdup(ctx->tmpfilename);
    argv[5] = NULL;
    
    g_spawn_async(NULL, argv, NULL,
		  G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
		  NULL, NULL, &pid, NULL);
    g_strfreev(argv);
    
    {
      gpointer *ud2;
      ud2 = g_new(gpointer, 2);
      ud2[0] = g_strdup(ctx->tmpfilename);
      ud2[1] = ctx->ndt;
      g_child_watch_add(pid, (GChildWatchFunc) handle_tar_dying, ud2);
    }
    
    return FALSE;
  }
    break;
  case G_IO_STATUS_ERROR: {
    /* just an error, return false to stop the download without setting download
       finished*/
    return FALSE;
  }
    break;
  case G_IO_STATUS_AGAIN:
    return TRUE;
    break;
  default:
    g_assert_not_reached();
    return FALSE;      
    break;
  }
}

static void
kill_hihd_ud(HttpDownloadCtx *ctx) {
  if (ctx->user_cancelled == TRUE) {
    install_start_dropbox_menu(ctx->ndt);
  }
  else if (ctx->download_finished == FALSE) {
    fail_dropbox_download(ctx->ndt, NULL);
  }

  g_io_channel_unref(ctx->tmpfilechan);
  g_free(ctx->tmpfilename);
  g_free(ctx);
}

static void
activate_cancel_download(GtkMenuItem *mi,
			 HttpDownloadCtx *ctx) {
  ctx->user_cancelled = TRUE;
  g_source_remove(ctx->ev_id);
}

static void
handle_dropbox_download_response(gint response_code,
				 GList *headers,
				 GIOChannel *chan,
				 HttpDownloadCtx *ctx) {
  int filesize = -1;

  switch (response_code) {
  case -1:
    fail_dropbox_download(ctx->ndt, NULL);
    g_free(ctx);
    return;
    break;
  case 300: case 301: case 302: case 303: case 304: case 305: case 306: case 307:
    {
      /* find the location header */
      GList *li;

      for (li = headers; li != NULL; li = g_list_next(li)) {

	if (g_ascii_strncasecmp((gchar *) li->data, "location:", 9) == 0) {
	  gchar *location;


	  location = g_strstrip(g_strdup((gchar *) li->data + sizeof("Location:")-1));

	  if (g_str_has_prefix(location, "http://")) {
	    gchar *host, *webpath;

	    host = location + sizeof("http://") - 1;
	    webpath = strchr(host, '/');
	    
	    host = g_strndup(host, webpath - host);
	    webpath = g_strdup(webpath);

	    printf("downloading dropbox from http://%s%s\n", host, webpath);

	    if (make_async_http_get_request(host, webpath,
					    NULL, (HttpResponseHandler)
					    handle_dropbox_download_response,
					    (gpointer) ctx)) {
	      /* made a successful request, stop this iteration */
	      break;
	    }
	  }
	}
      }

      if (li == NULL) {
	fail_dropbox_download(ctx->ndt, NULL);
	g_free(ctx);
      }

      return;
    }
    
    break;
  case 200:
    break;
  default: {
    gchar *msg;
    
    msg = g_strdup_printf("Couldn't download Dropbox. Server returned "
			  "%d.", response_code);
    fail_dropbox_download(ctx->ndt, msg);
    g_free(msg);
    g_free(ctx);
    return;
  }
    break;
  }

  /* find out the file size, -1 if unknown */
  {
    GList *li;
    for (li = headers; li != NULL; li = g_list_next(li)) {
      if (strncasecmp("content-length:", li->data, sizeof("content-length:")-1) == 0) {
	char *number_loc = strchr(li->data, ':');
	if (number_loc != NULL) {
	  filesize = atoi(number_loc + 1);
	  break;
	}
      }
    }
  }

  /* set channel to non blocking */
  {
    GIOFlags flags;
    flags = g_io_channel_get_flags(chan);
    if (g_io_channel_set_flags(chan, flags | G_IO_FLAG_NONBLOCK, NULL) !=
	G_IO_STATUS_NORMAL) {
      fail_dropbox_download(ctx->ndt, NULL);
      g_free(ctx);
      return;
    }
  }

  if (g_io_channel_set_encoding(chan, NULL, NULL) != G_IO_STATUS_NORMAL) {
    fail_dropbox_download(ctx->ndt, NULL);
    g_free(ctx);
    return;
  }

  ctx->filesize = filesize;
  {
    gint fd;
    gchar *filename;
    fd = g_file_open_tmp(NULL, &filename, NULL);
    if (fd == -1) {
      fail_dropbox_download(ctx->ndt, NULL);
      g_free(ctx);
      return;
    }

    /*debug("saving to %s", filename); */

    ctx->tmpfilechan = g_io_channel_unix_new(fd);
    g_io_channel_set_close_on_unref(ctx->tmpfilechan, TRUE);

    if (g_io_channel_set_encoding(ctx->tmpfilechan, NULL, NULL) != G_IO_STATUS_NORMAL) {
      fail_dropbox_download(ctx->ndt, NULL);
      g_io_channel_unref(ctx->tmpfilechan);
      g_free(filename);
      g_free(ctx);
      return;
    }

    ctx->tmpfilename = filename;
  }

  /*debug("installing http receiver callback"); */

  ctx->user_cancelled = FALSE;
  ctx->bytes_downloaded = 0;
  ctx->download_finished = FALSE;
  ctx->ev_id = g_io_add_watch_full(chan, G_PRIORITY_DEFAULT,
				   G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
				   (GIOFunc) handle_incoming_http_data,
				   ctx, (GDestroyNotify) kill_hihd_ud);

  /* great we got here, now set downloading menu */
  {
    /* setup the menu here */
    gtk_container_remove_all(GTK_CONTAINER(ctx->ndt->context_menu));
    
    {
      GtkWidget *item;
      item = gtk_menu_item_new_with_label("Downloading Dropbox...");
      ctx->percent_done_label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(item)));
      
      g_object_set(item, "sensitive", FALSE, NULL);
      gtk_menu_shell_append(GTK_MENU_SHELL(ctx->ndt->context_menu), item);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(ctx->ndt->context_menu),
			  gtk_separator_menu_item_new());

    {
      GtkWidget *item;
      item = gtk_menu_item_new_with_label("Cancel Download");
      
      gtk_menu_shell_append(GTK_MENU_SHELL(ctx->ndt->context_menu), item);

      g_signal_connect(G_OBJECT(item), "activate",
		       G_CALLBACK(activate_cancel_download), ctx);
    }

    menu_refresh(ctx->ndt);
  }
}

static void
nautilus_dropbox_tray_start_dropbox_transfer(NautilusDropboxTray *ndt) {
  /* setup the menu here */
  gtk_container_remove_all(GTK_CONTAINER(ndt->context_menu));
  {
    GtkWidget *item;
    item = gtk_menu_item_new_with_label("Attempting to download Dropbox...");
    g_object_set(item, "sensitive", FALSE, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(ndt->context_menu), item);
  }
  menu_refresh(ndt);

  gtk_status_icon_set_tooltip(ndt->status_icon, "Downloading Dropbox...");
  
  {
    gchar *dropbox_platform, *webpath;
    HttpDownloadCtx *ctx;
    ctx = g_new(HttpDownloadCtx, 1);
    ctx->ndt = ndt;
    
    dropbox_platform = nautilus_dropbox_common_get_platform();
    webpath = g_strdup_printf("/download?plat=%s", dropbox_platform);
    
    if (make_async_http_get_request("www.getdropbox.com", webpath,
				    NULL, (HttpResponseHandler) handle_dropbox_download_response,
				    (gpointer) ctx) == FALSE) {
      fail_dropbox_download(ndt, NULL);
    }
    
    g_free(dropbox_platform);
    g_free(webpath);
  }
}

static void
on_connect(NautilusDropboxTray *ndt) {
  /* reset these vars */
  ndt->ca.user_quit = FALSE;
  ndt->ca.dropbox_starting = FALSE;

  /* tell dropbox what X server we're on */
  dropbox_command_client_send_command(&(ndt->dc->dcc), NULL, NULL,
				      "on_x_server", "display", g_getenv("DISPLAY"), NULL);

  /* find out if we are out of date */
  dropbox_command_client_send_command(&(ndt->dc->dcc), NULL, 
				      ndt, "nautilus-dropbox-version",
				      "version", PACKAGE_VERSION, NULL);

  gtk_status_icon_set_tooltip(ndt->status_icon, "Dropbox");
  /* make our icon invisible */
  gtk_status_icon_set_visible(ndt->status_icon, FALSE); 
}

static void
connection_attempt(guint i, NautilusDropboxTray *ndt) {
  if (ndt->ca.user_quit == FALSE &&
      i > 3 &&
      ndt->ca.dropbox_starting == FALSE) {
    ndt->ca.dropbox_starting = TRUE;
#ifndef ND_DEBUG    
    debug("couldn't connect to dropbox, auto-starting");
    if (!nautilus_dropbox_common_start_dropbox()) {
      nautilus_dropbox_tray_start_dropbox_transfer(ndt);
    }
#endif
  }
}

static void
on_disconnect(NautilusDropboxTray *ndt) {
  if (ndt->ca.user_quit == TRUE) {
    install_start_dropbox_menu(ndt);
    gtk_status_icon_set_tooltip(ndt->status_icon, "Dropbox");
  }
  else {
    GtkWidget *item;

    item = gtk_menu_item_new_with_label("Reconnecting to Dropbox...");
    g_object_set(item, "sensitive", FALSE, NULL);    

    gtk_container_remove_all(GTK_CONTAINER(ndt->context_menu));
    gtk_menu_shell_append(GTK_MENU_SHELL(ndt->context_menu), item);

    menu_refresh(ndt);

    gtk_status_icon_set_tooltip(ndt->status_icon, "Reconnecting to Dropbox...");
  }

  /* make our icon visible */
  gtk_status_icon_set_visible(ndt->status_icon, TRUE); 
}

void
nautilus_dropbox_tray_setup(NautilusDropboxTray *ndt, DropboxClient *dc) {
  ndt->ca.user_quit = FALSE;
  ndt->ca.dropbox_starting = FALSE;

  ndt->dc = dc;
 
  /* register connect handler */
  dropbox_client_add_on_connect_hook(dc,
				     (DropboxClientConnectHook) on_connect, 
				     ndt);
  dropbox_client_add_on_disconnect_hook(dc,
					(DropboxClientConnectHook) on_disconnect, 
					ndt);
  dropbox_client_add_connection_attempt_hook(dc,
					     (DropboxClientConnectionAttemptHook)
					     connection_attempt, ndt);

  /* register hooks from the daemon */
  nautilus_dropbox_hooks_add(&(dc->hookserv), "bubble",
			     (DropboxUpdateHook) handle_bubble, ndt);
  nautilus_dropbox_hooks_add(&(dc->hookserv), "launch_folder",
			     (DropboxUpdateHook) handle_launch_folder, ndt);
  nautilus_dropbox_hooks_add(&(dc->hookserv), "highlight_file",
			     (DropboxUpdateHook) handle_highlight_file, ndt);
  

  {
    GdkPixbuf *dbicon, *old;

    old = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "dropbox", 16, 
				   0, NULL);

    dbicon = gdk_pixbuf_copy(old);
    g_object_unref(old);
    
    ndt->status_icon = gtk_status_icon_new_from_pixbuf(dbicon); 
    gtk_status_icon_set_visible(ndt->status_icon, TRUE); 
  }


  /* set up tray menu */
  gtk_status_icon_set_tooltip(ndt->status_icon, "Dropbox");
  {
    GtkWidget *item;
    ndt->context_menu = GTK_MENU(gtk_menu_new());
    item = gtk_menu_item_new_with_label("Connecting to Dropbox...");
    gtk_menu_shell_append(GTK_MENU_SHELL(ndt->context_menu), item);
    g_object_set(item, "sensitive", FALSE, NULL);    
  }

  gtk_status_icon_set_visible(ndt->status_icon, TRUE); 

  /* Connect signals */
  g_signal_connect (G_OBJECT (ndt->status_icon), "popup-menu",
		    G_CALLBACK (popup), ndt);
  
  /* TODO: do a popup notification if this failed */
  ndt->notify_inited = notify_init("nautilus-dropbox");
}
