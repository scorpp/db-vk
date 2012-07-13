if HAVE_NULLOUT
vkontaktedir = $(libdir)/$(PACKAGE)
pkglib_LTLIBRARIES = vkontakte.la
vkontakte_la_SOURCES = vkontakte.c
vkontakte_la_LDFLAGS = -module
# GTK2
vkontakte_la_LIBADD = $(LDADD) $(GTK2_DEPS_LIBS)
vkontakte_la_CFLAGS = -std=c99 $(GTK2_DEPS_CFLAGS)

AM_CFLAGS = $(CFLAGS) -std=c99
endif
