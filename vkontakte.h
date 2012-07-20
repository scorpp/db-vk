#ifndef __VKONTAKTE_H
#define __VKONTAKTE_H

#include <glib/gprintf.h>

#define trace(...) { g_fprintf(stderr, __VA_ARGS__); }

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(X) gettext(X)
#else
#define _(X) (X)
#endif

#define VK_AUTH_URL "http://oauth.vkontakte.ru/authorize?client_id=3035566\
					&scope=audio,friends\
					&redirect_uri=http://scorpspot.blogspot.com\
					&response_type=token"
#define VK_API_URL "https://api.vk.com/method/"

typedef struct {
	const char *query;
} SEARCH_QUERY;

typedef struct {
	int aid;
	int owner_id;
	const char *artist;
	const char *title;
	int duration;	// seconds
	const char *url;
} VK_AUDIO_INFO;

#endif