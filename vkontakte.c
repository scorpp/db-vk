#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <gtk/gtk.h>
#include <deadbeef.h>
#include <plugins/gtkui/gtkui_api.h>
#include <curl/curl.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>

#include "vkontakte.h"
//#include "util.h"

static DB_functions_t *deadbeef;
static ddb_gtkui_t *gtkui_plugin;

static intptr_t http_tid;	// thread for communication
static SEARCH_QUERY *query;

static GtkWidget *add_tracks_dlg;

static char *vk_access_token = "9f5f8a80cf1106e89f4346cefb9f3058e699f1e9f1e094271e9a4c72c961b6c";

/**
 * List view columns.
 */
enum
{
	ARTIST_COLUMN,
	TITLE_COLUMN,
	DURATION_COLUMN,
	URL_COLUMN,
	N_COLUMNS
};

// TODO move to util.c
gchar *
strdup(const gchar *str) {
	int len = strlen(str);
	gchar *new_str = malloc(len);
	return strcpy(new_str, str);
}

void
parse_audio_track(JsonArray *array, guint index_, JsonNode *element_node, gpointer user_data) {
	assert(JSON_NODE_HOLDS_OBJECT(element_node));
	GtkTreeIter *iter;
	JsonObject *track;
	
	iter = (GtkTreeIter *) user_data;
	track = json_node_get_object(element_node);
	
	// read data from json
	int aid = json_object_get_int_member(track, "aid");
	const char *artist = json_object_get_string_member(track, "artist");
	int duration = json_object_get_int_member(track, "duration");
	int owner_id = json_object_get_int_member(track, "owner_id");
	const char *title = json_object_get_string_member(track, "title");
	const char *url = json_object_get_string_member(track, "url");
	
	// write to list store
	gtk_list_store_append(query->store, iter);
	gtk_list_store_set(query->store, iter, 
		ARTIST_COLUMN, artist,
		TITLE_COLUMN, title,
		DURATION_COLUMN, duration,
		URL_COLUMN, url,
		-1);
}

void
parse_audio_resp(GString *resp_str) {
	GError *error;
	JsonParser *parser;
	JsonNode *root;
	JsonArray *tracks;
	JsonObject *tmp_obj;
	JsonNode *tmp_node;

	parser = json_parser_new();
	
	error = NULL;	
	if (!json_parser_load_from_data(parser, resp_str->str, -1, &error)) {
		trace("Unable to parse audio response: %s\n", error->message);
		g_error_free(error);
		g_object_unref(parser);
		return;
	}
	
	root = json_parser_get_root(parser);
	
	assert(JSON_NODE_HOLDS_OBJECT(root));
	tmp_obj = json_node_get_object(root);
	tmp_node = json_object_get_member(tmp_obj, "response");
	assert(JSON_NODE_HOLDS_ARRAY(tmp_node));
	
	tracks = json_node_get_array(tmp_node);
	int n_tracks = json_array_get_length(tracks);

	// Fill the store
	GtkTreeIter iter;
	gtk_list_store_clear(query->store);
	
	json_array_foreach_element(tracks, parse_audio_track, &iter);
	
	g_object_unref(parser);
}

size_t 
http_write_data( char *ptr, size_t size, size_t nmemb, void *userdata) {
	GString *resp_str = (GString *) userdata;
	g_string_append_len(resp_str, ptr, size * nmemb);	
	return size * nmemb;
}

void 
http_thread_func(void *ctx) {
	trace("===Http thread started\n");
	
	SEARCH_QUERY *query = (SEARCH_QUERY *) ctx;
	
	CURL *curl;
	GString *resp_str;
	GError *error = NULL;
	
	curl = curl_easy_init();
	resp_str = g_string_new("");
	
	char *curl_err_buf = malloc(CURL_ERROR_SIZE);
	char *method_url = malloc(strlen(VK_API_URL) + strlen(vk_access_token) + 24);
	sprintf(method_url, "%s%s%s", VK_API_URL, "audio.get?access_token=", vk_access_token);
	curl_easy_setopt(curl, CURLOPT_URL, method_url);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "deadbeef");
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
	// enable up to 10 redirects
    curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt (curl, CURLOPT_MAXREDIRS, 10);
	// setup handlers
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, http_write_data);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, resp_str);
	curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_err_buf);
	
	int status = curl_easy_perform(curl);
	trace("\ncurl_easy_perform(..) returned %d\n", status);
	if (status != 0) {
		trace(curl_err_buf);
	}
	
//	fwrite(resp_str->str, resp_str->allocated_len, 1, stderr);
	trace("\n");
	trace("=== Parsing response\n");
	parse_audio_resp(resp_str);
	
	free(method_url);
	curl_easy_cleanup(curl);
	trace("Http thread exit\n");
}

void
vk_add_track_from_tree_store_to_playlist(GtkTreeIter *iter, ddb_playlist_t *plt) {
	gchar *artist;
	gchar *title;
	int duration;
	gchar *url;
	
	gtk_tree_model_get(GTK_TREE_MODEL(query->store), iter, 
			ARTIST_COLUMN, &artist,
			TITLE_COLUMN, &title, 
			DURATION_COLUMN, &duration,
			URL_COLUMN, &url, 
			-1);

	DB_playItem_t *pt;
	int pabort = 0;
	
	pt = deadbeef->plt_insert_file(plt, NULL, url, &pabort, NULL, NULL);
	deadbeef->pl_add_meta(pt, "artist", artist);
	deadbeef->pl_add_meta(pt, "title", title);
	deadbeef->plt_set_item_duration(plt, pt, duration);
	deadbeef->pl_item_ref(pt);
	
	free(artist);
	free(title);
	free(url);	
}

