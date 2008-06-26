/*
 *
 *  nautilus-dropbox.c
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

#include <glib.h>
#include <glib/gprintf.h>
#include <glib-object.h>

#include <libnautilus-extension/nautilus-extension-types.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-extension/nautilus-info-provider.h>

#include "nautilus-dropbox-common.h"
#include "nautilus-dropbox.h"
#include "nautilus-dropbox-command.h"
#include "nautilus-dropbox-hooks.h"
#include "nautilus-dropbox-tray.h"

typedef struct {
  gchar *title;
  gchar *tooltip;
  gchar *verb;
} DropboxContextMenuItem;

static char *emblems[] = {"dropbox-uptodate", "dropbox-syncing"};

gboolean dropbox_use_nautilus_submenu_workaround;

static GType dropbox_type = 0;

static void
menu_item_free(gpointer data) {
  DropboxContextMenuItem *dcmi = (DropboxContextMenuItem *) data;
  g_free(dcmi->title);
  g_free(dcmi->tooltip);
  g_free(dcmi->verb);
  g_free(dcmi);
}

static void
test_cb(NautilusFileInfo *file, gpointer ud) {
  /* when the file changes, check if it's in the process of being updated
     if not, then invalidate it*/
  //  debug("%s changed", g_filename_from_uri(nautilus_file_info_get_uri(file), NULL, NULL));
}

static void
destroy_string(gpointer data) {
  g_string_free((GString *) data, TRUE);
  return;
}

static NautilusOperationResult
nautilus_dropbox_update_file_info(NautilusInfoProvider     *provider,
                                  NautilusFileInfo         *file,
                                  GClosure                 *update_complete,
                                  NautilusOperationHandle **handle) {
  NautilusDropbox *cvs;
  gchar *filename;
  
  filename = g_filename_from_uri(nautilus_file_info_get_uri(file), NULL, NULL);
  if (filename == NULL) {
    return NAUTILUS_OPERATION_COMPLETE;
  }
  
  cvs = NAUTILUS_DROPBOX(provider);

  if (g_list_index(cvs->file_store, file) == -1) {
    cvs->file_store = g_list_append(cvs->file_store, g_object_ref(file));
    /*g_signal_connect(file, "changed", G_CALLBACK(test_cb), NULL); */
  }

  if (nautilus_dropbox_command_is_connected(cvs) == FALSE) {
    return NAUTILUS_OPERATION_COMPLETE;
  }
  
  {
    DropboxFileInfoCommand *dfic = g_new0(DropboxFileInfoCommand, 1);

    dfic->cancelled = FALSE;
    dfic->provider = provider;
    dfic->dc.request_type = GET_FILE_INFO;
    dfic->update_complete = g_closure_ref(update_complete);
    dfic->file = g_object_ref(file);
    
    nautilus_dropbox_command_request(cvs, (DropboxCommand *) dfic);
    
    *handle = (NautilusOperationHandle *) dfic;
    
    return NAUTILUS_OPERATION_IN_PROGRESS;
  }
}


