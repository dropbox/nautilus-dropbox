/*
 * Copyright 2008 Evenflow, Inc.
 *
 * nautilus-dropbox.c
 * Implements the Nautilus extension API for Dropbox. 
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
#include <config.h> /* for GETTEXT_PACKAGE */
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <nautilus-extension.h>

#include "g-util.h"
#include "dropbox-command-client.h"
#include "nautilus-dropbox.h"
#include "nautilus-dropbox-hooks.h"

static char *emblems[] = {"dropbox-uptodate", "dropbox-syncing", "dropbox-unsyncable"};
gchar *DEFAULT_EMBLEM_PATHS[2] = { EMBLEMDIR , NULL };


gboolean dropbox_use_operation_in_progress_workaround;

static GType dropbox_type = 0;

/* for old versions of glib */
#if 0  // Silence Warnings.
static void my_g_hash_table_get_keys_helper(gpointer key,
					    gpointer value,
					    GList **ud) {
  *ud = g_list_append(*ud, key);
}

static GList *my_g_hash_table_get_keys(GHashTable *ght) {
  GList *list = NULL;
  g_hash_table_foreach(ght, (GHFunc) my_g_hash_table_get_keys_helper, &list);
  return list;
}
#endif

/*
  Simplifies a path by removing navigation elements such as '.' and '..'

  Arguments:
    - path: input path to be canonicalized

  Returns:
    Canonicalized path if input path is valid.
    NULL otherwise.
*/
static gchar *
canonicalize_path(gchar *path) {
  int i, j = 0;
  gchar *toret = NULL;
  gchar **cpy, **elts;
  
  g_assert(path != NULL);
  g_assert(path[0] == '/');

  elts = g_strsplit(path, "/", 0);
  cpy = g_new(gchar *, g_strv_length(elts)+1);
  cpy[j++] = "/";
  for (i = 0; elts[i] != NULL; i++) {
    if (strcmp(elts[i], "..") == 0) {
      if (j > 0) {
        j--;
      }
      else {
        // Input path has too many parent directory references and is invalid
        toret = NULL;
        goto exit;
      }
    }
    else if (strcmp(elts[i], ".") != 0 && elts[i][0] != '\0') {
      cpy[j++] = elts[i];
    }
  }
  
  cpy[j] = NULL;
  toret = g_build_filenamev(cpy);

exit:
  g_free(cpy);
  g_strfreev(elts);
  
  return toret;
}

static void
reset_file(NautilusFileInfo *file) {
  debug("resetting file %p", (void *) file);
  nautilus_file_info_invalidate_extension_info(file);
}

gboolean
reset_all_files(NautilusDropbox *cvs) {
  /* Only run this on the main loop or you'll cause problems. */

  /* this works because you can call a function pointer with
     more arguments than it takes */
  g_hash_table_foreach(cvs->obj2filename, (GHFunc) reset_file, NULL);
  return FALSE;
}


static void
when_file_dies(NautilusDropbox *cvs, NautilusFileInfo *address) {
  gchar *filename;

  filename = g_hash_table_lookup(cvs->obj2filename, address);
  
  /* we never got a change to view this file */
  if (filename == NULL) {
    return;
  }

  /* too chatty */
  /*  debug("removing %s <-> 0x%p", filename, address); */

  g_hash_table_remove(cvs->filename2obj, filename);
  g_hash_table_remove(cvs->obj2filename, address);
}

