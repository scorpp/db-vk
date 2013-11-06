/*
 * vk-api.h
 *
 *  Created on: Dec 9, 2012
 *      Author: scorp
 */

#ifndef VK_API_H_
#define VK_API_H_
G_BEGIN_DECLS

#include <json-glib/json-glib.h>


#define VK_API_URL "https://api.vk.com/method"
/** Search arbitrary tracks */
#define VK_API_METHOD_AUDIO_SEARCH VK_API_URL "/audio.search"
/** Retrieve 'My music' contents */
#define VK_API_METHOD_AUDIO_GET VK_API_URL "/audio.get"
/** Retrieve details of a track by it's ID */
#define VK_API_METHOD_AUDIO_GET_BY_ID VK_API_URL "/audio.getById"


typedef struct {
    const gchar *access_token;
    guint user_id;
    guint expires_in;
} VkAuthData;


#define VK_AUDIO_MAX_TRACKS 200
typedef struct {
    int aid;
    int owner_id;
    const gchar *artist;
    const gchar *title;
    int duration;   // seconds
    const char *url;
} VkAudioTrack;

typedef void (*VkAudioTrackCallback) (VkAudioTrack *track, guint index, gpointer userdata);

/**
 * Tries to parse given authentication data. Expects following JSON structure:
 * {
 *      'access_token': '...',
 *      'expires_in': 999999,
 *      'user_id': 9999999
 * }
 *
 * Returned instance should be disposed with vk_auth_data_free.
 *
 * @param auth_data_str authentication data string.
 * @return VkAuthData or NULL.
 */
VkAuthData *    vk_auth_data_parse (const gchar *auth_data_str);
/**
 * Dispose VkAuthData structures. Accepts NULLs.
 */
void            vk_auth_data_free (VkAuthData *vk_auth_data);

/**
 * Parse response of VK_API_METHOD_AUDIO_SEARCH or VK_API_METHOD_AUDIO_GET methods.
 * If FALSE was returned called is responsible for calling g_error_free on error
 * parameter.
 *
 * @return FALSE if error occurred, error details stored in error parameter.
 * TRUE otherwise
 */
gboolean        vk_audio_response_parse (const gchar *json,
                                         VkAudioTrackCallback callback,
                                         gpointer userdata,
                                         GError **error);

/**
 * Checks if response contains error, returns TRUE and sets error appropriately in
 * case of failure. Just returns FALSE otherwise.
 *
 * @return TRUE if response contains failure, FALSE otherwise.
 */
gboolean        vk_error_check (JsonParser *parser, GError **error);


G_END_DECLS
#endif /* VK_API_H_ */
