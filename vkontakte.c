// disable gdk_thread_enter\leave warnings
#define GDK_VERSION_MIN_REQUIRED GDK_VERSION_3_4

#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>
#include <json-glib/json-glib.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "common-defs.h"
#include "vk-api.h"
#include "ui.h"

DB_functions_t *deadbeef;
ddb_gtkui_t *gtkui_plugin;
DB_vfs_t *vfs_curl_plugin;

VkAuthData *vk_auth_data = NULL;

static intptr_t http_tid;    // thread for communication

typedef struct {
    const gchar *query;
    GtkTreeModel *store;
} SearchQuery;

#define VK_AUTH_APP_ID "3035566"
#define VK_AUTH_REDIR_URL "http://scorpp.github.io/db-vk/vk-id.html"
#define VK_AUTH_URL "http://oauth.vkontakte.ru/authorize?client_id=" VK_AUTH_APP_ID \
        "&scope=audio,friends,offline&redirect_uri=" VK_AUTH_REDIR_URL \
        "&response_type=token"

// URL formatting strings
#define VK_URL_LEN          300
#define VK_AUDIO_GET        VK_API_METHOD_AUDIO_GET "?access_token=%s"
#define VK_AUDIO_GET_BY_ID  VK_API_METHOD_AUDIO_GET_BY_ID "?access_token=%s&audios=%d_%d"
#define VK_AUDIO_SEARCH     VK_API_METHOD_AUDIO_SEARCH "?access_token=%s&count=%d&offset=%d&q=%s"

// deadbeef config keys
#define CONF_VK_AUTH_URL "vk.auth.url"
#define CONF_VK_AUTH_DATA "vk.auth.data"

// Max length for VFS URL to VK track
#define VK_VFS_URL_LEN  30


// util.c
gchar *     http_get_string(const gchar *url, GError **error);

int
vk_vfs_format_track_url(char *url,
                        int aid,
                        int owner_id) {
    return sprintf (url, "vk://%d_%d", owner_id, aid);
}

/**
 * Simple deduplication.
 * @return TRUE if given track already exists, FALSE otherwise.
 */
gboolean
vk_tree_model_has_track (GtkTreeModel *treemodel, VkAudioTrack *track) {
    // if no need to filter duplicates return FALSE immediately
    if (!vk_search_opts.filter_duplicates) {
        return FALSE;
    }

    gboolean has_track;
    GtkTreeIter iter;
    gboolean valid;
    gchar *track_artist_casefolded;
    gchar *track_title_casefolded;

    has_track = FALSE;

    track_artist_casefolded = g_utf8_casefold (track->artist, -1);
    track_title_casefolded = g_utf8_casefold (track->title, -1);

    valid = gtk_tree_model_get_iter_first (treemodel, &iter);
    while (valid && !has_track) {
        gchar *model_artist;
        gchar *model_title;
        gchar *model_artist_casefolded;
        gchar *model_title_casefolded;
        gint model_duration;

        gtk_tree_model_get (treemodel, &iter,
                            ARTIST_COLUMN, &model_artist,
                            TITLE_COLUMN, &model_title,
                            DURATION_COLUMN, &model_duration,
                            -1);
        model_artist_casefolded = g_utf8_casefold (model_artist, -1);
        model_title_casefolded = g_utf8_casefold (model_title, -1);

        if (0 == g_utf8_collate (model_artist_casefolded, track_artist_casefolded)
                && 0 == g_utf8_collate (model_title_casefolded, track_title_casefolded)) {
            has_track = TRUE;
            trace ("Duplicate %s - %s, duration existing %d vs %d\n", track->artist, track->title,
                   model_duration, track->duration);
        }

        g_free (model_artist);
        g_free (model_title);
        g_free (model_artist_casefolded);
        g_free (model_title_casefolded);

        valid = gtk_tree_model_iter_next (treemodel, &iter);
    }

    g_free (track_artist_casefolded);
    g_free (track_title_casefolded);

    return has_track;
}

/**
 * Apply additional filters.
 * @return TRUE if track matches additional filters, FALSE otherwise.
 */
static gboolean
vk_search_filter_matches (const gchar *search_query, VkAudioTrack *track) {
    // 'My music' doesn't use search query, don't filter it
    if (!vk_search_opts.search_whole_phrase
            || search_query == NULL) {
        return TRUE;
    }

    gchar *query_casefolded = g_utf8_casefold (search_query, -1);
    gchar *title_casefolded = g_utf8_casefold (track->title, -1);
    gchar *artist_casefolded = g_utf8_casefold (track->artist, -1);

    gboolean artist_matches = strstr (artist_casefolded, query_casefolded) != 0;
    gboolean title_matches = strstr (title_casefolded, query_casefolded) != 0;

    g_free (artist_casefolded);
    g_free (title_casefolded);
    g_free (query_casefolded);

    switch (vk_search_opts.search_target) {
    case VK_TARGET_ANY_FIELD:
        return artist_matches || title_matches;
    case VK_TARGET_ARTIST_FIELD:
        return artist_matches;
    case VK_TARGET_TITLE_FIELD:
        return title_matches;
    default:
        trace ("WARN: unexpected VkSearchTraget value: %d\n", vk_search_opts.search_target);
        return TRUE;
    }
}

