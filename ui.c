/*
 * ui.c
 *
 *  Created on: Dec 9, 2012
 *  Author: scorp
 */
#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>

#include "ui.h"
#include "common-defs.h"
#include "gtk_compat.h"


// vkontakte.c
void vk_add_track_from_tree_model_to_playlist (GtkTreeModel *treestore, GtkTreeIter *iter);
void vk_search_music (const gchar *query_text, GtkListStore *liststore);
void vk_get_my_music (GtkTreeModel *liststore);

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

static void
add_to_playlist (GtkTreeModel *treemodel, GtkTreePath *path) {
	GtkTreeIter treeiter;

	if (gtk_tree_model_get_iter(treemodel, &treeiter, path)) {
		vk_add_track_from_tree_model_to_playlist(treemodel, &treeiter);
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

	add_to_playlist(model, path);
}

static void
on_menu_item_add_to_playlist(GtkWidget *menuItem, gpointer userdata) {
    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    GtkTreeModel *treemodel;
    GList *selected_rows, *i;

    treeview = GTK_TREE_VIEW (userdata);
    selection = gtk_tree_view_get_selection (treeview);
    selected_rows = gtk_tree_selection_get_selected_rows (selection, &treemodel);

    i = g_list_last (selected_rows);
    while (i) {
        add_to_playlist (treemodel, (GtkTreePath *) i->data);
        i = g_list_previous (i);
    }

    g_list_free (selected_rows);
}

static void
show_popup_menu(GtkTreeView *treeview, GdkEventButton *event) {
    GtkWidget *menu, *item;

    menu = gtk_menu_new ();

    item = gtk_menu_item_new_with_label ("Add to playlist");
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_item_add_to_playlist), treeview);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

//    item = gtk_menu_item_new_with_label ("Clear playlist")
//    gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);

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

    const gchar *query_text = gtk_entry_get_text (GTK_ENTRY (widget) );
    vk_search_music (query_text, GTK_LIST_STORE (data));

    gtk_widget_set_sensitive (widget, TRUE);
    gtk_widget_grab_focus (widget);
}

static void
on_my_music (GtkWidget *widget, gpointer *data) {
    gtk_widget_set_sensitive (widget, FALSE);

    vk_get_my_music (GTK_TREE_MODEL (data));

    gtk_widget_set_sensitive (widget, TRUE);
}

GtkWidget *
vk_create_add_tracks_dlg () {
    GtkWidget *dlg;
    GtkWidget *dlg_vbox;
    GtkWidget *scroll_window;
    GtkWidget *search_text;
    GtkWidget *search_results;
    GtkListStore *list_store;
    GtkCellRenderer *artist_cell;
    GtkCellRenderer *title_cell;
    GtkTreeSelection *selection;
    GtkWidget *bottom_hbox;
    GtkWidget *my_music_button;
    int col_i;

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
                                     G_TYPE_STRING );   // URL, not rendered

    search_text = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (dlg_vbox), search_text, FALSE, FALSE, 0);
    gtk_widget_show (search_text);
    g_signal_connect(search_text, "activate", G_CALLBACK (on_search), list_store);

    GtkCellRenderer *duration_cell;
    search_results = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store) );
    artist_cell = gtk_cell_renderer_text_new ();
    title_cell = gtk_cell_renderer_text_new ();
    duration_cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (search_results), -1, "Artist",
                                                 artist_cell, "text", ARTIST_COLUMN, NULL );
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (search_results), -1, "Title",
                                                 title_cell, "text", TITLE_COLUMN, NULL );
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (search_results), -1, "Duration",
                                                 duration_cell, "text", DURATION_COLUMN, NULL );
    // allow column resize & sort
    GList *columns;
    GList *i;
    columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (search_results) );
    i = g_list_first (columns);
    col_i = 0;
    while (i) {
        GtkTreeViewColumn *col;
        col = GTK_TREE_VIEW_COLUMN (i->data);
        gtk_tree_view_column_set_resizable (col, TRUE);
        gtk_tree_view_column_set_sort_column_id (col, col_i++);

        i = g_list_next (i);
    }
    g_list_free (columns);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (search_results));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

    g_signal_connect(search_results, "row-activated", G_CALLBACK (on_search_results_row_activate), NULL);
    g_signal_connect(search_results, "popup-menu", G_CALLBACK(on_search_results_popup_menu), NULL);
    g_signal_connect(search_results, "button-press-event", G_CALLBACK(on_search_results_button_press), NULL);

    scroll_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scroll_window), search_results);
    gtk_box_pack_start (GTK_BOX (dlg_vbox), scroll_window, TRUE, TRUE, 12);

    bottom_hbox = gtk_hbox_new (FALSE, 12);
    gtk_box_pack_start (GTK_BOX (dlg_vbox), bottom_hbox, FALSE, TRUE, 0);

    my_music_button = gtk_button_new_with_label ("My music");
    g_signal_connect (my_music_button, "clicked", G_CALLBACK (on_my_music), list_store);
    gtk_box_pack_start (GTK_BOX (bottom_hbox), my_music_button, FALSE, FALSE, 0);

    gtk_widget_show_all (dlg);

    return dlg;
}