static void
changed_cb(NautilusFileInfo *file, NautilusDropbox *cvs) {
  /* check if this file's path has changed, if so update the hash and invalidate
     the file */
  gchar *filename, *pfilename;
  gchar *filename2;
  gchar *uri;

  uri = nautilus_file_info_get_uri(file);
  pfilename = g_filename_from_uri(uri, NULL, NULL);
  filename = pfilename ? canonicalize_path(pfilename) : NULL;

  /* Canonicalization will only null-out a non-null filename if it is invalid */
  g_assert((pfilename == NULL && filename == NULL) || (pfilename != NULL && filename != NULL));

  filename2 =  g_hash_table_lookup(cvs->obj2filename, file);

  g_free(pfilename);
  g_free(uri);

  /* if filename2 is NULL we've never seen this file in update_file_info */
  if (filename2 == NULL) {
    g_free(filename);
    return;
  }

  if (filename == NULL) {
      /* A file has moved to offline storage. Lets remove it from our tables. */
      g_object_weak_unref(G_OBJECT(file), (GWeakNotify) when_file_dies, cvs);
      g_hash_table_remove(cvs->filename2obj, filename2);
      g_hash_table_remove(cvs->obj2filename, file);
      g_signal_handlers_disconnect_by_func(file, G_CALLBACK(changed_cb), cvs);
      reset_file(file);
      return;
  }

  /* this is a hack, because nautilus doesn't do this for us, for some reason
     the file's path has changed */
  if (strcmp(filename, filename2) != 0) {
    debug("shifty old: %s, new %s", filename2, filename);

    /* gotta do this first, the call after this frees filename2 */
    g_hash_table_remove(cvs->filename2obj, filename2);

    g_hash_table_replace(cvs->obj2filename, file, g_strdup(filename));

    {
      NautilusFileInfo *f2;
      /* we shouldn't have another mapping from filename to an object */
      f2 = g_hash_table_lookup(cvs->filename2obj, filename);
      if (f2 != NULL) {
	/* lets fix it if it's true, just remove the mapping */
	g_hash_table_remove(cvs->filename2obj, filename);
	g_hash_table_remove(cvs->obj2filename, f2);
      }
    }

    g_hash_table_insert(cvs->filename2obj, g_strdup(filename), file);
    reset_file(file);
  }
  
  g_free(filename);
}

static NautilusOperationResult
nautilus_dropbox_update_file_info(NautilusInfoProvider     *provider,
                                  NautilusFileInfo         *file,
                                  GClosure                 *update_complete,
                                  NautilusOperationHandle **handle) {
  NautilusDropbox *cvs;

  cvs = NAUTILUS_DROPBOX(provider);

  /* this code adds this file object to our two-way hash of file objects
     so we can shell touch these files later */
  {
    gchar *pfilename, *uri;

    uri = nautilus_file_info_get_uri(file);
    pfilename = g_filename_from_uri(uri, NULL, NULL);
    g_free(uri);
    if (pfilename == NULL) {
      return NAUTILUS_OPERATION_COMPLETE;
    }
    else {
      int cmp = 0;
      gchar *stored_filename;
      gchar *filename;
      
      filename = canonicalize_path(pfilename);
      g_free(pfilename);
      if (filename == NULL) {
        /* pfilename path was invalid if canonicalize operation nulled it out */
        return NAUTILUS_OPERATION_FAILED;
      }
      stored_filename = g_hash_table_lookup(cvs->obj2filename, file);

      /* don't worry about the dup checks, gcc is smart enough to optimize this
	 GCSE ftw */
      if ((stored_filename != NULL && (cmp = strcmp(stored_filename, filename)) != 0) ||
	  stored_filename == NULL) {
	
	if (stored_filename != NULL && cmp != 0) {
	  /* this happens when the filename changes name on a file obj 
	     but changed_cb isn't called */
	  g_object_weak_unref(G_OBJECT(file), (GWeakNotify) when_file_dies, cvs);
	  g_hash_table_remove(cvs->obj2filename, file);
	  g_hash_table_remove(cvs->filename2obj, stored_filename);
	  g_signal_handlers_disconnect_by_func(file, G_CALLBACK(changed_cb), cvs);
	}
	else if (stored_filename == NULL) {
	  NautilusFileInfo *f2;

	  if ((f2 = g_hash_table_lookup(cvs->filename2obj, filename)) != NULL) {
	    /* if the filename exists in the filename2obj hash
	       but the file obj doesn't exist in the obj2filename hash:
	       
	       this happens when nautilus allocates another file object
	       for a filename without first deleting the original file object
	       
	       just remove the association to the older file object, it's obsolete
	    */
	    g_object_weak_unref(G_OBJECT(f2), (GWeakNotify) when_file_dies, cvs);
	    g_signal_handlers_disconnect_by_func(f2, G_CALLBACK(changed_cb), cvs);
	    g_hash_table_remove(cvs->filename2obj, filename);
	    g_hash_table_remove(cvs->obj2filename, f2);
	  }
	}

	/* too chatty */
	/* debug("adding %s <-> 0x%p", filename, file);*/
	g_object_weak_ref(G_OBJECT(file), (GWeakNotify) when_file_dies, cvs);
	g_hash_table_insert(cvs->filename2obj, g_strdup(filename), file);
	g_hash_table_insert(cvs->obj2filename, file, g_strdup(filename));
	g_signal_connect(file, "changed", G_CALLBACK(changed_cb), cvs);
      }

      g_free(filename);
    }
  }

  if (dropbox_client_is_connected(&(cvs->dc)) == FALSE ||
      nautilus_file_info_is_gone(file)) {
    return NAUTILUS_OPERATION_COMPLETE;
  }

  {
    DropboxFileInfoCommand *dfic = g_new0(DropboxFileInfoCommand, 1);

    dfic->cancelled = FALSE;
    dfic->provider = provider;
    dfic->dc.request_type = GET_FILE_INFO;
    dfic->update_complete = g_closure_ref(update_complete);
    dfic->file = g_object_ref(file);
    
    dropbox_command_client_request(&(cvs->dc.dcc), (DropboxCommand *) dfic);
    
    *handle = (NautilusOperationHandle *) dfic;
    
    return dropbox_use_operation_in_progress_workaround
      ? NAUTILUS_OPERATION_COMPLETE
      : NAUTILUS_OPERATION_IN_PROGRESS;
  }
}

