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
#include "util.h"

static DB_functions_t *deadbeef;
static ddb_gtkui_t *gtkui_plugin;

static intptr_t http_tid;	// thread for communication

static GtkWidget *add_tracks_dlg;

static char *vk_access_token = "019d159c51d9c4c6011ed942e601f2c7fa001dc01dc965e4c016eb73302b192";

void
parse_audio_track(JsonArray *array, guint index_, JsonNode *element_node, gpointer user_data) {
	assert(JSON_NODE_HOLDS_OBJECT(element_node));
	VK_AUDIO_INFO *vk_tracks;
	JsonObject *track;
	
	vk_tracks = (VK_AUDIO_INFO *) user_data;	// user_data is an array
	vk_tracks += index_;
	
	track = json_node_get_object(element_node);
	
	vk_tracks->aid = json_object_get_int_member(track, "aid");
	vk_tracks->artist = json_object_get_string_member(track, "artist");
	vk_tracks->duration = json_object_get_int_member(track, "duration");
	vk_tracks->owner_id = json_object_get_int_member(track, "owner_id");
	vk_tracks->title = json_object_get_string_member(track, "title");
	vk_tracks->url = json_object_get_string_member(track, "url");
}

void
parse_audio_resp(GInputStream *input) {
	GError *error = NULL;
	JsonParser *parser;
	JsonNode *root;
	JsonArray *tracks;
	JsonObject *tmp_obj;
	JsonNode *tmp_node;

	parser = json_parser_new();
	json_parser_load_from_stream(parser, input, NULL, &error);
	if (error) {
		trace("Unable to parse audio response: %s\n", error->message);
		g_error_free(error);
		g_object_unref(parser);
		return;
	}
	
	root = json_parser_get_root(parser);
	
	assert(JSON_NODE_HOLDS_OBJECT(root));
	tmp_obj = json_node_get_object(root);
	tmp_node = json_object_get_member(tmp_obj, "responses");
	assert(JSON_NODE_HOLDS_ARRAY(tmp_node));
	
	tracks = json_node_get_array(tmp_node);
	int n_tracks = json_array_get_length(tracks);
	VK_AUDIO_INFO vk_tracks[n_tracks];// = VK_AUDIO_INFO[n_tracks];//calloc(n_tracks, sizeof(VK_AUDIO_INFO));
	json_array_foreach_element(tracks, parse_audio_track, &vk_tracks);
	
	for (int i = 0; i < n_tracks; i++) {
		VK_AUDIO_INFO track = vk_tracks[i];
		trace("%s - %s [%d]\n", track.artist, track.title, track.duration);
	}
	
	g_object_unref(parser);
	
	for (int i = 0; i < n_tracks; i++) {
		VK_AUDIO_INFO track = vk_tracks[i];
		trace("%s - %s [%d]\n", track.artist, track.title, track.duration);
	}	
}

size_t 
http_write_data( char *ptr, size_t size, size_t nmemb, void *userdata) {
	GMemoryInputStream *resp_stream = (GMemoryInputStream *) userdata;
	
	trace(ptr);
	//trace("size * nmemb %d\nstrlen %d\ng_utf8_strlen %d", size * nmemb, strlen(ptr), g_utf8_strlen(ptr, -1));
	//fwrite(ptr, 1, nmemb * size, stderr);
//	trace("\n");
	char *formatted = str_replace(ptr, ",", ",\n");
	if (formatted) {
		g_memory_input_stream_add_data(resp_stream, 
				ptr, 
				size * nmemb, 
				NULL);
		free(formatted);
	} else {
		trace("Null data in http_write_date");
	}
	
	return size * nmemb;
}

void 
http_thread_func(void *ctx) {
	trace("Http thread started\n");
	CURL *curl;
	GInputStream *resp_stream;
	
	curl = curl_easy_init();
	resp_stream = g_memory_input_stream_new();
	
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
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, resp_stream);
	curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_err_buf);
	
	int status = curl_easy_perform(curl);
	trace("curl_easy_perform(..) returned %d\n", status);
	if (status != 0) {
		trace(curl_err_buf);
	}
	
	g_input_stream_read()
	parse_audio_resp(resp_stream);
	
	g_object_unref(resp_stream);
	free(method_url);
	curl_easy_cleanup(curl);
	trace("Http thread exit\n");
}

