#include "vkontakte-vfs.h"
#include <string.h>
#include <gtk/gtk.h>
#include "common-defs.h"
#include "vk-api.h"


extern DB_functions_t *deadbeef;
extern VkAuthData *vk_auth_data;
extern DB_vfs_t *vfs_curl_plugin;


// util.c   TODO
gchar *     http_get_string(const gchar *url, GError **error);


const char *vk_ddb_vfs_format_track_url(int aid,
                                        int owner_id,
                                        const char *artist,
                                        const char *title) {
    return (const char *) g_strdup_printf ("vk://%d_%d", owner_id, aid);
}

static const char *scheme_names[] = { "vk://", NULL };
const char **
vk_ddb_vfs_get_schemes () {
    return scheme_names;
}

static void
vk_ddb_vfs_store_track (VkAudioTrack *track, int index, DB_FILE **f) {
    if (index == 0) {
        // TODO ensure URL is of supported scheme
        *f = deadbeef->fopen (track->url);
    }
}

DB_FILE *
vk_ddb_vfs_open (const char *fname) {
    int owner;
    int aid;
    char *get_audio_url;
    char *audio_resp;
    GError *error;
    DB_FILE *f = 0;

    if (!vk_auth_data || !vk_auth_data->access_token) {
        trace ("Not authenticated? Visit VK.com\n");
        return 0;
    }

    // retrieve audio URL
    sscanf (fname, "vk://%d_%d", &owner, &aid);
    get_audio_url
        = g_strdup_printf (VK_API_METHOD_AUDIO_GET_BY_ID "?access_token=%s&audios=%d_%d",
                           vk_auth_data->access_token,
                           owner,
                           aid);
    audio_resp = http_get_string (get_audio_url, &error);
    if (!audio_resp) {
        trace ("Cannot get URL for VK audio %d_%d\n", owner, aid);
        g_error_free (error);

    } else {
        vk_audio_response_parse (audio_resp,
                                 (VkAudioTrackCallback) vk_ddb_vfs_store_track,
                                 &f,
                                 &error);
    }

    g_free (get_audio_url);
    g_free (audio_resp);
    return f;
}

int
vk_ddb_vfs_is_streaming (void) {
    return vfs_curl_plugin->is_streaming ();
}