static void
handle_shell_touch(GHashTable *args, NautilusDropbox *cvs) {
  gchar **path;

  //  debug_enter();

  if ((path = g_hash_table_lookup(args, "path")) != NULL &&
      path[0][0] == '/') {
    NautilusFileInfo *file;
    gchar *filename;

    filename = canonicalize_path(path[0]);
    if (filename != NULL) {
      debug("shell touch for %s", filename);

      file = g_hash_table_lookup(cvs->filename2obj, filename);

      if (file != NULL) {
        debug("gonna reset %s", filename);
        reset_file(file);
      }
      g_free(filename);
    }
  }

  return;
}

gboolean
nautilus_dropbox_finish_file_info_command(DropboxFileInfoCommandResponse *dficr) {

  //debug_enter();
  NautilusOperationResult result = NAUTILUS_OPERATION_FAILED;

  if (!dficr->dfic->cancelled) {
    gchar **status = NULL;
    gboolean isdir;

    isdir = nautilus_file_info_is_directory(dficr->dfic->file) ;

    /* if we have emblems just use them. */
    if (dficr->emblems_response != NULL &&
	(status = g_hash_table_lookup(dficr->emblems_response, "emblems")) != NULL) {
      int i;
      for ( i = 0; status[i] != NULL; i++) {
	  if (status[i][0])
	    nautilus_file_info_add_emblem(dficr->dfic->file, status[i]);
      }
      result = NAUTILUS_OPERATION_COMPLETE;
    }
    /* if the file status command went okay */
    else if ((dficr->file_status_response != NULL &&
	(status =
	 g_hash_table_lookup(dficr->file_status_response, "status")) != NULL) &&
	((isdir == TRUE &&
	  dficr->folder_tag_response != NULL) || isdir == FALSE)) {
      gchar **tag = NULL;

      /* set the tag emblem */
      if (isdir &&
	  (tag = g_hash_table_lookup(dficr->folder_tag_response, "tag")) != NULL) {
	if (strcmp("public", tag[0]) == 0) {
	  nautilus_file_info_add_emblem(dficr->dfic->file, "web");
	}
	else if (strcmp("shared", tag[0]) == 0) {
	  nautilus_file_info_add_emblem(dficr->dfic->file, "people");
	}
	else if (strcmp("photos", tag[0]) == 0) {
	  nautilus_file_info_add_emblem(dficr->dfic->file, "photos");
	}
	else if (strcmp("sandbox", tag[0]) == 0) {
	  nautilus_file_info_add_emblem(dficr->dfic->file, "star");
	}
      }

      /* set the status emblem */
      {
	int emblem_code = 0;

	if (strcmp("up to date", status[0]) == 0) {
	  emblem_code = 1;
	}
	else if (strcmp("syncing", status[0]) == 0) {
	  emblem_code = 2;
	}
	else if (strcmp("unsyncable", status[0]) == 0) {
	  emblem_code = 3;
	}

	if (emblem_code > 0) {
	  /*
	    debug("%s to %s", emblems[emblem_code-1],
	    g_filename_from_uri(nautilus_file_info_get_uri(dficr->dfic->file),
	    NULL, NULL));
	  */
	  nautilus_file_info_add_emblem(dficr->dfic->file, emblems[emblem_code-1]);
	}
      }
      result = NAUTILUS_OPERATION_COMPLETE;
    }
  }

  /* complete the info request */
  if (!dropbox_use_operation_in_progress_workaround) {
      nautilus_info_provider_update_complete_invoke(dficr->dfic->update_complete,
						    dficr->dfic->provider,
						    (NautilusOperationHandle*) dficr->dfic,
						    result);
  }

  /* destroy the objects we created */
  if (dficr->file_status_response != NULL)
    g_hash_table_unref(dficr->file_status_response);
  if (dficr->folder_tag_response != NULL)
    g_hash_table_unref(dficr->folder_tag_response);
  if (dficr->emblems_response != NULL)
    g_hash_table_unref(dficr->emblems_response);

  /* unref the objects we didn't create */
  g_closure_unref(dficr->dfic->update_complete);
  g_object_unref(dficr->dfic->file);

  /* now free the structs */
  g_free(dficr->dfic);
  g_free(dficr);

  return FALSE;
}