void
on_search_results_row_activate(GtkTreeView *tree_view,
                               GtkTreePath *path, 
                               GtkTreeViewColumn *column,
							   gpointer user_data) {
	GtkTreeModel *model;
	GtkTreeIter iter;
	ddb_playlist_t *plt;
	
	model = gtk_tree_view_get_model(tree_view);
	plt = deadbeef->plt_get_curr();
	
	if (gtk_tree_model_get_iter(model, &iter, path)) {
		vk_add_track_from_tree_store_to_playlist(&iter, plt);
	}
	
	deadbeef->plt_unref(plt);
}

void 
on_search(GtkWidget *widget, gpointer data) {
	// TODO block widget until thread is started
	if (http_tid) {
		trace("Killing http thread");
		deadbeef->thread_detach(http_tid);
		free(query->query);
		free(query);

		http_tid = 0;
		query = NULL;
	}
	
	const gchar *query_text = gtk_entry_get_text(GTK_ENTRY(widget));
	trace("== Searching for %s\n", query_text);

	query = malloc(sizeof(SEARCH_QUERY));
	query->query = strdup(query_text);
	query->store = GTK_LIST_STORE(data);
	
	http_tid = deadbeef->thread_start(http_thread_func, query);
}

static GtkWidget *
vk_create_add_tracks_dlg()
{
	GtkWidget *dlg;
	GtkWidget *dlg_vbox;
	GtkWidget *search_text;
	GtkWidget *search_results;
	GtkListStore *list_store;
	GtkCellRenderer *artist_cell;
	GtkCellRenderer *title_cell;
	
	dlg = gtk_dialog_new ();
	gtk_container_set_border_width (GTK_CONTAINER (dlg), 12);
	gtk_window_set_title (GTK_WINDOW (dlg), /*_(*/"Search tracks"/*)*/);
	gtk_window_set_type_hint (GTK_WINDOW (dlg), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_dialog_set_has_separator (GTK_DIALOG (dlg), FALSE);
	
	dlg_vbox = gtk_dialog_get_content_area(GTK_DIALOG(dlg));

	list_store = gtk_list_store_new(N_COLUMNS, 
		G_TYPE_STRING,	// ARTIST
		G_TYPE_STRING,	// TITLE
		G_TYPE_INT,		// DURATION seconds, not rendered
		G_TYPE_STRING);	// URL, not rendered
	
	search_text = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(dlg_vbox), search_text, FALSE, FALSE, 0);
	gtk_widget_show(search_text);	
	g_signal_connect(search_text, "activate", G_CALLBACK(on_search), list_store);
	
	search_results = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
	artist_cell = gtk_cell_renderer_text_new();
	title_cell = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(search_results),
			-1, "Artist", artist_cell, "text", ARTIST_COLUMN, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(search_results), 
			-1, "Title", title_cell, "text", TITLE_COLUMN, NULL);
	g_signal_connect(search_results, "row-activated", 
			G_CALLBACK(on_search_results_row_activate), NULL);
	
	gtk_box_pack_start(GTK_BOX(dlg_vbox), search_results, TRUE, TRUE, 12);
	gtk_widget_show_all(dlg);
	
	return dlg;
}

static gboolean
vk_action_gtk (void *data) {
    add_tracks_dlg = vk_create_add_tracks_dlg();
    gtk_widget_set_size_request (add_tracks_dlg, 400, 400);
    gtk_window_set_transient_for(GTK_WINDOW(add_tracks_dlg),
                                 GTK_WINDOW(gtkui_plugin->get_mainwin()));
    gtk_widget_show(add_tracks_dlg);
    return FALSE;
}

static int
vk_action_callback(DB_plugin_action_t *action,
                                void *user_data) {
    g_idle_add (vk_action_gtk, NULL);
    return 0;
}

static DB_plugin_action_t vk_action = {
    .title = "File/Add tracks from VK",
    .name = "vk_add_tracks",
    .flags = DB_ACTION_COMMON,
    .callback = vk_action_callback,
    .next = NULL,
};

static DB_plugin_action_t *
vk_getactions(DB_playItem_t *it) {
    return &vk_action;
}

int vk_connect() {
#if GTK_CHECK_VERSION(3,0,0)
    gtkui_plugin = (ddb_gtkui_t *)deadbeef->plug_get_for_id ("gtkui3");
#else
    gtkui_plugin = (ddb_gtkui_t *)deadbeef->plug_get_for_id ("gtkui");
#endif

    if(!gtkui_plugin) {
        return -1;
    }
    return 0;
}

DB_misc_t plugin = {
    .plugin.api_vmajor = 1,
    .plugin.api_vminor = 0,
    .plugin.type = DB_PLUGIN_MISC,
    .plugin.version_major = 1,
    .plugin.version_minor = 0,
    .plugin.id = "vkontakte",
    .plugin.name = "VKontakte",
    .plugin.descr = "TODO descr\n",
    .plugin.copyright = 
        "TODO (C) scorpp\n",
    .plugin.website = "http://TODO.com",
	.plugin.connect = vk_connect,
	.plugin.get_actions = vk_getactions,
};

DB_plugin_t *
vkontakte_load (DB_functions_t *api) {
    deadbeef = api;
    return &plugin.plugin;
}