void 
on_search(GtkWidget *widget, GtkWidget *entry) {
	if (http_tid) {
		trace("Killing http thread");
		deadbeef->thread_detach(http_tid);
		http_tid = 0;
	}
	
	const gchar *query_text = gtk_entry_get_text(GTK_ENTRY(widget));
	trace("Searching for %s\n", query_text);
	SEARCH_QUERY query = { .query = query_text };
	
	http_tid = deadbeef->thread_start(http_thread_func, &query);
}

static GtkWidget *
vk_create_add_tracks_dlg()
{
	GtkWidget *dlg = gtk_dialog_new ();
	gtk_container_set_border_width (GTK_CONTAINER (dlg), 12);
	gtk_window_set_title (GTK_WINDOW (dlg), /*_(*/"Search tracks"/*)*/);
	gtk_window_set_type_hint (GTK_WINDOW (dlg), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_dialog_set_has_separator (GTK_DIALOG (dlg), FALSE);
	
	GtkWidget *dlg_vbox = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
	
	GtkWidget *search_text = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(dlg_vbox), search_text, FALSE, FALSE, 0);
	gtk_widget_show(search_text);	
	g_signal_connect(search_text, "activate", G_CALLBACK(on_search), NULL);
	
	GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
	GtkWidget *search_results = gtk_tree_view_new_with_model(store);
	gtk_box_pack_start(GTK_BOX(dlg_vbox), search_results, TRUE, TRUE, 12);
	gtk_widget_show(search_results);	
	return dlg;
}

static gboolean
vk_action_gtk (void *data)
{
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

/*
 {
   "response":[      
      {
         "aid":26372908,
         "owner_id":4293576,
         "artist":"Фліт",
         "title":"Їжачок",
         "duration":230,
         "url":"http:\/\/cs1334.vkontakte.ru\/u4293576\/audio\/528b3745fd2b.mp3"
      },
      {
         "aid":22102671,
         "owner_id":4293576,
         "artist":"Dire Straits",
         "title":"Sultans of Swing",
         "duration":346,
         "url":"http:\/\/cs1335.vkontakte.ru\/u4293576\/audio\/36893e57f6f6.mp3"
      },
      {
         "aid":22098274,
         "owner_id":4293576,
         "artist":"Dire Straits",
         "title":"Money for Nothing",
         "duration":246,
         "url":"http:\/\/cs1335.vkontakte.ru\/u4293576\/audio\/25224557805f.mp3"
      },
      {
         "aid":42601125,
         "owner_id":4293576,
         "artist":"Frank Klepacki",
         "title":"Big Foot",
         "duration":381,
         "url":"http:\/\/cs1631.vkontakte.ru\/u4293576\/audio\/c8733785bd12.mp3"
      },
      {
         "aid":43432450,
         "owner_id":4293576,
         "artist":"ScORcH",
         "title":"ScORcH-tisfaction",
         "duration":227,
         "url":"http:\/\/cs1628.vkontakte.ru\/u4293576\/audio\/10f8a514936d.mp3"
      },
      {
         "aid":47040293,
         "owner_id":4293576,
         "artist":"Dropkick Murphys",
         "title":"Loyal To No-One",
         "duration":145,
         "url":"http:\/\/cs1710.vkontakte.ru\/u4293576\/audio\/0be1eae8bab4.mp3",
         "lyrics_id":"1415403"
      },
      {
         "aid":40826173,
         "owner_id":4293576,
         "artist":"Dropkick Murphys",
         "title":"The State Of Massachusetts",
         "duration":232,
         "url":"http:\/\/cs1544.vkontakte.ru\/u4293576\/audio\/7547b34f6572.mp3",
         "lyrics_id":"1415411"
      },
      {
         "aid":18013801,
         "owner_id":4293576,
         "artist":"In Flames",
         "title":"Gyroscope",
         "duration":208,
         "url":"http:\/\/cs1269.vkontakte.ru\/u4293576\/audio\/305fdb750f9a.mp3"
      }
   ]
}
  */