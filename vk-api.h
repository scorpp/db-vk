/*
 * vk-api.h
 *
 *  Created on: Dec 9, 2012
 *      Author: scorp
 */

#ifndef VK_API_H_
#define VK_API_H_

typedef struct {
    int aid;
    int owner_id;
    const gchar *artist;
    const gchar *title;
    int duration;   // seconds
    const char *url;
} VK_AUDIO_INFO;

#endif /* VK_API_H_ */
