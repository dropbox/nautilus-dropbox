/*
 * Copyright 2008 Evenflow, Inc.
 *
 * nautilus-dropbox-common.c
 * Utility functions for nautilus-dropbox
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

#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include <config.h> /* for GETTEXT_PACKAGE */
#endif

#include <glib.h>
#include <gdk/gdk.h>

#include "g-util.h"
#include "nautilus-dropbox-common.h"
#include "nautilus-dropbox-command.h"
#include "nautilus-dropbox-tray.h"

typedef struct {
  NautilusDropbox *cvs;
  NautilusDropboxGlobalCB cb;
  gpointer ud;
} GetGlobalsData;

static void
get_dropbox_globals_cb(GHashTable *response, GetGlobalsData *ggd) {
  gchar **value;
  
  if (response != NULL &&
      (value = g_hash_table_lookup(response, "values")) != NULL) {
    ggd->cb(value, ggd->cvs, ggd->ud);
  }
  else {
    ggd->cb(NULL, ggd->cvs, ggd->ud);
  }
  
  g_free(ggd);
}

void
nautilus_dropbox_common_get_globals(NautilusDropbox *cvs,
				    gchar *tabbed_keys,
				    NautilusDropboxGlobalCB cb, gpointer ud) {
  DropboxGeneralCommand *dgc;

  /* TODO: change to g_return_if_fail */
  g_assert(cvs != NULL);
  g_assert(cb != NULL);
  g_assert(tabbed_keys != NULL);

  dgc = g_new(DropboxGeneralCommand, 1);
  
  dgc->dc.request_type = GENERAL_COMMAND;
  dgc->command_name = g_strdup("get_dropbox_globals");

  /* build command args */
  {
    dgc->command_args = g_hash_table_new_full((GHashFunc) g_str_hash,
					      (GEqualFunc) g_str_equal,
					      (GDestroyNotify) g_free,
					      (GDestroyNotify) g_strfreev);

    g_hash_table_insert(dgc->command_args, g_strdup("keys"),
			g_strsplit(tabbed_keys, "\t", 0));
  }
  
  dgc->handler = (NautilusDropboxCommandResponseHandler) get_dropbox_globals_cb;
  {
    GetGlobalsData *ggd;
    ggd = g_new0(GetGlobalsData, 1);
    ggd->cvs = cvs;
    ggd->cb = cb;
    ggd->ud = ud;
    dgc->handler_ud = (gpointer) ggd;
  }

  nautilus_dropbox_command_request(cvs, (DropboxCommand *) dgc);
}

static 
void dropboxd_setup(gpointer ud) {
  /* daemonize dropbox */
  setsid();
}

gboolean
nautilus_dropbox_common_start_dropbox(NautilusDropbox *cvs, gboolean download) {
  gchar *dropboxd_path;

  /* first kill any pre-existing instances of dropbox */
  /* TODO: is there a portable way to kill processes by name? */
  g_spawn_command_line_async("killall dropboxd", NULL);

  /* now check if dropboxd exists in the magic place,
     first generate executable name
   */
  dropboxd_path = g_strdup_printf("%s/.dropbox-dist/dropboxd",
                                  g_get_home_dir());

  /* if dropboxd exists let's just start it */
  if (g_file_test(dropboxd_path,
                  G_FILE_TEST_IS_EXECUTABLE | G_FILE_TEST_EXISTS)) {
    gchar **argv;

    argv = g_new(gchar *, 2);
    argv[0] = dropboxd_path;
    argv[1] = NULL;
    
    if (g_spawn_async(g_get_home_dir(), argv, NULL, 0, dropboxd_setup, NULL,
		      NULL, NULL)) {
      g_free(argv);
      goto OUTTRUE;
    }
    else {
      gchar *delete_dropbox_cmdline;

      g_free(argv);

      delete_dropbox_cmdline = g_strdup_printf("rm -rf %s/.dropbox-dist/",
                                               g_get_home_dir());
      g_spawn_command_line_async(delete_dropbox_cmdline, NULL);
      
      g_free(delete_dropbox_cmdline);

      if (download) {
	nautilus_dropbox_tray_start_dropbox_transfer(cvs);
	goto OUTTRUE;
      }
      else {
	goto OUTFALSE;
      }
    }
  }
  /* else we have to download it */
  else {
    if (download) {
      nautilus_dropbox_tray_start_dropbox_transfer(cvs);
      goto OUTTRUE;
    }
    else {
      goto OUTFALSE;
    }
  }

 OUTTRUE:
  g_free(dropboxd_path);
  return TRUE;

 OUTFALSE:
  g_free(dropboxd_path);
  return FALSE;
}

gchar *
nautilus_dropbox_common_get_platform() {
  /* this function is haxed, we don't support a plethora of
     platforms yet, so we don't need great platform detection */
  return g_strdup_printf("lnx-%s", sizeof(long) == 8 ? "x86_64" : "x86");
}


static void
handle_launch_command_dying(GPid pid, gint status, gpointer *ud) {
  if (status != 0) {
    nautilus_dropbox_tray_bubble(ud[0], ud[1], ud[2], NULL, NULL, NULL, NULL);
  }

  g_free(ud[1]);
  g_free(ud[2]);
  g_free(ud);
}

void
nautilus_dropbox_common_launch_command_with_error(NautilusDropbox * cvs,
						  const gchar *command_line,
						  const gchar *caption,
						  const gchar *msg) {
  GPid childpid;
  gint argc;
  gchar **argv;
  
  g_shell_parse_argv(command_line, &argc, &argv, NULL);
  
  if (!gdk_spawn_on_screen(gdk_screen_get_default(),
			   NULL, argv, NULL,
			   G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
			   G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_DO_NOT_REAP_CHILD,
			   NULL, NULL,
			   &childpid, NULL)) {
    nautilus_dropbox_tray_bubble(&(cvs->ndt), caption, msg, NULL, NULL, NULL, NULL);
  }
  else {
    /* undefined data struct (i.e. dynamic) */
    gpointer *ud;
    ud = g_new(gpointer, 3);
    ud[0] = &(cvs->ndt);
    ud[1] = g_strdup(caption);
    ud[2] = g_strdup(msg);
    g_child_watch_add(childpid,
		      (GChildWatchFunc) handle_launch_command_dying, ud);
  }
}

void
nautilus_dropbox_common_launch_folder(NautilusDropbox *cvs,
				      const gchar *folder_path) {
  gchar *command_line, *escaped_string;
  gchar *msg;
  
  escaped_string = g_strescape(folder_path, NULL);
  command_line = g_strdup_printf("nautilus \"%s\"", escaped_string);
  msg = g_strdup_printf("Couldn't start '%s'. Is nautilus in your PATH?",
			command_line);
  
  nautilus_dropbox_common_launch_command_with_error(cvs, command_line,
						    "Couldn't open folder", msg);
  
  g_free(msg);
  g_free(escaped_string);
  g_free(command_line);
}

