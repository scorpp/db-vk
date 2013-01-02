/*
 * ui.h
 *
 *  Created on: Dec 9, 2012
 *      Author: scorp
 */

#ifndef UI_H_
#define UI_H_

#include <glib.h>


/**
 * List view columns.
 */
enum {
    ARTIST_COLUMN = 0,
    TITLE_COLUMN,
    DURATION_COLUMN,
    URL_COLUMN,
    N_COLUMNS
};


struct {
    gboolean filter_duplicates;
} vk_search_opts;


gboolean        show_message (GtkMessageType messageType, const gchar *message);
GtkWidget *     vk_create_add_tracks_dlg ();

#endif /* UI_H_ */
