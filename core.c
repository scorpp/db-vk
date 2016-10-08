
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <gtk/gtk.h>

#include "core.h"
#include "common-defs.h"
#include "vk-api.h"
#include "ui.h"
#include "util.h"


// URL formatting strings
#define MAX_URL_LEN         300
#define VK_AUDIO_GET                    VK_API_METHOD_AUDIO_GET "?access_token=%s"
#define VK_AUDIO_GET_BY_OWNER           VK_API_METHOD_AUDIO_GET "?access_token=%s&owner_id=%ld"
#define VK_AUDIO_GET_BY_ID              VK_API_METHOD_AUDIO_GET_BY_ID "?access_token=%s&audios=%d_%d"
#define VK_AUDIO_SEARCH                 VK_API_METHOD_AUDIO_SEARCH "?access_token=%s&count=%d&offset=%d&q=%s"
#define VK_AUDIO_GET_RECOMMENDATIONS    VK_API_METHOD_AUDIO_GET_RECOMMENDATIONS "?access_token=%s&count=%d&offset=%d"
#define VK_UTILS_RESOLVE_SCREEN_NAME    VK_API_METHOD_UTILS_RESOLVE_SCREEN_NAME "?access_token=%s&screen_name=%s"

// Max length for VFS URL to VK track
#define VK_VFS_URL_LEN  30

#define vk_send_audio_request_and_parse_response_va(query, url_format, ...) { \
        gchar formatted_url[MAX_URL_LEN]; \
        sprintf (formatted_url, url_format, __VA_ARGS__); \
        vk_send_audio_request_and_parse_response (query, formatted_url); \
    }


static DB_functions_t *deadbeef;
/** Used by ui.c to detect design mode */
ddb_gtkui_t *gtkui_plugin;
static VkAuthData *vk_auth_data = NULL;
static intptr_t http_tid;    // thread for communication

typedef struct {
    const gchar *query;
    glong id; // user or group id
    GtkListStore *store;
} SearchQuery;


static int
vk_vfs_format_track_url (char *url,
                         int aid,
                         int owner_id) {
    return sprintf (url, "vk://%d_%d", owner_id, aid);
}

/**
 * Simple deduplication.
 * @return TRUE if given track already exists, FALSE otherwise.
 */
static gboolean
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
parse_audio_track_callback(VkAudioTrack *track, size_t index, SearchQuery *query) {
    GtkTreeIter iter;

    if (vk_search_filter_matches (query->query, track)
        && !vk_tree_model_has_track (GTK_TREE_MODEL (query->store), track)) {

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
    if (!vk_audio_response_parse (resp_str, (VkAudioTrackCallback) parse_audio_track_callback, query, &error)) {
        show_message(GTK_MESSAGE_ERROR, error->message);
        g_error_free (error);
    }
    gdk_threads_leave ();
}

void
vk_add_tracks_from_tree_model_to_playlist (GtkTreeModel *treemodel, GList *gtk_tree_path_list, const char *plt_name) {
    ddb_playlist_t *plt;

    if (plt_name) {
        int idx = deadbeef->plt_add (deadbeef->plt_get_count (), plt_name);
        deadbeef->plt_set_curr_idx (idx);
        plt = deadbeef->plt_get_for_idx (idx);
    } else {
        plt = deadbeef->plt_get_curr ();
    }

    if (!deadbeef->plt_add_files_begin (plt, 0)) {
        DB_playItem_t *last = deadbeef->plt_get_last (plt, 0);

        gtk_tree_path_list = g_list_last (gtk_tree_path_list);
        while (gtk_tree_path_list) {
            GtkTreeIter iter;
            gchar *artist;
            gchar *title;
            int duration;
            int aid;
            int owner_id;
            char url[VK_VFS_URL_LEN];

            if (gtk_tree_model_get_iter(treemodel, &iter, (GtkTreePath *) gtk_tree_path_list->data)) {
                DB_playItem_t *pt;
                int pabort = 0;

                gtk_tree_model_get (treemodel, &iter,
                                    ARTIST_COLUMN, &artist,
                                    TITLE_COLUMN, &title,
                                    DURATION_COLUMN, &duration,
                                    AID_COLUMN, &aid,
                                    OWNER_ID_COLUMN, &owner_id,
                                    -1);
                vk_vfs_format_track_url (url, aid, owner_id);

                pt = deadbeef->plt_insert_file2 (0, plt, last, url, &pabort, NULL, NULL);
                deadbeef->pl_add_meta (pt, "artist", artist);
                deadbeef->pl_add_meta (pt, "title", title);
                deadbeef->plt_set_item_duration (plt, pt, duration);

                g_free (artist);
                g_free (title);
            }

            gtk_tree_path_list = g_list_previous (gtk_tree_path_list);
        }

        if (last) {
            deadbeef->pl_item_unref (last);
        }
    }

    deadbeef->plt_add_files_end (plt, 0);
    deadbeef->plt_save_config (plt);
    deadbeef->plt_unref (plt);
}

static void
vk_send_audio_request_and_parse_response (SearchQuery *query, const gchar *url) {
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
        rows_added = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (query->store), NULL);
        vk_send_audio_request_and_parse_response_va (query, VK_AUDIO_SEARCH,
                 vk_auth_data->access_token,
                 VK_AUDIO_MAX_TRACKS,
                 VK_AUDIO_MAX_TRACKS * iteration,
                 escaped_search_str);
        rows_added = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (query->store), NULL) - rows_added;
    } while (++iteration < 10 && rows_added > 0);

    trace ("INFO: Did %d iterations and stopped discovering new tracks\n", iteration);

    g_free (escaped_search_str);
    curl_easy_cleanup (curl);
    g_free ((gchar *) query->query);
    g_free (query);
    http_tid = 0;
}