static void
nautilus_dropbox_cancel_update(NautilusInfoProvider     *provider,
                               NautilusOperationHandle  *handle) {
  DropboxFileInfoCommand *dfic = (DropboxFileInfoCommand *) handle;
  dfic->cancelled = TRUE;
  return;
}

static void
menu_item_cb(NautilusMenuItem *item,
	     NautilusDropbox *cvs) {
  gchar *verb;
  GList *files;
  DropboxGeneralCommand *dcac;

  dcac = g_new(DropboxGeneralCommand, 1);

  /* maybe these would be better passed in a container
     struct used as the userdata pointer, oh well this
     is how dave camp does it */
  files = g_object_get_data(G_OBJECT(item), "nautilus_dropbox_files");
  verb = g_object_get_data(G_OBJECT(item), "nautilus_dropbox_verb");

  dcac->dc.request_type = GENERAL_COMMAND;

  /* build the argument list */
  dcac->command_args = g_hash_table_new_full((GHashFunc) g_str_hash,
					     (GEqualFunc) g_str_equal,
					     (GDestroyNotify) g_free,
					     (GDestroyNotify) g_strfreev);
  {
    gchar **arglist;
    guint i;
    GList *li;

    arglist = g_new0(gchar *,g_list_length(files) + 1);

    for (li = files, i = 0; li != NULL; li = g_list_next(li)) {
      char *uri = nautilus_file_info_get_uri(NAUTILUS_FILE_INFO(li->data));
      char *path = g_filename_from_uri(uri, NULL, NULL);
      g_free(uri);
      if (!path)
	continue;
      arglist[i] = path;
      i++;
    }

    g_hash_table_insert(dcac->command_args,
			g_strdup("paths"),
			arglist);
  }

  {
    gchar **arglist;
    arglist = g_new(gchar *, 2);
    arglist[0] = g_strdup(verb);
    arglist[1] = NULL;
    g_hash_table_insert(dcac->command_args, g_strdup("verb"), arglist);
  }

  dcac->command_name = g_strdup("icon_overlay_context_action");
  dcac->handler = NULL;
  dcac->handler_ud = NULL;

  dropbox_command_client_request(&(cvs->dc.dcc), (DropboxCommand *) dcac);
}