static void
parse_audio_track_callback (VkAudioTrack *track, guint index, gpointer userdata) {
    SearchQuery *query;
    GtkTreeIter iter;

    query = (SearchQuery *) userdata;

    if (vk_search_filter_matches (query->query, track)
            && !vk_tree_model_has_track (query->store, track)) {

        char duration_formatted[10];
        sprintf (duration_formatted, "%d:%02d", track->duration / 60, track->duration % 60);

        // write to list store
        gtk_list_store_append(GTK_LIST_STORE (query->store), &iter);
        gtk_list_store_set (GTK_LIST_STORE (query->store), &iter,
                            ARTIST_COLUMN, track->artist,
                            TITLE_COLUMN, track->title,
                            DURATION_COLUMN, track->duration,
                            DURATION_FORMATTED_COLUMN, duration_formatted,
                            URL_COLUMN, track->url,
                            AID_COLUMN, track->aid,
                            OWNER_ID_COLUMN, track->owner_id,
                            -1);
    }
}

static void
parse_audio_resp (SearchQuery *query, const gchar *resp_str) {
    GError *error;

    gdk_threads_enter ();
    if (!vk_audio_response_parse (resp_str, parse_audio_track_callback, query, &error)) {
        show_message(GTK_MESSAGE_ERROR, error->message);
        g_error_free (error);
    }
    gdk_threads_leave ();
}

void
vk_add_track_from_tree_model_to_playlist (GtkTreeModel *treestore, GtkTreeIter *iter) {
    gchar *artist;
    gchar *title;
    int duration;
    int aid;
    int owner_id;
    char url[VK_VFS_URL_LEN];
    ddb_playlist_t *plt;

    gtk_tree_model_get (treestore, iter,
                        ARTIST_COLUMN, &artist,
                        TITLE_COLUMN, &title,
                        DURATION_COLUMN, &duration,
                        AID_COLUMN, &aid,
                        OWNER_ID_COLUMN, &owner_id,
                        -1);
    vk_vfs_format_track_url (url, aid, owner_id);
    plt = deadbeef->plt_get_curr ();

    DB_playItem_t *pt;
    int pabort = 0;

    deadbeef->pl_lock();
    if (!deadbeef->pl_add_files_begin (plt)) {
        DB_playItem_t *last = deadbeef->plt_get_last (plt, 0);
        pt = deadbeef->plt_insert_file (plt, last, url, &pabort, NULL, NULL);
        deadbeef->pl_add_meta (pt, "artist", artist);
        deadbeef->pl_add_meta (pt, "title", title);
        deadbeef->plt_set_item_duration (plt, pt, duration);
        deadbeef->pl_add_files_end();
    }
    deadbeef->pl_unlock();

    deadbeef->plt_unref (plt);
    g_free (artist);
    g_free (title);
}

static void
vk_send_audio_request_and_parse_response (const gchar *url, SearchQuery *query) {
    GError *error;
    gchar *resp_str;

    resp_str = http_get_string (url, &error);
    if (NULL == resp_str) {
        trace ("VK error: %s\n", error->message);
        g_error_free (error);
    } else {
        parse_audio_resp (query, resp_str);
        g_free (resp_str);
    }
}

static void
vk_search_audio_thread_func (SearchQuery *query) {
    CURL *curl;
    gint rows_added;
    gint iteration = 0;

    curl = curl_easy_init ();

    char *escaped_search_str = curl_easy_escape (curl, query->query, 0);

    do {
        char method_url[VK_URL_LEN];

        rows_added = gtk_tree_model_iter_n_children (query->store, NULL);
        sprintf (method_url,
                 VK_AUDIO_SEARCH,
                 vk_auth_data->access_token,
                 VK_AUDIO_MAX_TRACKS,
                 VK_AUDIO_MAX_TRACKS * iteration,
                 escaped_search_str);
        vk_send_audio_request_and_parse_response (method_url, query);
        rows_added = gtk_tree_model_iter_n_children (query->store, NULL) - rows_added;
    } while (++iteration < 10 && rows_added > 0);

    g_free (escaped_search_str);
    curl_easy_cleanup (curl);
    g_free ((gchar *) query->query);
    g_free (query);
    http_tid = 0;
}