static void
vk_get_my_music_thread_func (void *ctx) {
    SearchQuery query;

    query.query = NULL,
            query.store = GTK_LIST_STORE (ctx);

    vk_send_audio_request_and_parse_response_va (&query, VK_AUDIO_GET, vk_auth_data->access_token);

    http_tid = 0;
}

static void
vk_get_recommended_music_thread_func(void *ctx) {
    SearchQuery query;
    gint rows_added;
    gint iteration = 0;
    query.query = NULL,
            query.store = GTK_LIST_STORE (ctx);
    do {
        rows_added = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (query.store), NULL);
        vk_send_audio_request_and_parse_response_va (&query, VK_AUDIO_GET_RECOMMENDATIONS,
                 vk_auth_data->access_token,
                 VK_AUDIO_MAX_TRACKS,
                 VK_AUDIO_MAX_TRACKS * iteration);
        rows_added = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (query.store), NULL) - rows_added;
    } while (++iteration < 10 && rows_added > 0);
    http_tid = 0;
}

static void
vk_get_by_owner_music_thread_func (SearchQuery *query) {
    CURL *curl;
    curl = curl_easy_init ();

    vk_send_audio_request_and_parse_response_va (query, VK_AUDIO_GET_BY_OWNER, vk_auth_data->access_token, query->id);

    curl_easy_cleanup (curl);
    g_free (query);
    http_tid = 0;
}

/**
 * Try to detect if search_text is a known vk.com link and configure `query` correspondingly.
 *
 * There are three possible outcomes - link to a user profile, link to a group or (fallback option) just
 * regular query text.
 */
static void
vk_detect_search_target(const gchar *search_text, SearchQuery *query) {
    const gchar *remains = NULL;
    gchar url[MAX_URL_LEN];
    gchar *resp_str;
    GError *error = NULL;

    query->query = NULL;

    // try to find of know vk.com prefixes in search string
    for (gint i = 0; remains == NULL && VK_PUBLIC_SITE_PREFIXES[i] != NULL; i++) {
        remains = strip_prefix (search_text, VK_PUBLIC_SITE_PREFIXES[i]);
    }
    if (remains == NULL) {
        // not a vk.com url, fallback to plain search
        query->query = g_strdup (search_text);
        return;
    }

    // ok, it is an vk.com url indeed! an object may be identified with ID or a string alias
    if (g_str_has_prefix (remains, "id")) {
        query->id = strtol (remains + 2, NULL, 10);
        trace("Searched URL containing user id=%ld\n", query->id);
        return;

    } else if (g_str_has_prefix (remains, "club")) {
        query->id = -1 * strtol (remains + 4, NULL, 10);
        trace("Searched URL containing group id=%ld\n", query->id);
        return;
    }

    // else this is an alias
    trace("Searched URL containing name alias=%s\n", remains);
    sprintf (url, VK_UTILS_RESOLVE_SCREEN_NAME, vk_auth_data->access_token, remains);
    resp_str = http_get_string (url, &error);
    if (NULL == resp_str) {
        trace ("VK error: %s\n", error->message);
        query->query = g_strdup (search_text);
        g_error_free (error);
        return;

    } else {
        glong id = vk_utils_resolve_screen_name_parse (resp_str, &error);
        if (!id) {
            trace("Unknown object type under alias %s\n", remains);
            query->query = g_strdup (search_text);
            g_error_free (error);
        } else {
            query->id = id;
        }
        g_free (resp_str);
    }
}

static void
vk_kill_http_thread () {
    if (http_tid) {
        trace("Killing http thread\n");
        deadbeef->thread_detach (http_tid);

        http_tid = 0;
    }
}

