/*
 * ui.c
 *
 *  Created on: Dec 9, 2012
 *  Author: scorp
 */
#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>
#include <string.h>

#include "ui.h"
#include "common-defs.h"
#include "gtk_compat.h"


const gchar *last_search_query = NULL;

typedef void (*TreeSelectionIterCallback) (GtkTreeModel*, GtkTreePath*, gpointer user_data);

typedef struct {
    gchar *url;
    gchar *filename;
} DownloadTrack;


// vkontakte.c
void vk_add_track_from_tree_model_to_playlist (GtkTreeModel *treestore, GtkTreeIter *iter);
void vk_search_music (const gchar *query_text, GtkListStore *liststore);
void vk_get_my_music (GtkTreeModel *liststore);
void vk_ddb_set_config_var (const char *key, GValue *value);

gboolean
show_message (GtkMessageType messageType, const gchar *message) {
    GtkWidget *dlg;

    dlg =  gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   messageType,
                                   GTK_BUTTONS_OK,
                                   "%s",
                                   message);
    g_signal_connect_swapped (dlg, "response", G_CALLBACK (gtk_widget_destroy), dlg);
    gtk_dialog_run (GTK_DIALOG (dlg) );
    return FALSE;
}

/**
 * Handler for various search-affecting controls that would trigger search again if needed.
 */
static void
maybe_do_search_again (GtkWidget *widget, gpointer data) {
    if (last_search_query == NULL) {
        return;
    }

    const gchar *current_query = gtk_entry_get_text (GTK_ENTRY (data));

    // don't care about encodings - if it's the same string there would be zero
    if (strcmp (last_search_query, current_query) == 0) {
        // if search query wasn't changed, emit search to refresh results
        g_signal_emit_by_name (data, "activate");
    }
}

/**
 * Opens file choose dialog and returns selected directory path
 * if user confirmed selection. NULL otherwise.
 *
 * @return Directory path or NULL. Free with g_free()
 */
gchar *
open_directory_selection_dialog () {
    GtkWidget *dlg;

    dlg = gtk_file_chooser_dialog_new ("Save to",
                                       NULL,
                                       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_OPEN, GTK_RESPONSE_OK,
                                       NULL);
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK) {
        gtk_widget_destroy (dlg);
        return gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
    } else {
        gtk_widget_destroy (dlg);
        return NULL;
    }
}

static void
save_active_property_value_to_config (GtkWidget *widget, gpointer data) {
    GValue value = G_VALUE_INIT;

    if (GTK_IS_COMBO_BOX (widget)) {
        g_value_init (&value, G_TYPE_INT);
    } else if (GTK_IS_CHECK_BUTTON (widget)) {
        g_value_init (&value, G_TYPE_BOOLEAN);
    } else {
        trace ("FATAL: %s unsupported widget type\n", __FUNCTION__);
    }

    g_object_get_property (G_OBJECT (widget), "active", &value);
    vk_ddb_set_config_var ((const gchar *) data, &value);
}

static void
tree_view_foreach_selected (GtkTreeView *tree_view, TreeSelectionIterCallback callback, gpointer user_data) {
    GtkTreeSelection *selection;
    GtkTreeModel *tree_model;
    GList *selected_rows, *i;

    selection = gtk_tree_view_get_selection (tree_view);
    selected_rows = gtk_tree_selection_get_selected_rows (selection, &tree_model);

    i = g_list_first (selected_rows);
    while (i) {
        callback (tree_model, (GtkTreePath *) i->data, user_data);
        i = g_list_next (i);
    }
    g_list_free (selected_rows);
}

static void
add_to_playlist (GtkTreeModel *tree_model, GtkTreePath *path, gpointer user_data) {
	GtkTreeIter treeiter;

	if (gtk_tree_model_get_iter(tree_model, &treeiter, path)) {
		vk_add_track_from_tree_model_to_playlist(tree_model, &treeiter);
	} else {
		trace("gtk_tree_model_get_iter failed, %s:%d", __FILE__, __LINE__);
	}
}

