#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>
#include <curl/curl.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>

#include "vkontakte.h"


static DB_functions_t *deadbeef;
static ddb_gtkui_t *gtkui_plugin;

static struct {
	const gchar *access_token;
	guint user_id;
	guint expires_in;
} *vk_auth_data;

static intptr_t http_tid;	// thread for communication
static SEARCH_QUERY *query;

static GtkWidget *add_tracks_dlg;

#define VK_AUTH_URL "http://oauth.vkontakte.ru/authorize?client_id=3035566&scope=audio,friends&redirect_uri=http://scorpspot.blogspot.com/p/vk-id.html&response_type=token"
#define VK_API_URL "https://api.vk.com/method/"

#define CONF_VK_AUTH_URL "vk.auth.url"
#define CONF_VK_AUTH_DATA "vk.auth.data"

/* 
 * {"error":{
 * 		"error_code":5,
 * 		"error_msg":"User authorization failed: invalid access_token.",
 * 		"request_params":[
 * 			{"key":"oauth","value":"1"},
 * 			{"key":"method","value":"audio.get"},
 * 			{"key":"access_token","value""... 
 */
static char *VK_ERR_JSON_KEY = "error";
static char *VK_ERR_JSON_CODE_KEY = "error_code";
static char *VK_ERR_JSON_MSG_KEY = "error_msg";
static char *VK_ERR_JSON_EXTRA_KEY = "request_params";
typedef struct {
	int error;
	const gchar *err_msg;
	const gchar *extra;
} VkError;

/**
 * List view columns.
 */
enum {
	ARTIST_COLUMN,
	TITLE_COLUMN,
	DURATION_COLUMN,
	URL_COLUMN,
	N_COLUMNS
};


void
vk_error_free(VkError *err) {
	assert(NULL != err);
	g_free((gchar *) err->err_msg);
	g_free((gchar *) err->extra);
	g_free(err);
}

VkError*
vk_error_check(JsonParser *parser) {
	JsonNode *root = json_parser_get_root(parser);
	
	if (!JSON_NODE_HOLDS_OBJECT(root)) {
		VkError *err = g_malloc(sizeof *err);
		err->error = -1;
		err->err_msg = "Root should be a JSON object";
		err->extra = "";
		return err;
	}
	
	JsonObject * rootObj = json_node_get_object(root);
	if (json_object_has_member(rootObj, VK_ERR_JSON_KEY)) {
		rootObj = json_object_get_object_member(rootObj, VK_ERR_JSON_KEY);
		assert(json_object_has_member(rootObj, VK_ERR_JSON_MSG_KEY));
		
		VkError *err = g_malloc(sizeof *err);
		err->error = json_object_get_int_member(rootObj, VK_ERR_JSON_CODE_KEY);
		err->err_msg = g_strdup(json_object_get_string_member(rootObj, VK_ERR_JSON_MSG_KEY));
		if (json_object_has_member(rootObj, "request_params")) {
			JsonGenerator *gen = json_generator_new();
			json_generator_set_root(gen, json_object_get_member(rootObj, "request_params"));
			err->extra = json_generator_to_data(gen, NULL);
		} else {
			err->extra = "";
		}
		return err;
	}	
	return NULL;
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
	const char *artist = g_strdup(json_object_get_string_member(track, "artist"));
	int duration = json_object_get_int_member(track, "duration");
	int owner_id = json_object_get_int_member(track, "owner_id");
	const char *title = g_strdup(json_object_get_string_member(track, "title"));
	const char *url = g_strdup(json_object_get_string_member(track, "url"));
	
	//trace("%d (%d) %s - %s %d:%d %s\n", aid, owner_id, artist, title,
	//	duration / 60, duration % 60, url);
		
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
	VkError *vk_err;	
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
	
	vk_err = vk_error_check(parser);
	if (NULL != vk_err) {
		trace("Error from VK:\n%d\n%s\n%s\n", vk_err->error, vk_err->err_msg, vk_err->extra);
		vk_error_free(vk_err);
		g_object_unref(parser);
		// TODO report error to user
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
	trace("=== Http thread started\n");
	
	SEARCH_QUERY *query = (SEARCH_QUERY *) ctx;
	
	CURL *curl;
	GString *resp_str;
	GError *error = NULL;
	
	curl = curl_easy_init();
	resp_str = g_string_new("");
	
	char *curl_err_buf = malloc(CURL_ERROR_SIZE);
	char *method_url = malloc(strlen(VK_API_URL) + strlen(vk_auth_data->access_token) + 24);
	sprintf(method_url, "%s%s%s", VK_API_URL, "audio.get?access_token=", vk_auth_data->access_token);
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
	
	trace("\n");
	trace("==== Parsing response\n");
	parse_audio_resp(resp_str);
	trace("==== Response parsed\n");
	
	free(method_url);
	curl_easy_cleanup(curl);
	trace("=== Http thread exit\n");
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
	
	deadbeef->pl_lock();
	if (!deadbeef->pl_add_files_begin(plt)) {
		pt = deadbeef->plt_insert_file(plt, NULL, url, &pabort, NULL, NULL);
		deadbeef->pl_add_meta(pt, "artist", artist);
		deadbeef->pl_add_meta(pt, "title", title);
		deadbeef->plt_set_item_duration(plt, pt, duration);
		deadbeef->pl_add_files_end();
		// there is no legal way to refresh the playlist :(
	}
	deadbeef->pl_unlock();
	
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
	gtk_widget_set_sensitive(widget, FALSE);

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
	query->query = g_strdup(query_text);
	query->store = GTK_LIST_STORE(data);
	
	http_tid = deadbeef->thread_start(http_thread_func, query);

	gtk_widget_set_sensitive(widget, TRUE);
	gtk_widget_grab_focus(widget);
}

static GtkWidget *
vk_create_add_tracks_dlg()
{
	GtkWidget *dlg;
	GtkWidget *dlg_vbox;
	GtkWidget *scroll_window;
	GtkWidget *search_text;
	GtkWidget *search_results;
	GtkListStore *list_store;
	GtkCellRenderer *artist_cell;
	GtkCellRenderer *title_cell;
	
	dlg = gtk_dialog_new ();
	gtk_container_set_border_width (GTK_CONTAINER (dlg), 12);
	gtk_window_set_title (GTK_WINDOW (dlg), "Search tracks");
	gtk_window_set_type_hint (GTK_WINDOW (dlg), GDK_WINDOW_TYPE_HINT_DIALOG);
	
	dlg = gtk_dialog_new();
	gtk_container_set_border_width(GTK_CONTAINER (dlg), 12);
	gtk_window_set_default_size(GTK_WINDOW(dlg), 840, 400);
	gtk_window_set_title(GTK_WINDOW(dlg), "Search tracks");
	gtk_window_set_type_hint(GTK_WINDOW(dlg), GDK_WINDOW_TYPE_HINT_DIALOG);

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

	GtkCellRenderer *duration_cell;	
	search_results = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
	artist_cell = gtk_cell_renderer_text_new();
	title_cell = gtk_cell_renderer_text_new();
	duration_cell = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(search_results),
			-1, "Artist", artist_cell, "text", ARTIST_COLUMN, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(search_results), 
			-1, "Title", title_cell, "text", TITLE_COLUMN, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(search_results), 
			-1, "Duration", duration_cell, "text", DURATION_COLUMN, NULL);
	g_signal_connect(search_results, "row-activated", 
			G_CALLBACK(on_search_results_row_activate), NULL);
	
	// allow column resize
	GList *columns;
	GList *i;
	columns = gtk_tree_view_get_columns(GTK_TREE_VIEW(search_results));
	i = g_list_first(columns);
	while (i) {
		GtkTreeViewColumn *col;
		col = GTK_TREE_VIEW_COLUMN(i->data);
		gtk_tree_view_column_set_resizable(col, TRUE);
		
		i = g_list_next(i);
	}
	g_list_free(columns);
	
	scroll_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scroll_window), search_results);
	gtk_box_pack_start(GTK_BOX(dlg_vbox), scroll_window, TRUE, TRUE, 12);
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

