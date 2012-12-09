/*
 * vk-api.c
 *
 *  Created on: Dec 9, 2012
 *      Author: scorp
 */

#include <assert.h>
#include <string.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>

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

VkAuthData *
vk_auth_data_parse (const gchar *auth_data_str) {
    if (auth_data_str == NULL || strlen (auth_data_str) == 0) {
        trace ("VK auth data missing");
        return NULL ;
    }
    VkAuthData *vk_auth_data;
    GError *error;
    JsonParser *parser = json_parser_new ();

    if (!json_parser_load_from_data (parser, auth_data_str, strlen (auth_data_str), &error)) {
        trace ("VK auth data invalid");
        g_free (error);
        g_object_unref (parser);
    }

    assert (JSON_NODE_HOLDS_OBJECT (json_parser_get_root (parser)));
    JsonObject *root = json_node_get_object (json_parser_get_root (parser));

    vk_auth_data = g_malloc (sizeof *vk_auth_data);
    vk_auth_data->access_token = g_strdup (json_object_get_string_member (root, "access_token"));
    vk_auth_data->user_id = json_object_get_int_member (root, "user_id");
    vk_auth_data->expires_in = json_object_get_int_member (root, "expires_in");

    g_object_unref (parser);
    return vk_auth_data;
}

void vk_auth_data_free (VkAuthData *vk_auth_data) {
    if (vk_auth_data != NULL ) {
        g_free ((gchar *) vk_auth_data->access_token);
        g_free (vk_auth_data);
    }
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
