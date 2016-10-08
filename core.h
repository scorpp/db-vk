
#ifndef DB_VK_CORE_H
#define DB_VK_CORE_H

#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>


#define VK_AUTH_APP_ID "3035566"
#define VK_AUTH_REDIR_URL "http://scorpp.github.io/db-vk/vk-id.html"
#define VK_AUTH_URL "http://oauth.vkontakte.ru/authorize?client_id=" VK_AUTH_APP_ID \
        "&scope=audio,friends,groups,offline&redirect_uri=" VK_AUTH_REDIR_URL \
        "&response_type=token"

// deadbeef config keys
#define CONF_VK_AUTH_URL "vk.auth.url"
#define CONF_VK_AUTH_DATA "vk.auth.data"


/// Plugin lifecycle

gboolean vk_action_gtk (void *data);

ddb_gtkui_widget_t *w_vkbrowser_create ();

void vk_config_changed ();

DB_FILE *vk_vfs_open (const gchar *fname);

void vk_initialise (DB_functions_t *deadbeef_instance, ddb_gtkui_t *gtkui_plugin);

void vk_perform_cleanup ();

/// UI backend

void vk_add_tracks_from_tree_model_to_playlist (GtkTreeModel *treemodel,
                                                GList *gtk_tree_path_list,
                                                const char *plt_name);

void vk_search_music (const gchar *query_text, GtkListStore *liststore);

void vk_get_my_music (GtkListStore *liststore);

void vk_get_recommended_music (GtkListStore *liststore);

void vk_set_config_var (const char *key, GValue *value);

#endif //DB_VK_CORE_H