int 
vk_stop() {
	if (vk_auth_data != NULL) {
		g_free((gchar *) vk_auth_data->access_token);
		g_free(vk_auth_data);
	}
	
	if (query != NULL) {
		g_free(query->query);
		// TODO g_free query->store ?
		g_free(query);
	}
}

int 
vk_connect() {
#if GTK_CHECK_VERSION(3,0,0)
    gtkui_plugin = (ddb_gtkui_t *)deadbeef->plug_get_for_id ("gtkui3");
#else
    gtkui_plugin = (ddb_gtkui_t *)deadbeef->plug_get_for_id ("gtkui");
#endif

    if(!gtkui_plugin) {
        return -1;
    }
	
	/* Read config */
	// restore auth url if it was occasionally changed
	deadbeef->conf_set_str(CONF_VK_AUTH_URL, VK_AUTH_URL);
	// read VK auth data
	const gchar *auth_data_str = deadbeef->conf_get_str_fast(CONF_VK_AUTH_DATA, NULL);
	if (auth_data_str != NULL 
			&& strlen(auth_data_str) > 0) {
		GError *error;
		JsonParser *parser = json_parser_new();
		
		if (!json_parser_load_from_data(parser, auth_data_str, strlen(auth_data_str), &error)) {
			trace("VK auth data invalid, clearing");
			g_free(error);
			g_object_unref(parser);
		}
		
		assert(JSON_NODE_HOLDS_OBJECT(json_parser_get_root(parser)));
		JsonObject *root = json_node_get_object(json_parser_get_root(parser));
		
		vk_auth_data = g_malloc(sizeof *vk_auth_data);
		vk_auth_data->access_token = g_strdup(json_object_get_string_member(root, "access_token"));
		vk_auth_data->user_id = json_object_get_int_member(root, "user_id");
		vk_auth_data->expires_in = json_object_get_int_member(root, "expires_in");
		
		g_object_unref(parser);
	}
    return 0;
}

static const char vk_config_dlg[] = 
	"property \"Navigate to the URL below\n(don't change the URL)\" entry " CONF_VK_AUTH_URL " " VK_AUTH_URL ";\n"
	"property \"Paste data from the page here\" entry " CONF_VK_AUTH_DATA " \"\";\n";

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
	.plugin.configdialog = vk_config_dlg,
	.plugin.stop = vk_stop,
	.plugin.connect = vk_connect,
	.plugin.get_actions = vk_getactions,
};

DB_plugin_t *
vkontakte_load (DB_functions_t *api) {
    deadbeef = api;
    return &plugin.plugin;
}