gboolean
nautilus_dropbox_finish_file_info_command(DropboxFileInfoCommandResponse *dficr) {
  if (dficr->dfic->cancelled == FALSE) {
    GString *status_lookup, *status= NULL, *options_lookup, *options=NULL;
    
    status_lookup = g_string_new("status");
    options_lookup = g_string_new("options");

    /* if the file status command went okay */
    if ((dficr->file_status_response != NULL &&
	(status =
	 g_hash_table_lookup(dficr->file_status_response, status_lookup)) != NULL) &&
	(dficr->context_options_response != NULL &&
	 (options =
	  g_hash_table_lookup(dficr->context_options_response,
			      options_lookup)) != NULL)) {
	
      /* set the emblem */
      {
	int emblem_code = 0;
	
	if (strcmp("up to date", status->str) == 0) {
	  emblem_code = 1;
	}
	else if (strcmp("syncing", status->str) == 0) {
	  emblem_code = 2;
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
      
      /* save the context menu options */
      {
	/* great now we have to parse these freaking optionssss also make the menu items */
	/* this is where python really makes things easy */
	GHashTable *context_option_hash;
	gchar **options_list;
	int i;
	
	context_option_hash = g_hash_table_new_full((GHashFunc) g_str_hash,
						    (GEqualFunc) g_str_equal,
						    g_free, menu_item_free);
	
	options_list = g_strsplit(options->str, "|", 0);
	
	for (i = 0; options_list[i] != NULL; i++) {
	  gchar **option_info;
	  
	  option_info = g_strsplit(options_list[i], "~", 3);
	  /* if this is a valid string */
	  if (option_info[0] != NULL && option_info[1] != NULL &&
	      option_info[2] != NULL && option_info[3] == NULL) {
	    DropboxContextMenuItem *dcmi = g_new0(DropboxContextMenuItem, 1);
	    
	    dcmi->title = g_strdup(option_info[0]);	  
	    dcmi->tooltip = g_strdup(option_info[1]);
	    dcmi->verb = g_strdup(option_info[2]);
	    
	    g_hash_table_insert(context_option_hash, g_strdup(dcmi->verb), dcmi);
	  }
	  
	  g_strfreev(option_info);
	}
	
	g_object_set_data_full(G_OBJECT(dficr->dfic->file),
			       "nautilus_dropbox_menu_item",
			       context_option_hash,
			       (GDestroyNotify) g_hash_table_destroy);
	
	g_strfreev(options_list);
	
	/* lol that wasn't so bad, glib is a big help */
      }    

      /* finally complete the request */
      nautilus_info_provider_update_complete_invoke(dficr->dfic->update_complete,
						    dficr->dfic->provider,
						    (NautilusOperationHandle*) dficr->dfic,
						    NAUTILUS_OPERATION_COMPLETE);
    }
    else {
      /* operation failed, for some reason... */
      nautilus_info_provider_update_complete_invoke(dficr->dfic->update_complete,
						    dficr->dfic->provider,
						    (NautilusOperationHandle*) dficr->dfic,
						    NAUTILUS_OPERATION_FAILED);
    }
    
    g_string_free(status_lookup, TRUE);
    g_string_free(options_lookup, TRUE);
  }

  /* destroy the objects we created */
  if (dficr->file_status_response != NULL)
    g_hash_table_destroy(dficr->file_status_response);
  if (dficr->context_options_response != NULL)
    g_hash_table_destroy(dficr->context_options_response);

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

/*
  this is the context options plan:
  1. for each file, whenever we get a file we get the context options too
  2. when the context options are needed for a group of files or file
  we take the intersection of all the context options
  3. when a action is needed to be executed on a group of files:
  create the menu/menuitmes and install a responder that takes 
  the group and then sends the command to dropbox_command
  4. the dropbox command "action", takes a list of paths and an action to do
  (needs to be implemented on the server)
*/
static void
menu_item_cb(NautilusMenuItem *item,
	     NautilusDropbox *cvs) {
  gchar *verb;
  GList *files;
  DropboxGeneralCommand *dcac = g_new(DropboxGeneralCommand, 1);

  /* maybe these would be better passed in a container
     struct used as the userdata pointer, oh well this
     is how dave camp does it */
  files = g_object_get_data(G_OBJECT(item), "nautilus_dropbox_files");
  verb = g_object_get_data(G_OBJECT(item), "nautilus_dropbox_verb");

  dcac->dc.request_type = GENERAL_COMMAND;

  /* build the argument list */
  dcac->command_args = g_hash_table_new_full((GHashFunc) g_string_hash,
					     (GEqualFunc) g_string_equal,
					     (GDestroyNotify) destroy_string,
					     (GDestroyNotify) destroy_string);
  {
    GList *li;
    int i;
    gchar **str_list = g_new0(gchar *, g_list_length(files)+1);
    gchar *conc_string;
    for (li = files, i = 0; li != NULL; li = g_list_next(li), i++) {
      char *path =
	g_filename_from_uri(nautilus_file_info_get_uri(NAUTILUS_FILE_INFO(li->data)),
			    NULL, NULL);
      str_list[i] = g_strdup(path);//g_strescape(path, NAUTILUS_DROPBOX_STR_EXCEPTIONS);
    }
    str_list[i] = NULL;
    
    conc_string = g_strjoinv("\t", str_list);

    g_hash_table_insert(dcac->command_args,
			g_string_new("paths"),
			g_string_new(conc_string));
    
    g_free(conc_string);
    g_strfreev(str_list);
  }

  g_hash_table_insert(dcac->command_args,
		      g_string_new("verb"),
		      g_string_new(verb));

  dcac->command_name = g_strdup("icon_overlay_context_action");
  dcac->handler = NULL;
  dcac->handler_ud = NULL;

  nautilus_dropbox_command_request(cvs, (DropboxCommand *) dcac);
}

static GList *
nautilus_dropbox_get_file_items(NautilusMenuProvider *provider,
                                GtkWidget            *window,
				GList                *files) {
  GList *toret = NULL;
  GList *li;
  GHashTable *set;

  /* we only do options for single files... for now */
  if (g_list_length(files) != 1) {
    return NULL;
  }

  set = g_hash_table_new((GHashFunc) g_str_hash,
			 (GEqualFunc) g_str_equal);

  /* first seed the set with the first items options */  
  {
    GHashTableIter iter;
    GHashTable *initialset;
    gchar *key;
    DropboxContextMenuItem *dcmi;
    initialset = (GHashTable *) g_object_get_data(G_OBJECT(files->data),
						  "nautilus_dropbox_menu_item");

    /* if a single file isn't a dropbox file
       we don't want it */
    if (initialset == NULL) {
      return NULL;
    }

    g_hash_table_iter_init(&iter, initialset);
    while (g_hash_table_iter_next(&iter, (gpointer) &key,
				  (gpointer) &dcmi)) {
      g_hash_table_insert(set, key, dcmi);
    }
  }

  /* need to do the set intersection of all the menu options */
  /* THIS IS EFFECTIVELY IGNORED, we only do options for single files... for now */
  for (li = g_list_next(files); li != NULL; li = g_list_next(li)) {
    GHashTableIter iter;
    GHashTable *fileset;
    gchar *key;
    DropboxContextMenuItem *dcmi;
    GList *keys_to_remove = NULL, *li2;
    
    fileset = (GHashTable *) g_object_get_data(G_OBJECT(li->data),
					       "nautilus_dropbox_menu_item");
    /* check if all the values in set are in the
       this fileset, if they are then keep that file in the set
       if not, then remove that file from the set */
    g_hash_table_iter_init(&iter, set);      
    while (g_hash_table_iter_next(&iter, (gpointer) &key,
				  (gpointer) &dcmi)) {
      if (g_hash_table_lookup(fileset, key) == NULL) {
	keys_to_remove = g_list_append(keys_to_remove, key);
      }
    }

    /* now actually remove, since we can't in the iterator */
    for (li2 = keys_to_remove; li2 != NULL; li2 = g_list_next(li2)) {
      g_hash_table_remove(set, li2->data);
    }

    g_list_free(keys_to_remove);
  }

  /* if the hash table is empty, don't show options */
  if (g_hash_table_size(set) == 0) {
    return NULL;
  }

  /* build the menu */
  {
    NautilusMenuItem *root_item;
    NautilusMenu *root_menu;
    GHashTableIter iter;
    gchar *key;
    DropboxContextMenuItem *dcmi;

    root_menu = nautilus_menu_new();
    root_item = nautilus_menu_item_new("NautilusDropbox::root_item",
				       "Dropbox", "Dropbox Options", NULL);
    nautilus_menu_item_set_submenu(root_item, root_menu);

    toret = g_list_append(toret, root_item);

    g_hash_table_iter_init(&iter, set);      
    while (g_hash_table_iter_next(&iter, (gpointer) &key,
				  (gpointer) &dcmi)) {
      NautilusMenuItem *item;
      GString *new_action_string;
      
      new_action_string = g_string_new("NautilusDropbox::");
      g_string_append(new_action_string, dcmi->verb);

      item = nautilus_menu_item_new(new_action_string->str,
				    dcmi->title,
				    dcmi->tooltip, NULL);

      nautilus_menu_append_item(root_menu, item);
      /* add the file metadata to this item */
      g_object_set_data_full (G_OBJECT(item), "nautilus_dropbox_files", 
			      nautilus_file_info_list_copy (files),
			      (GDestroyNotify) nautilus_file_info_list_free);
      /* add the verb metadata */
      g_object_set_data_full (G_OBJECT(item), "nautilus_dropbox_verb", 
			      g_strdup(dcmi->verb),
			      (GDestroyNotify) g_free);
      g_signal_connect (item, "activate", G_CALLBACK (menu_item_cb), provider);

      /* taken from nautilus-file-repairer (http://repairer.kldp.net/):
       * this code is a workaround for a bug of nautilus
       * See: http://bugzilla.gnome.org/show_bug.cgi?id=508878 */
      if (dropbox_use_nautilus_submenu_workaround) {
	toret = g_list_append(toret, item);
      }

      g_object_unref(item);
      g_string_free(new_action_string, TRUE);
    }

    g_object_unref(root_menu);
    g_hash_table_destroy(set);
    
    return toret;
  }
}

void
nautilus_dropbox_on_connect(NautilusDropbox *cvs) {
  DropboxGeneralCommand *dgc = g_new(DropboxGeneralCommand, 1);
  
  dgc->dc.request_type = GENERAL_COMMAND;
  dgc->command_name = g_strdup("icon_overlay_init");
  dgc->command_args = NULL;
  dgc->handler = NULL;
  dgc->handler_ud = NULL;
  
  nautilus_dropbox_command_request(cvs, (DropboxCommand *) dgc);
}

void
nautilus_dropbox_on_disconnect(NautilusDropbox *cvs) {
  GList *li;
  
  for (li = cvs->file_store; li != NULL; li = g_list_next(li)) {
    nautilus_file_info_invalidate_extension_info(NAUTILUS_FILE_INFO(li->data));
  }
}

static void
nautilus_dropbox_menu_provider_iface_init (NautilusMenuProviderIface *iface) {
  iface->get_file_items = nautilus_dropbox_get_file_items;
  return;
}

static void
nautilus_dropbox_info_provider_iface_init (NautilusInfoProviderIface *iface) {
  iface->update_file_info = nautilus_dropbox_update_file_info;
  iface->cancel_update = nautilus_dropbox_cancel_update;
  return;
}

static void
nautilus_dropbox_instance_init (NautilusDropbox *cvs) {
  /* this data is shared by all submodules */
  cvs->ca.user_quit = FALSE;
  cvs->ca.dropbox_starting = FALSE;
  cvs->file_store = NULL;
  cvs->dispatch_table = g_hash_table_new((GHashFunc) g_str_hash,
					 (GEqualFunc) g_str_equal);

  /* setup our different submodules */
  nautilus_dropbox_hooks_setup(cvs);
  nautilus_dropbox_command_setup(cvs);
  nautilus_dropbox_tray_setup(cvs);

  /* now start up the two connections */
  nautilus_dropbox_hooks_start(cvs);
  nautilus_dropbox_command_start(cvs);

  return;
}

static void
nautilus_dropbox_class_init (NautilusDropboxClass *class) {
}

static void
nautilus_dropbox_class_finalize (NautilusDropboxClass *class) {
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