void
vk_search_music (const gchar *query_text, GtkListStore *liststore) {
    SearchQuery *query;

    vk_kill_http_thread ();
    trace("== Searching for %s\n", query_text);

    query = g_malloc (sizeof(SearchQuery));
    query->store = liststore;
    // TODO the below func performs http rq, need to call it in separate thread
    vk_detect_search_target (query_text, query);

    if (query->query == NULL) {
        http_tid = deadbeef->thread_start ((DB_thread_func_t) vk_get_by_owner_music_thread_func, query);
    } else {
        http_tid = deadbeef->thread_start ((DB_thread_func_t) vk_search_audio_thread_func, query);
    }
}

void
vk_get_my_music (GtkListStore *liststore) {
    vk_kill_http_thread ();
    trace("== Getting my music, uid=%ld\n", vk_auth_data->user_id);

    http_tid = deadbeef->thread_start (vk_get_my_music_thread_func, liststore);
}

void
vk_get_recommended_music (GtkListStore *liststore) {
    vk_kill_http_thread ();
    trace("== Getting my music, uid=%ld\n", vk_auth_data->user_id);

    http_tid = deadbeef->thread_start (vk_get_recommended_music_thread_func, liststore);
}

static void
vk_vfs_store_track (VkAudioTrack *track, int index, DB_FILE **f) {
    trace ("vk_vfs_store_track: %s, index=%d\n", track->url, index);
    if (index == 0) {
        // TODO ensure URL is of supported scheme
        *f = deadbeef->fopen (track->url);
    }
}

DB_FILE *
vk_vfs_open (const gchar* fname) {
    int owner;
    int aid;
    GError *error;
    DB_FILE *f = 0;
    char *audio_resp;
    char get_audio_url[MAX_URL_LEN];

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

gboolean
vk_action_gtk (void *data) {
    if (vk_auth_data == NULL) {
        // not authenticated, show warning and that's it
        trace ("VK - not authenticated\n")
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

ddb_gtkui_widget_t *
w_vkbrowser_create () {
    if (vk_auth_data == NULL) {
        // not authenticated, show warning and that's it
        // TODO
    }

    ddb_gtkui_widget_t *w = malloc (sizeof (ddb_gtkui_widget_t));
    memset (w, 0, sizeof (ddb_gtkui_widget_t));
    vk_setup_browser_widget (w);
    return w;
}

void
vk_config_changed () {
    // restore auth url if it was occasionally changed
    deadbeef->conf_set_str (CONF_VK_AUTH_URL, VK_AUTH_URL);

    // read VK auth data
    deadbeef->conf_lock ();
    const gchar *auth_data_str = deadbeef->conf_get_str_fast (CONF_VK_AUTH_DATA, NULL);
    // old version of authentication page used single quotes instead of double. so silly!
    auth_data_str = repl_str (auth_data_str, "'", "\"");
    deadbeef->conf_unlock ();

    vk_auth_data_free (vk_auth_data);
    vk_auth_data = vk_auth_data_parse (auth_data_str);

    g_free ((gpointer) auth_data_str);
}

void
vk_set_config_var (const char *key, GValue *value) {
    if (G_VALUE_HOLDS_INT (value)) {
        deadbeef->conf_set_int (key, g_value_get_int (value));
    } else if (G_VALUE_HOLDS_INT64 (value)) {
        deadbeef->conf_set_int64 (key, (int64_t) g_value_get_int64 (value));
    } else if (G_VALUE_HOLDS_FLOAT (value)) {
        deadbeef->conf_set_float (key, g_value_get_float (value));
    } else if (G_VALUE_HOLDS_STRING (value)) {
        deadbeef->conf_set_str (key, g_value_get_string (value));
    } else if (G_VALUE_HOLDS_BOOLEAN (value)) {
        deadbeef->conf_set_int (key, g_value_get_boolean (value) ? 1 : 0);
    } else {
        trace ("WARN unsupported GType to vk_set_config_var: %s\n", G_VALUE_TYPE_NAME (value));
    }
}

void
vk_initialise (DB_functions_t *deadbeef_instance, ddb_gtkui_t *gtkui_instance) {
    deadbeef = deadbeef_instance;
    gtkui_plugin = gtkui_instance;
    // set default UI options
    vk_search_opts.filter_duplicates = (1 == deadbeef->conf_get_int (CONF_VK_UI_DEDUP, 1));
    vk_search_opts.search_whole_phrase = (1 == deadbeef->conf_get_int (CONF_VK_UI_WHOLE_PHRASE, 1));
    vk_search_opts.search_target = (VkSearchTarget) deadbeef->conf_get_int (CONF_VK_UI_TARGET, VK_TARGET_ANY_FIELD);
}

void
vk_perform_cleanup () {
    vk_kill_http_thread ();
    vk_auth_data_free (vk_auth_data);
}
