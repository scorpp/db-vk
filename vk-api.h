/*
 * vk-api.h
 *
 *  Created on: Dec 9, 2012
 *      Author: scorp
 */

#ifndef VK_API_H_
#define VK_API_H_
G_BEGIN_DECLS

#define VK_API_URL "https://api.vk.com/method"
#define VK_API_METHOD_AUDIO_SEARCH VK_API_URL "/audio.search"

typedef struct {
    const gchar *access_token;
    guint user_id;
    guint expires_in;
} VkAuthData;

typedef struct {
    int aid;
    int owner_id;
    const gchar *artist;
    const gchar *title;
    int duration;   // seconds
    const char *url;
} VkAudioTrack;


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
 * Checks if response contains error, returns TRUE and sets error appropriately in
 * case of failure. Just returns FALSE otherwise.
 *
 * @return TRUE if response contains failure, FALSE otherwise.
 */
gboolean        vk_error_check (JsonParser *parser, GError **error);

G_END_DECLS
#endif /* VK_API_H_ */