static void
vk_get_my_music_thread_func (void *ctx) {
    SearchQuery query;
    char method_url[VK_URL_LEN];

    query.query = NULL,
    query.store = GTK_TREE_MODEL (ctx);

    sprintf (method_url, VK_AUDIO_GET, vk_auth_data->access_token);
    vk_send_audio_request_and_parse_response (method_url, &query);

    http_tid = 0;
}

void
vk_ddb_set_config_var (const char *key, GValue *value) {
    if (G_VALUE_HOLDS_INT (value)) {
        deadbeef->conf_set_int (key, g_value_get_int (value));
    } else if (G_VALUE_HOLDS_INT64 (value)) {
        deadbeef->conf_set_int64 (key, g_value_get_int64 (value));
    } else if (G_VALUE_HOLDS_FLOAT (value)) {
        deadbeef->conf_set_float (key, g_value_get_float (value));
    } else if (G_VALUE_HOLDS_STRING (value)) {
        deadbeef->conf_set_str (key, g_value_get_string (value));
    } else if (G_VALUE_HOLDS_BOOLEAN (value)) {
        deadbeef->conf_set_int (key, g_value_get_boolean (value) ? 1 : 0);
    } else {
        trace ("WARN unsupported GType to vk_ddb_set_config_var: %s\n", G_VALUE_TYPE_NAME (value));
    }
}

void
vk_search_music (const gchar *query_text, GtkTreeModel *liststore) {
    SearchQuery *query;

    if (http_tid) {
        trace("Killing http thread\n");
        deadbeef->thread_detach (http_tid);

        http_tid = 0;
    }
    trace("== Searching for %s\n", query_text);

    query = g_malloc (sizeof(SearchQuery));
    query->query = g_strdup (query_text);
    query->store = liststore;

    http_tid = deadbeef->thread_start ((DB_thread_func_t) vk_search_audio_thread_func, query);
}

void
vk_get_my_music (GtkTreeModel *liststore) {
    if (http_tid) {
        trace("Killing http thread\n");
        deadbeef->thread_detach (http_tid);

        http_tid = 0;
    }
    trace("== Getting my music, uid=%d\n", vk_auth_data->user_id);

    http_tid = deadbeef->thread_start (vk_get_my_music_thread_func, liststore);
}

static ddb_gtkui_widget_t *
w_vkbrowser_create (void) {
    if (vk_auth_data == NULL) {
        // not authenticated, show warning and that's it
        // TODO
    }

    ddb_gtkui_widget_t *w = malloc (sizeof (ddb_gtkui_widget_t));
    memset (w, 0, sizeof (ddb_gtkui_widget_t));
    vk_setup_browser_widget (w);
    return w;
}

static gboolean
vk_action_gtk (void *data) {
    if (vk_auth_data == NULL) {
        // not authenticated, show warning and that's it
        gdk_threads_enter ();
        show_message (GTK_MESSAGE_WARNING,
                      "To be able to use VKontakte plugin you need to provide your\n"
                      "authentication details. Please visit plugin configuration.\n"
                      "Then you will be able to add tracks from VK.com");
        gdk_threads_leave ();
        return FALSE;
    }

    gtk_widget_show (vk_create_browser_dialogue ());
    return FALSE;
}

static const char *scheme_names[] = { "vk://", NULL };
static const char **
vk_ddb_vfs_get_schemes () {
    return scheme_names;
}

static void
vk_vfs_store_track (VkAudioTrack *track, int index, DB_FILE **f) {
    if (index == 0) {
        // TODO ensure URL is of supported scheme
        *f = deadbeef->fopen (track->url);
    }
}

static DB_FILE *
vk_ddb_vfs_open (const char *fname) {
    int owner;
    int aid;
    GError *error;
    DB_FILE *f = 0;
    char *audio_resp;
    char get_audio_url[VK_URL_LEN];

    if (!vk_auth_data || !vk_auth_data->access_token) {
        trace ("Not authenticated? Visit VK.com\n");
        return 0;
    }

    // retrieve audio URL
    sscanf (fname, "vk://%d_%d", &owner, &aid);
    sprintf (get_audio_url, VK_AUDIO_GET_BY_ID, vk_auth_data->access_token, owner, aid);
    audio_resp = http_get_string (get_audio_url, &error);

    if (audio_resp) {
        // got URL, delegate the rest to other plugin
        vk_audio_response_parse (audio_resp,
                                 (VkAudioTrackCallback) vk_vfs_store_track,
                                 &f,
                                 &error);
        g_free (audio_resp);

    } else {
        trace ("Cannot get URL for VK audio %d_%d\n", owner, aid);
        g_error_free (error);
    }

    return f;
}

static int
vk_ddb_vfs_is_streaming (void) {
    return 1;
}

static int
vk_action_callback (DB_plugin_action_t *action, int ctx) {
    g_idle_add (vk_action_gtk, NULL);
    return 0;
}