static char from_hex(gchar ch) {
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

// decode in --> out, but dont fill more than n chars into out
// returns len of out if thing went well, -1 if n wasn't big enough
// can be used in place (whoa!)
int GhettoURLDecode(gchar* out, gchar* in, int n) {
  char *out_initial;

  for(out_initial = out; out-out_initial < n && *in != '\0'; out++) {
    if (*in == '%') {
      if ((in[1] != '\0') && (in[2] != '\0')) {
        *out = from_hex(in[1]) << 4 | from_hex(in[2]);
        in += 3;
      }
      else {
        // Input string isn't well-formed
        return -1;
      }
    }
    else {
      *out = *in;
      in++;
    }
  }

  if (out-out_initial < n) {
    *out = '\0';
    return out-out_initial;
  }
  return -1;
}

static int
nautilus_dropbox_parse_menu(gchar			**options,
			    NautilusMenu		*menu,
			    GString			*old_action_string,
			    NautilusMenuProvider	*provider,
			    GList			*files)
{
  int ret = 0;
  int i;

  for ( i = 0; options[i] != NULL; i++) {
    gchar **option_info = g_strsplit(options[i], "~", 3);
    /* if this is a valid string */
    if (option_info[0] == NULL || option_info[1] == NULL ||
	option_info[2] == NULL || option_info[3] != NULL) {
	g_strfreev(option_info);
	continue;
    }

    gchar* item_name = option_info[0];
    gchar* item_inner = option_info[1];
    gchar* verb = option_info[2];

    GhettoURLDecode(item_name, item_name, strlen(item_name));
    GhettoURLDecode(verb, verb, strlen(verb));
    GhettoURLDecode(item_inner, item_inner, strlen(item_inner));

    // If the inner section has a menu in it then we create a submenu.  The verb will be ignored.
    // Otherwise add the verb to our map and add the menu item to the list.
    if (strchr(item_inner, '~') != NULL) {
      GString *new_action_string = g_string_new(old_action_string->str);
      gchar **suboptions = g_strsplit(item_inner, "|", -1);
      NautilusMenuItem *item;
      NautilusMenu *submenu = nautilus_menu_new();

      g_string_append(new_action_string, item_name);
      g_string_append(new_action_string, "::");

      ret += nautilus_dropbox_parse_menu(suboptions, submenu, new_action_string,
					 provider, files);

      item = nautilus_menu_item_new(new_action_string->str,
				    item_name, "", NULL);
      nautilus_menu_item_set_submenu(item, submenu);
      nautilus_menu_append_item(menu, item);

      g_strfreev(suboptions);
      g_object_unref(item);
      g_object_unref(submenu);
      g_string_free(new_action_string, TRUE);
    } else {
      NautilusMenuItem *item;
      GString *new_action_string = g_string_new(old_action_string->str);
      gboolean grayed_out = FALSE;

      g_string_append(new_action_string, verb);

      if (item_name[0] == '!') {
	  item_name++;
	  grayed_out = TRUE;
      }

      item = nautilus_menu_item_new(new_action_string->str, item_name, item_inner, NULL);

      nautilus_menu_append_item(menu, item);
      /* add the file metadata to this item */
      g_object_set_data_full (G_OBJECT(item), "nautilus_dropbox_files",
			      nautilus_file_info_list_copy (files),
			      (GDestroyNotify) nautilus_file_info_list_free);
      /* add the verb metadata */
      g_object_set_data_full (G_OBJECT(item), "nautilus_dropbox_verb",
			      g_strdup(verb),
			      (GDestroyNotify) g_free);
      g_signal_connect (item, "activate", G_CALLBACK (menu_item_cb), provider);

      if (grayed_out) {
	GValue sensitive = { 0 };
	g_value_init (&sensitive, G_TYPE_BOOLEAN);
	g_value_set_boolean (&sensitive, FALSE);
	g_object_set_property (G_OBJECT(item), "sensitive", &sensitive);
      }

      g_object_unref(item);
      g_string_free(new_action_string, TRUE);
      ret++;
    }
    g_strfreev(option_info);
  }
  return ret;
}

static void
get_file_items_callback(GHashTable *response, gpointer ud)
{
  GAsyncQueue *reply_queue = ud;

  /* queue_push doesn't accept NULL as a value so we create an empty hash table
   * if we got no response. */
  g_async_queue_push(reply_queue, response ? g_hash_table_ref(response) :
		     g_hash_table_new((GHashFunc) g_str_hash, (GEqualFunc) g_str_equal));
  g_async_queue_unref(reply_queue);
}


static GList *
nautilus_dropbox_get_file_items(NautilusMenuProvider *provider,
				GList                *files)
{
  /*
   * 1. Convert files to filenames.
   */
  int file_count = g_list_length(files);

  if (file_count < 1)
    return NULL;

  gchar **paths = g_new0(gchar *, file_count + 1);
  int i = 0;
  GList* elem;

  for (elem = files; elem; elem = elem->next, i++) {
    gchar *uri = nautilus_file_info_get_uri(elem->data);
    gchar *filename_un = uri ? g_filename_from_uri(uri, NULL, NULL) : NULL;
    gchar *filename = filename_un ? g_filename_to_utf8(filename_un, -1, NULL, NULL, NULL) : NULL;

    g_free(uri);
    g_free(filename_un);

    if (filename == NULL) {
      /* oooh, filename wasn't correctly encoded, or isn't a local file.  */
      g_strfreev(paths);
      return NULL;
    }

    paths[i] = filename;
  }

  GAsyncQueue *reply_queue = g_async_queue_new_full((GDestroyNotify)g_hash_table_unref);
  
  /*
   * 2. Create a DropboxGeneralCommand to call "icon_overlay_context_options"
   */

  DropboxGeneralCommand *dgc = g_new0(DropboxGeneralCommand, 1);
  dgc->dc.request_type = GENERAL_COMMAND;
  dgc->command_name = g_strdup("icon_overlay_context_options");
  dgc->command_args = g_hash_table_new_full((GHashFunc) g_str_hash,
					    (GEqualFunc) g_str_equal,
					    (GDestroyNotify) g_free,
					    (GDestroyNotify) g_strfreev);
  g_hash_table_insert(dgc->command_args, g_strdup("paths"), paths);
  dgc->handler = get_file_items_callback;
  dgc->handler_ud = g_async_queue_ref(reply_queue);

  /*
   * 3. Queue it up for the helper thread to run it.
   */
  NautilusDropbox *cvs = NAUTILUS_DROPBOX(provider);
  dropbox_command_client_request(&(cvs->dc.dcc), (DropboxCommand *) dgc);

  /*
   * 4. We have to block until it's done because nautilus expects a reply.  But we will
   * only block for 50 ms for a reply.
   */

  GHashTable *context_options_response = g_async_queue_timeout_pop(reply_queue, 50000);
  g_async_queue_unref(reply_queue);

  if (!context_options_response) {
      return NULL;
  }

  /*
   * 5. Parse the reply.
   */

  char **options = g_hash_table_lookup(context_options_response, "options");
  GList *toret = NULL;

  if (options && *options && **options)  {
    /* build the menu */
    NautilusMenuItem *root_item;
    NautilusMenu *root_menu;
    GList *submenu_items;
    guint list_length;

    root_menu = nautilus_menu_new();

    GString *action_string = g_string_new("NautilusDropbox::");

    nautilus_dropbox_parse_menu(options, root_menu, action_string,
				provider, files);

    submenu_items = nautilus_menu_get_items(root_menu);
    list_length = g_list_length(submenu_items);

    if (list_length == 1) {
      /* Return only the single menu item */
      toret = submenu_items;
    } else {
      /* Construct a Dropbox submenu */
      root_item = nautilus_menu_item_new("NautilusDropbox::root_item",
					 "Dropbox", "Dropbox Options",
					 "dropbox");
      nautilus_menu_item_set_submenu(root_item, root_menu);
      toret = g_list_append(toret, root_item);

      g_list_free_full(submenu_items, g_object_unref);
    }

    g_string_free(action_string, TRUE);
    g_object_unref(root_menu);
  }

  g_hash_table_unref(context_options_response);

  return toret;
}

gboolean
add_emblem_paths(GHashTable* emblem_paths_response)
{
  /* Only run this on the main loop or you'll cause problems. */
  if (!emblem_paths_response)
    return FALSE;

  gchar **emblem_paths_list;
  int i;
  GtkIconTheme *theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());

  if (emblem_paths_response &&
      (emblem_paths_list = g_hash_table_lookup(emblem_paths_response, "path"))) {
      for (i = 0; emblem_paths_list[i] != NULL; i++) {
	if (emblem_paths_list[i][0])
	  gtk_icon_theme_add_search_path(theme, emblem_paths_list[i]);
      }
  }
  g_hash_table_unref(emblem_paths_response);
  return FALSE;
}