static void
on_search_results_row_activate (GtkTreeView *tree_view,
                                GtkTreePath *path,
                                GtkTreeViewColumn *column,
                                gpointer user_data) {
    GtkTreeModel *model;

	model = gtk_tree_view_get_model(tree_view);

	add_to_playlist(model, path, NULL);
}

static void
on_menu_item_add_to_playlist (GtkWidget *menuItem, gpointer userdata) {
    tree_view_foreach_selected (GTK_TREE_VIEW (userdata), add_to_playlist, NULL);
}

static void
append_track_url_to_buffer (GtkTreeModel *tree_model, GtkTreePath *tree_path, gpointer user_data) {
    GtkTreeIter iter;
    GString *urls_buf;
    gchar *track_url;

    urls_buf = (GString*) user_data;
    gtk_tree_model_get_iter (tree_model, &iter, tree_path);
    gtk_tree_model_get (tree_model, &iter,
                        URL_COLUMN, &track_url,
                        -1);
    g_string_append (urls_buf, track_url);
    g_string_append (urls_buf, "\n");
    g_free (track_url);
}

static void
on_menu_item_copy_url (GtkWidget *menuItem, gpointer user_data) {
    GString *urls_buf;

    urls_buf = g_string_sized_new(500);
    tree_view_foreach_selected(GTK_TREE_VIEW (user_data), append_track_url_to_buffer, urls_buf);

    gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), urls_buf->str, urls_buf->len);

    g_string_free(urls_buf, TRUE);
}

static void
append_download_track (GtkTreeModel *tree_model, GtkTreePath *tree_path, gpointer user_data) {
    GtkTreeIter iter;
    DownloadTrack *dl_track;
    GList** dl_list;
    gchar *artist_name, *track_title;

    dl_list = user_data;
    dl_track = g_malloc (sizeof (DownloadTrack));
    gtk_tree_model_get_iter (tree_model, &iter, tree_path);
    gtk_tree_model_get (tree_model, &iter,
                        URL_COLUMN, &dl_track->url,
                        ARTIST_COLUMN, &artist_name,
                        TITLE_COLUMN, &track_title,
                        -1);

    dl_track->filename = g_strdup_printf ("%s - %s", artist_name, track_title);
    *dl_list = g_list_append (*dl_list, dl_track);

    g_free (artist_name);
    g_free (track_title);
}

static void
on_menu_item_download (GtkWidget *menuItem, gpointer user_data) {
    GList *to_download;
    gchar *dl_target_dir;

    to_download = NULL;
    tree_view_foreach_selected(GTK_TREE_VIEW (user_data), append_download_track, &to_download);
    if (to_download == NULL)
        return; // Nothing selected

    dl_target_dir = open_directory_selection_dialog ();
    if (dl_target_dir == NULL)
        return; // Canceled by user

/* debug */
    GList *i;
    i = g_list_first (to_download);
    while (i) {
        DownloadTrack *dl_track;
        dl_track = i->data;
        trace ("%s <- %s\n", dl_track->filename, dl_track->url);
        i = g_list_next (i);
    }
/****/

    // download tracks
    g_list_free (to_download);
}

static void
show_popup_menu (GtkTreeView *treeview, GdkEventButton *event) {
    GtkWidget *menu, *item;

    menu = gtk_menu_new ();

    item = gtk_menu_item_new_with_label ("Add to playlist");
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_item_add_to_playlist), treeview);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_menu_item_new_with_label ("Copy URL(s)");
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_item_copy_url), treeview);
    gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label ("Download track(s)");
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_item_download), treeview);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    gtk_widget_show_all (menu);
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0,
                    gdk_event_get_time ((GdkEvent *) event));
}

static gboolean
on_search_results_button_press (GtkTreeView *treeview, GdkEventButton *event, gpointer userdata) {
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkTreeSelection *selection;

        selection = gtk_tree_view_get_selection (treeview);
        if (gtk_tree_selection_count_selected_rows (selection) <= 1) {
            GtkTreePath *path;
            if (gtk_tree_view_get_path_at_pos (treeview, event->x, event->y, &path, NULL, NULL, NULL )) {
                gtk_tree_selection_unselect_all(selection);
                gtk_tree_selection_select_path (selection, path);
                gtk_tree_path_free (path);
            }
        }

        show_popup_menu (treeview, event);
        return TRUE;
    }
    return FALSE;
}

