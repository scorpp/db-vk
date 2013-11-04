/*
 * vk-api.c
 *
 *  Created on: Dec 9, 2012
 *      Author: scorp
 */

#include <assert.h>
#include <string.h>
#include <glib-object.h>
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

typedef struct {
    VkAudioTrackCallback callback;
    gpointer callback_userdata;
} _VkAudioParseParams;

VkAuthData *
vk_auth_data_parse (const gchar *auth_data_str) {
    if (auth_data_str == NULL || strlen (auth_data_str) == 0) {
        trace ("VK auth data missing");
        return NULL ;
    }
    VkAuthData *vk_auth_data = NULL;
    GError *error;
    JsonParser *parser = json_parser_new ();

    if (!json_parser_load_from_data (parser, auth_data_str, strlen (auth_data_str), &error)) {
        trace ("VK auth data invalid");
        g_free (error);
        g_object_unref (parser);
        return NULL;
    }

    if (JSON_NODE_HOLDS_OBJECT (json_parser_get_root (parser))) {
        JsonObject *root = json_node_get_object (json_parser_get_root (parser));

        vk_auth_data = g_malloc (sizeof *vk_auth_data);
        vk_auth_data->access_token = g_strdup (json_object_get_string_member (root, "access_token"));
        vk_auth_data->user_id = json_object_get_int_member (root, "user_id");
        vk_auth_data->expires_in = json_object_get_int_member (root, "expires_in");
    }

    g_object_unref (parser);
    return vk_auth_data;
}

void
vk_auth_data_free (VkAuthData *vk_auth_data) {
    if (vk_auth_data != NULL) {
        g_free ((gchar *) vk_auth_data->access_token);
        g_free (vk_auth_data);
    }
}

static void
vk_audio_track_parse (JsonArray *array, guint index_, JsonNode *element_node, gpointer userdata) {
    _VkAudioParseParams *parse_params;
    JsonObject *json_track;
    VkAudioTrack audio_track;

    parse_params = (_VkAudioParseParams *) userdata;

    // first element may contain number of elements
    if (0 == index_
            && JSON_NODE_VALUE == json_node_get_node_type(element_node)) {
        return;
    }

    assert(JSON_NODE_HOLDS_OBJECT (element_node));
    json_track = json_node_get_object(element_node);

    // read data from json
    audio_track.aid            = json_object_get_int_member (json_track, "aid");
    audio_track.artist        = json_object_get_string_member (json_track, "artist");
    audio_track.duration     = json_object_get_int_member (json_track, "duration");
    audio_track.owner_id    = json_object_get_int_member (json_track, "owner_id");
    audio_track.title        = json_object_get_string_member (json_track, "title");
    audio_track.url            = json_object_get_string_member (json_track, "url");

    parse_params->callback(&audio_track, index_, parse_params->callback_userdata);
}

gboolean
vk_audio_response_parse (const gchar *json,
                         VkAudioTrackCallback callback,
                         gpointer userdata,
                         GError **error) {
    JsonParser *parser;
    JsonNode *root;
    JsonArray *tracks;
    JsonObject *tmp_obj;
    JsonNode *tmp_node;
    _VkAudioParseParams parse_params;

    parse_params.callback = callback;
    parse_params.callback_userdata = userdata;

    parser = json_parser_new();

    if (!json_parser_load_from_data(parser, json, -1, error)) {
        trace("Unable to parse audio response: %s\n", (*error)->message);
        g_object_unref(parser);
        return FALSE;
    }

    if (!vk_error_check(parser, error)) {
        trace("Error from VK: %d, %s\n", (*error)->code, (*error)->message);
        g_object_unref(parser);
        return FALSE;
    }

    root = json_parser_get_root(parser);

    assert(JSON_NODE_HOLDS_OBJECT (root));
    tmp_obj = json_node_get_object(root);
    tmp_node = json_object_get_member(tmp_obj, "response");
    assert(JSON_NODE_HOLDS_ARRAY (tmp_node));

    tracks = json_node_get_array(tmp_node);
    json_array_foreach_element(tracks, vk_audio_track_parse, &parse_params);

    g_object_unref(parser);
    return TRUE;
}

gboolean
vk_error_check (JsonParser *parser, GError **error) {
    JsonNode *root = json_parser_get_root (parser);

    if (!JSON_NODE_HOLDS_OBJECT (root) ) {
        *error = g_error_new_literal (g_quark_from_static_string (VK_API_DOMAIN_STR),
                                      -1,
                                      "Root should be a JSON object");
        return FALSE;
    }

    JsonObject * rootObj = json_node_get_object (root);
    if (json_object_has_member (rootObj, VK_ERR_JSON_KEY) ) {
        rootObj = json_object_get_object_member (rootObj, VK_ERR_JSON_KEY);
        assert (json_object_has_member (rootObj, VK_ERR_JSON_MSG_KEY) );

        *error = g_error_new_literal (g_quark_from_static_string (VK_API_DOMAIN_STR),
                                      json_object_get_int_member (rootObj, VK_ERR_JSON_CODE_KEY),
                                      g_strdup (json_object_get_string_member (rootObj, VK_ERR_JSON_MSG_KEY) ) );
        return FALSE;
    }
    return TRUE;
}