gboolean
remove_emblem_paths(GHashTable* emblem_paths_response)
{
  /* Only run this on the main loop or you'll cause problems. */
  if (!emblem_paths_response)
    return FALSE;

  gchar **emblem_paths_list = g_hash_table_lookup(emblem_paths_response, "path");
  if (!emblem_paths_list)
      goto exit;

  // We need to remove the old paths.
  gchar ** paths;
  GtkIconTheme *theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());

  paths = gtk_icon_theme_get_search_path(theme);

  gint i, j, out = 0;
  gboolean found = FALSE;
  for (i = 0; paths[i] != NULL; i++) {
      gboolean keep = TRUE;
      for (j = 0; emblem_paths_list[j] != NULL; j++) {
	  if (emblem_paths_list[j][0]) {
	      if (!g_strcmp0(paths[i], emblem_paths_list[j])) {
		  found = TRUE;
		  keep = FALSE;
		  g_free(paths[i]);
		  break;
	      }
	  }
      }
      if (keep) {
	  paths[out] = paths[i];
	  out++;
      }
  }

  /* If we found one we need to reset the path to
     accomodate the changes */
  if (found) {
    paths[out] = NULL; /* Clear the last one */
    gtk_icon_theme_set_search_path(theme, (const gchar **)paths);
  }

  g_strfreev(paths);
