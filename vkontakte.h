#ifndef __VKONTAKTE_H
#define __VKONTAKTE_H

#include <glib/gprintf.h>

#define trace(...) { g_fprintf(stderr, __VA_ARGS__); }

typedef struct {
	gchar *query;
	GtkListStore *store;
} SEARCH_QUERY;


#endif
