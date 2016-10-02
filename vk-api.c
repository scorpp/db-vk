/*
 * vk-api.c
 *
 *  Created on: Dec 9, 2012
 *      Author: scorp
 */

#include <assert.h>
#include <string.h>
#include <glib-object.h>
#include <jansson.h>
#include "common-defs.h"
#include "vk-api.h"


#define VK_API_DOMAIN_STR "vkontakte api for deadbeef"
/*
 * {"error":{
 *      "error_code":5,
 *      "error_msg":"User authorization failed: invalid access_token.",
 *      "request_params":[
 *          {"key":"oauth","value":"1"},
 *          {"key":"method","value":"audio.get"},
 *          {"key":"access_token","value""...
 */
#define VK_ERR_JSON_KEY         "error"
#define VK_ERR_JSON_CODE_KEY    "error_code"
#define VK_ERR_JSON_MSG_KEY     "error_msg"
#define VK_ERR_JSON_EXTRA_KEY   "request_params"


/**
 * Checks if response contains error, returns TRUE and sets error appropriately in
 * case of failure. Just returns FALSE otherwise.
 *
 * @return TRUE if response contains failure, FALSE otherwise.
 */
static gboolean
vk_error_check (json_t *root, GError **error) {

    if (!json_is_object (root) ) {
        *error = g_error_new_literal (g_quark_from_static_string (VK_API_DOMAIN_STR),
                                      -1,
                                      "Root should be a JSON object");
        return FALSE;
    }

    json_t *error_response = json_object_get (root, VK_ERR_JSON_KEY);
    if (error_response) {
        json_t *error_message = json_object_get (error_response, VK_ERR_JSON_MSG_KEY);
        assert (error_message);

        *error = g_error_new_literal (g_quark_from_static_string (VK_API_DOMAIN_STR),
                                      (gint) json_integer_value (json_object_get (root, VK_ERR_JSON_CODE_KEY)),
                                      g_strdup (json_string_value (error_message)) );
        return FALSE;
    }
    return TRUE;
}

static void
json_error_to_g_error(json_error_t *json_error, GError **error) {
    *error = g_error_new (
            g_quark_from_static_string (VK_API_DOMAIN_STR),
            0,
            "%s. Line: %d, column %d",
            json_error->text,
            json_error->line,
            json_error->column
    );
}

static void
vk_audio_track_parse (json_t *tracks_array, size_t index_, VkAudioTrack *audio_track) {
    json_t *json_track;

    json_track = json_array_get (tracks_array, index_);

    // first element may contain number of elements
    if (0 == index_ && !json_is_object (json_track)) {
        return;
    }

    assert (json_is_object (json_track));

    // read data from json
    audio_track->aid          = (int) json_integer_value (json_object_get (json_track, "aid"));
    audio_track->artist       = json_string_value (json_object_get (json_track, "artist"));
    audio_track->duration     = (int) json_integer_value (json_object_get (json_track, "duration"));
    audio_track->owner_id     = (int) json_integer_value (json_object_get (json_track, "owner_id"));
    audio_track->title        = json_string_value (json_object_get (json_track, "title"));
    audio_track->url          = json_string_value (json_object_get (json_track, "url"));
}

gboolean
vk_audio_response_parse (const gchar *json,
                         VkAudioTrackCallback callback,
                         gpointer userdata,
                         GError **error) {
    json_t *root;
    json_error_t json_error;
    json_t *tmp_node;

    root = json_loads(json, 0, &json_error);

    if (!root) {
        trace("Unable to parse audio response: %s\n", json_error.text);
        json_error_to_g_error (&json_error, error);
        return FALSE;
    }

    if (!vk_error_check (root, error)) {
        trace("Error from VK: %d, %s\n", (*error)->code, (*error)->message);
        json_decref (root);
        return FALSE;
    }

    assert(json_is_object (root));
    tmp_node = json_object_get (root, "response");
    assert(json_is_array (tmp_node));

    for (size_t i = 0; i < json_array_size (tmp_node); i++) {
        VkAudioTrack audio_track;
        vk_audio_track_parse (tmp_node, i, &audio_track);
        callback(&audio_track, i, userdata);
    }

    json_decref (root);
    return TRUE;
}

VkAuthData *
vk_auth_data_parse (const gchar *auth_data_str) {
    if (auth_data_str == NULL || strlen (auth_data_str) == 0) {
        trace ("VK auth data missing\n");
        return NULL ;
    }
    VkAuthData *vk_auth_data = NULL;
    json_t *root;
    json_error_t error;

    root = json_loads(auth_data_str, 0, &error);
    if (!root) {
        trace ("VK auth data invalid\n");
        return NULL;
    }

    if (json_is_object (root)) {
        vk_auth_data = g_malloc (sizeof *vk_auth_data);
        vk_auth_data->access_token = g_strdup (json_string_value (json_object_get (root, "access_token")));
        vk_auth_data->user_id = json_integer_value (json_object_get (root, "user_id"));
        vk_auth_data->expires_in = json_integer_value (json_object_get (root, "expires_in"));
    }

    json_decref(root);
    return vk_auth_data;
}

void
vk_auth_data_free (VkAuthData *vk_auth_data) {
    if (vk_auth_data != NULL) {
        g_free ((gchar *) vk_auth_data->access_token);
        g_free (vk_auth_data);
    }
}