exit:
  g_hash_table_unref(emblem_paths_response);
  return FALSE;
}

void get_emblem_paths_cb(GHashTable *emblem_paths_response, NautilusDropbox *cvs)
{
  if (!emblem_paths_response) {
      emblem_paths_response = g_hash_table_new((GHashFunc) g_str_hash,
					       (GEqualFunc) g_str_equal);
      g_hash_table_insert(emblem_paths_response, "path", DEFAULT_EMBLEM_PATHS);
  } else {
      /* Increase the ref so that finish_general_command doesn't delete it. */
      g_hash_table_ref(emblem_paths_response);
  }

  g_mutex_lock(&(cvs->emblem_paths_mutex));
  if (cvs->emblem_paths) {
    g_idle_add((GSourceFunc) remove_emblem_paths, cvs->emblem_paths);
    cvs->emblem_paths = NULL;
  }
  cvs->emblem_paths = emblem_paths_response;
  g_mutex_unlock(&(cvs->emblem_paths_mutex));

  g_idle_add((GSourceFunc) add_emblem_paths, g_hash_table_ref(emblem_paths_response));
  g_idle_add((GSourceFunc) reset_all_files, cvs);
}

static void
on_connect(NautilusDropbox *cvs) {
  reset_all_files(cvs);

  dropbox_command_client_send_command(&(cvs->dc.dcc),
				      (NautilusDropboxCommandResponseHandler) get_emblem_paths_cb,
				      cvs, "get_emblem_paths", NULL);
}