static gboolean
on_search_results_popup_menu (GtkTreeView *treeview, gpointer userdata) {
    show_popup_menu(treeview, NULL);
    return TRUE;
}

static void
on_search (GtkWidget *widget, gpointer data) {
    gtk_widget_set_sensitive (widget, FALSE);

    const gchar *query_text = gtk_entry_get_text (GTK_ENTRY (widget));

    // refresh last search query
    if (last_search_query != NULL) {
        g_free ((gchar*) last_search_query);
    }
    last_search_query = g_strdup (query_text);

    gtk_list_store_clear (GTK_LIST_STORE (data));
    vk_search_music (query_text, GTK_LIST_STORE (data));

    gtk_widget_set_sensitive (widget, TRUE);
    gtk_widget_grab_focus (widget);
}

static void
on_my_music (GtkWidget *widget, gpointer *data) {
    gtk_widget_set_sensitive (widget, FALSE);

    last_search_query = NULL;
    gtk_list_store_clear (GTK_LIST_STORE (data));
    vk_get_my_music (GTK_TREE_MODEL (data));

    gtk_widget_set_sensitive (widget, TRUE);
}

static void
on_filter_duplicates (GtkWidget *widget, gpointer *data) {
    vk_search_opts.filter_duplicates = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void
on_whole_phrase_search (GtkWidget *widget, gpointer *data) {
    vk_search_opts.search_whole_phrase = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void
on_search_target_changed (GtkWidget *widget, gpointer *data) {
    vk_search_opts.search_target = gtk_combo_box_get_active( GTK_COMBO_BOX (widget));
}

GtkWidget *
vk_create_add_tracks_dlg () {
    GtkWidget *dlg;
    GtkWidget *dlg_vbox;
    GtkWidget *scroll_window;
    GtkWidget *search_hbox;
    GtkWidget *search_text;
    GtkWidget *search_target;
    GtkWidget *search_results;
    GtkListStore *list_store;
    GtkCellRenderer *artist_cell;
    GtkCellRenderer *title_cell;
    GtkWidget *bottom_hbox;
    GtkWidget *my_music_button;
    GtkWidget *filter_duplicates;
    GtkWidget *search_whole_phrase;

    dlg = gtk_dialog_new ();
    gtk_container_set_border_width (GTK_CONTAINER (dlg), 12);
    gtk_window_set_default_size (GTK_WINDOW (dlg), 840, 400);
    gtk_window_set_title (GTK_WINDOW (dlg), "Search tracks");
    gtk_window_set_type_hint (GTK_WINDOW (dlg), GDK_WINDOW_TYPE_HINT_DIALOG);

    dlg_vbox = gtk_dialog_get_content_area (GTK_DIALOG (dlg) );

    list_store = gtk_list_store_new (N_COLUMNS,
                                     G_TYPE_STRING,     // ARTIST
                                     G_TYPE_STRING,     // TITLE
                                     G_TYPE_INT,        // DURATION seconds, not rendered
                                     G_TYPE_STRING,     // DURATION_FORMATTED
                                     G_TYPE_STRING );   // URL, not rendered

    search_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start (GTK_BOX (dlg_vbox), search_hbox, FALSE, FALSE, 0);

    search_text = gtk_entry_new ();
    gtk_widget_show (search_text);
    g_signal_connect(search_text, "activate", G_CALLBACK (on_search), list_store);
    gtk_box_pack_start (GTK_BOX (search_hbox), search_text, TRUE, TRUE, 0);

    search_target = gtk_combo_box_text_new ();
    // must to order of VkSearchTarget entries
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (search_target), "Anywhere");
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (search_target), "Artist");
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (search_target), "Title");
    gtk_combo_box_set_active (GTK_COMBO_BOX (search_target), vk_search_opts.search_target);
    g_signal_connect (search_target, "changed", G_CALLBACK (on_search_target_changed), NULL);
    gtk_box_pack_start (GTK_BOX (search_hbox), search_target, FALSE, FALSE, 0);

    GtkCellRenderer *duration_cell;
    search_results = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));
    artist_cell = gtk_cell_renderer_text_new ();
    title_cell = gtk_cell_renderer_text_new ();
    duration_cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (search_results), -1, "Artist",
                                                 artist_cell, "text", ARTIST_COLUMN, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (search_results), -1, "Title",
                                                 title_cell, "text", TITLE_COLUMN, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (search_results), -1, "Duration",
                                                 duration_cell, "text", DURATION_FORMATTED_COLUMN, NULL);

    //// Setup columns
    GtkTreeViewColumn *col;
    // artist col is resizeable and sortable
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (search_results), 0);
    gtk_tree_view_column_set_resizable (col, TRUE);
    gtk_tree_view_column_set_sort_column_id (col, 0);
    // title col is resizeable, sortable and expanded
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (search_results), 1);
    gtk_tree_view_column_set_resizable (col, TRUE);
    gtk_tree_view_column_set_expand (col, TRUE);
    gtk_tree_view_column_set_sort_column_id (col, 1);
    // duration col is sortable and fixed width
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (search_results), 2);
    gtk_tree_view_column_set_min_width(col, 10);
    gtk_tree_view_column_set_max_width(col, 70);
    gtk_tree_view_column_set_fixed_width (col, 50);
    gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_sort_column_id (col, 2);


    gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (search_results)),
                                 GTK_SELECTION_MULTIPLE);

    g_signal_connect(search_results, "row-activated", G_CALLBACK (on_search_results_row_activate), NULL);
    g_signal_connect(search_results, "popup-menu", G_CALLBACK(on_search_results_popup_menu), NULL);
    g_signal_connect(search_results, "button-press-event", G_CALLBACK(on_search_results_button_press), NULL);

    scroll_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scroll_window), search_results);
    gtk_box_pack_start (GTK_BOX (dlg_vbox), scroll_window, TRUE, TRUE, 12);

    bottom_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start (GTK_BOX (dlg_vbox), bottom_hbox, FALSE, TRUE, 0);

    my_music_button = gtk_button_new_with_label ("My music");
    g_signal_connect (my_music_button, "clicked", G_CALLBACK (on_my_music), list_store);
    gtk_box_pack_start (GTK_BOX (bottom_hbox), my_music_button, FALSE, FALSE, 0);

    filter_duplicates = gtk_check_button_new_with_label ("Filter duplicates");
    gtk_widget_set_tooltip_text (filter_duplicates, "When checked removes duplicates during next search");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filter_duplicates), vk_search_opts.filter_duplicates);
    g_signal_connect (filter_duplicates, "clicked", G_CALLBACK (on_filter_duplicates), list_store);
    gtk_box_pack_start (GTK_BOX (bottom_hbox), filter_duplicates, FALSE, FALSE, 0);

    search_whole_phrase = gtk_check_button_new_with_label ("Whole phrase");
    gtk_widget_set_tooltip_text (search_whole_phrase,
                                 "Searching 'foo bar' would match exactly what you typed and not just 'foo' or 'bar'");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (search_whole_phrase), vk_search_opts.search_whole_phrase);
    g_signal_connect (search_whole_phrase, "clicked", G_CALLBACK (on_whole_phrase_search), list_store);
    gtk_box_pack_start (GTK_BOX (bottom_hbox), search_whole_phrase, FALSE, FALSE, 0);

    // refresh results when search criteria changed
    g_signal_connect (search_target, "changed", G_CALLBACK (maybe_do_search_again), search_text);
    g_signal_connect (filter_duplicates, "clicked", G_CALLBACK (maybe_do_search_again), search_text);
    g_signal_connect (search_whole_phrase, "clicked", G_CALLBACK (maybe_do_search_again), search_text);
    // save controls state to config
    g_signal_connect (search_target, "changed",
                      G_CALLBACK (save_active_property_value_to_config), CONF_VK_UI_TARGET);
    g_signal_connect (filter_duplicates, "clicked",
                      G_CALLBACK (save_active_property_value_to_config), CONF_VK_UI_DEDUP);
    g_signal_connect (search_whole_phrase, "clicked",
                      G_CALLBACK (save_active_property_value_to_config), CONF_VK_UI_WHOLE_PHRASE);

    gtk_widget_show_all (dlg);
    return dlg;
}
