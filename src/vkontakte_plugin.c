// disable gdk_thread_enter\leave warnings
#define GDK_VERSION_MIN_REQUIRED GDK_VERSION_3_4

#define DDB_WARN_DEPRECATED 1
#define DDB_API_LEVEL 6

#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>

#include "common-defs.h"
#include "core.h"


static DB_functions_t *deadbeef;
static ddb_gtkui_t *gtkui_plugin;
static DB_vfs_t *vfs_curl_plugin;


static const char *scheme_names[] = { "vk://", NULL };
static const char **
vk_ddb_vfs_get_schemes () {
    return scheme_names;
}

static DB_FILE *
vk_ddb_vfs_open (const char *fname) {
    return vk_vfs_open (fname);
}

static int
vk_ddb_vfs_is_streaming () {
    return 1;
}

static int
vk_ddb_action_callback(DB_plugin_action_t *action, int ctx) {
    g_idle_add (vk_action_gtk, NULL);
    return 0;
}

static int
vk_ddb_connect () {
    vfs_curl_plugin = (DB_vfs_t *) deadbeef->plug_get_for_id ("vfs_curl");
    if (!vfs_curl_plugin) {
        trace ("cURL VFS plugin required\n");
        return -1;
    }

    gtkui_plugin = (ddb_gtkui_t *) deadbeef->plug_get_for_id (DDB_GTKUI_PLUGIN_ID);

    if (gtkui_plugin && gtkui_plugin->gui.plugin.version_major == 2) {  // gtkui version 2
        vk_initialise (deadbeef, gtkui_plugin);
        gtkui_plugin->w_reg_widget ("VK Browser", DDB_WF_SINGLE_INSTANCE, w_vkbrowser_create, "vkbrowser", NULL);
        vk_config_changed ();   // refresh config at start
        return 0;
    }

    return -1;
}

static DB_plugin_action_t vk_ddb_action = {
    .title = "File/Add tracks from VK",
    .name = "vk_add_tracks",
    .flags = DB_ACTION_COMMON | DB_ACTION_ADD_MENU,
    .callback2 = (DB_plugin_action_callback2_t) vk_ddb_action_callback,
    .next = NULL,
};

static DB_plugin_action_t *
vk_ddb_get_actions(DB_playItem_t *it) {
    return &vk_ddb_action;
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
        default:
            break;
    }
    return 0;
}

static int
vk_ddb_disconnect () {
    vk_perform_cleanup ();

    if (gtkui_plugin) {
        gtkui_plugin->w_unreg_widget ("vkbrowser");
    }

    gtkui_plugin = NULL;
    vfs_curl_plugin = NULL;
    return 0;
}

static const char vk_ddb_config_dialog[] =
    "property \"Navigate to the URL in text box\n(don't change the URL here)\" entry " CONF_VK_AUTH_URL " " VK_AUTH_URL ";\n"
    "property \"Paste data from the page here\" entry " CONF_VK_AUTH_DATA " \"\";\n"
    "property \"Automatically fallback to plain HTTP for tracks\n(VK API still over HTTPS)\" checkbox " CONF_TRACKS_FORCE_HTTP " 0;\n"
;

DB_vfs_t plugin = {
    DDB_REQUIRE_API_VERSION(1, 5)
    .plugin.type = DB_PLUGIN_VFS,
    .plugin.version_major = 0,
    .plugin.version_minor = 2,
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
    .plugin.configdialog    = vk_ddb_config_dialog,
    .plugin.connect         = vk_ddb_connect,
    .plugin.disconnect      = vk_ddb_disconnect,
    .plugin.message         = vk_ddb_message,
    .plugin.get_actions     = vk_ddb_get_actions,
    // overriding minimum methods of a VFS plugin since vfs_curl will do all
    // the work for us. files opened with vkontakte plugin will actually look
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