static void
on_disconnect(NautilusDropbox *cvs) {
  reset_all_files(cvs);

  g_mutex_lock(&(cvs->emblem_paths_mutex));
  /* This call will free the data too. */
  g_idle_add((GSourceFunc) remove_emblem_paths, cvs->emblem_paths);
  cvs->emblem_paths = NULL;
  g_mutex_unlock(&(cvs->emblem_paths_mutex));
}


static void
nautilus_dropbox_menu_provider_iface_init (NautilusMenuProviderInterface *iface) {
  iface->get_file_items = nautilus_dropbox_get_file_items;
  return;
}

static void
nautilus_dropbox_info_provider_iface_init (NautilusInfoProviderInterface *iface) {
  iface->update_file_info = nautilus_dropbox_update_file_info;
  iface->cancel_update = nautilus_dropbox_cancel_update;
  return;
}

static void
nautilus_dropbox_instance_init (NautilusDropbox *cvs) {
  cvs->filename2obj = g_hash_table_new_full((GHashFunc) g_str_hash,
					    (GEqualFunc) g_str_equal,
					    (GDestroyNotify) g_free,
					    (GDestroyNotify) NULL);
  cvs->obj2filename = g_hash_table_new_full((GHashFunc) g_direct_hash,
					    (GEqualFunc) g_direct_equal,
					    (GDestroyNotify) NULL,
					    (GDestroyNotify) g_free);
  g_mutex_init(&(cvs->emblem_paths_mutex));
  cvs->emblem_paths = NULL;

  /* setup the connection obj*/
  dropbox_client_setup(&(cvs->dc));

  /* our hooks */
  nautilus_dropbox_hooks_add(&(cvs->dc.hookserv), "shell_touch",
			     (DropboxUpdateHook) handle_shell_touch, cvs);

  /* add connection handlers */
  dropbox_client_add_on_connect_hook(&(cvs->dc),
				     (DropboxClientConnectHook) on_connect,
				     cvs);
  dropbox_client_add_on_disconnect_hook(&(cvs->dc),
					(DropboxClientConnectHook) on_disconnect,
					cvs);
  
  /* now start the connection */
  debug("about to start client connection");
  dropbox_client_start(&(cvs->dc));

  return;
}

static void
nautilus_dropbox_class_init (NautilusDropboxClass *class) {
}

static void
nautilus_dropbox_class_finalize (NautilusDropboxClass *class) {
  debug("just checking");
  /* kill threads here? */
}

GType
nautilus_dropbox_get_type (void) {
  return dropbox_type;
}

void
nautilus_dropbox_register_type (GTypeModule *module) {
  static const GTypeInfo info = {
    sizeof (NautilusDropboxClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) nautilus_dropbox_class_init,
    (GClassFinalizeFunc) nautilus_dropbox_class_finalize,
    NULL,
    sizeof (NautilusDropbox),
    0,
    (GInstanceInitFunc) nautilus_dropbox_instance_init,
  };

  static const GInterfaceInfo menu_provider_iface_info = {
    (GInterfaceInitFunc) nautilus_dropbox_menu_provider_iface_init,
    NULL,
    NULL
  };

  static const GInterfaceInfo info_provider_iface_info = {
    (GInterfaceInitFunc) nautilus_dropbox_info_provider_iface_init,
    NULL,
    NULL
  };

  dropbox_type =
    g_type_module_register_type(module,
				G_TYPE_OBJECT,
				"NautilusDropbox",
				&info, 0);
  
  g_type_module_add_interface (module,
			       dropbox_type,
			       NAUTILUS_TYPE_MENU_PROVIDER,
			       &menu_provider_iface_info);

  g_type_module_add_interface (module,
			       dropbox_type,
			       NAUTILUS_TYPE_INFO_PROVIDER,
			       &info_provider_iface_info);
}
