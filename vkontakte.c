#include <gtk/gtk.h>
#include "../../deadbeef.h"
#include "../gtkui/gtkui_api.h"

#define trace(...) { fprintf(stderr, __VA_ARGS__); }
#ifdef ENABLE_NLS
#include <libintl.h>
#define _(X) gettext(X)
#else
#define _(X) (X)
#endif

static DB_functions_t *deadbeef;
static ddb_gtkui_t *gtkui_plugin;
static intptr_t http_tid;	// thread for communication

static GtkWidget *add_tracks_dlg;

void 
on_search(GtkWidget *widget, GtkWidget *entry) {
	deadbeef->thread_start();
	trace(gtk_entry_get_text(GTK_ENTRY(widget)));
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
