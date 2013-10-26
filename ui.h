/*
 * ui.h
 *
 *  Created on: Dec 9, 2012
 *      Author: scorp
 */

#ifndef UI_H_
#define UI_H_

#include <glib.h>

G_BEGIN_DECLS

/**
 * List view columns.
 */
enum {
    ARTIST_COLUMN = 0,
    TITLE_COLUMN,
    DURATION_COLUMN,
    DURATION_FORMATTED_COLUMN,
    URL_COLUMN,
    N_COLUMNS
};

typedef enum {
    VK_TARGET_ANY_FIELD = 0,
    VK_TARGET_ARTIST_FIELD,
    VK_TARGET_TITLE_FIELD
} VkSearchTarget;

struct {
    gboolean filter_duplicates;
    gboolean search_whole_phrase;
    VkSearchTarget search_target;
} vk_search_opts;

#define CONF_VK_UI_DEDUP "vk.ui.filter.duplicates"
#define CONF_VK_UI_WHOLE_PHRASE "vk.ui.whole.phrase"
#define CONF_VK_UI_TARGET "vk.uk.target"


gboolean        show_message (GtkMessageType messageType, const gchar *message);
GtkWidget *     vk_create_browser_dialogue ();
void            vk_setup_browser_widget (ddb_gtkui_widget_t *w);

G_END_DECLS
#endif /* UI_H_ */