static void
vk_config_changed () {
    // restore auth url if it was occasionally changed
    deadbeef->conf_set_str (CONF_VK_AUTH_URL, VK_AUTH_URL);

    // read VK auth data
    deadbeef->conf_lock ();
    const gchar *auth_data_str = deadbeef->conf_get_str_fast (CONF_VK_AUTH_DATA, NULL);
    deadbeef->conf_unlock ();

    vk_auth_data_free (vk_auth_data);
    vk_auth_data = vk_auth_data_parse (auth_data_str);
}

static int
vk_ddb_connect () {
    vfs_curl_plugin = (DB_vfs_t *) deadbeef->plug_get_for_id ("vfs_curl");
    if (!vfs_curl_plugin) {
        trace ("cURL VFS plugin required");
        return -1;
    }

    gtkui_plugin = (ddb_gtkui_t *) deadbeef->plug_get_for_id (DDB_GTKUI_PLUGIN_ID);

    if (gtkui_plugin && gtkui_plugin->gui.plugin.version_major == 2) {  // gtkui version 2
        // set default UI options
        vk_search_opts.filter_duplicates = (1 == deadbeef->conf_get_int (CONF_VK_UI_DEDUP, 1));
        vk_search_opts.search_whole_phrase = (1 == deadbeef->conf_get_int (CONF_VK_UI_WHOLE_PHRASE, 1));
        vk_search_opts.search_target = deadbeef->conf_get_int (CONF_VK_UI_TARGET, VK_TARGET_ANY_FIELD);


        gtkui_plugin->w_reg_widget ("VK Browser", DDB_WF_SINGLE_INSTANCE, w_vkbrowser_create, "vkbrowser", NULL);

        vk_config_changed ();   // refresh config at start
        return 0;
    }

    return -1;
}

static DB_plugin_action_t vk_action = {
    .title = "File/Add tracks from VK",
    .name = "vk_add_tracks",
    .flags = DB_ACTION_COMMON | DB_ACTION_ADD_MENU,
    .callback = (DB_plugin_action_callback_t) vk_action_callback,
    .next = NULL,
};

static DB_plugin_action_t *
vk_ddb_getactions (DB_playItem_t *it) {
    return &vk_action;
}

/**
 * DeadBeef messages handler.
 *
 * @param id message id.
 * @param ctx ?
 * @param p1 ?
 * @param p2 ?
 * @return ?
 */
static int
vk_ddb_message (uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
    switch (id) {
    case DB_EV_CONFIGCHANGED:
        vk_config_changed();
        break;
    }
    return 0;
}

static int
vk_ddb_disconnect () {
    vk_auth_data_free(vk_auth_data);
    if (gtkui_plugin) {
        gtkui_plugin->w_unreg_widget ("vkbrowser");
    }
    return 0;
}

static const char vk_ddb_config_dlg[] =
    "property \"Navigate to the URL in text box\n(don't change the URL here)\" entry " CONF_VK_AUTH_URL " " VK_AUTH_URL ";\n"
    "property \"Paste data from the page here\" entry " CONF_VK_AUTH_DATA " \"\";\n";

DB_vfs_t plugin = {
    DDB_REQUIRE_API_VERSION(1, 5)
    .plugin.type = DB_PLUGIN_VFS,
    .plugin.version_major = 0,
    .plugin.version_minor = 1,
#if GTK_CHECK_VERSION(3,0,0)
    .plugin.id          = "vkontakte_3",
#else
    .plugin.id          = "vkontakte_2",
#endif
    .plugin.name        = "VKontakte",
    .plugin.descr       = "Play music from VKontakte social network site.\n",
    .plugin.copyright   = "Kirill Malyshev",
    .plugin.website     = "http://scorpp.github.io/db-vk/",
    // callbacks
    .plugin.configdialog    = vk_ddb_config_dlg,
    .plugin.connect         = vk_ddb_connect,
    .plugin.disconnect      = vk_ddb_disconnect,
    .plugin.message         = vk_ddb_message,
    .plugin.get_actions     = vk_ddb_getactions,
    // overriding minimum methods of a VFS plugin since cfs_curl will do all
    // the work for us. files open with vkontakte plugin will actually look
    // as those opened by vfs_curl.
    .get_schemes    = vk_ddb_vfs_get_schemes,
    .is_streaming   = vk_ddb_vfs_is_streaming,
    .open           = vk_ddb_vfs_open
};

DB_plugin_t *
#if GTK_CHECK_VERSION(3,0,0)
vkontakte_gtk3_load (DB_functions_t *api) {
#else
vkontakte_gtk2_load (DB_functions_t *api) {
#endif
    deadbeef = api;
    return DB_PLUGIN (&plugin);
}
