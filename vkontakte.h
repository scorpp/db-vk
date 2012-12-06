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

typedef struct {
	gchar *query;
	GtkListStore *store;
} SEARCH_QUERY;

typedef struct {
	int aid;
	int owner_id;
	const gchar *artist;
	const gchar *title;
	int duration;	// seconds
	const char *url;
} VK_AUDIO_INFO;

#endif