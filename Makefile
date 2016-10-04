OUT_GTK2=vkontakte_gtk2.so
OUT_GTK3=vkontakte_gtk3.so
CC?=gcc
PREFIX?=/usr/local
GTK2_CFLAGS?=$(shell pkg-config --cflags gtk+-2.0 --silence-errors)
GTK2_LIBS?=$(shell pkg-config --libs gtk+-2.0 --silence-errors)
GTK3_CFLAGS?=$(shell pkg-config --cflags gtk+-3.0 --silence-errors)
GTK3_LIBS?=$(shell pkg-config --libs gtk+-3.0 --silence-errors)
JANSSON_CFLAGS?=`pkg-config --cflags jansson`
JANSSON_LIBS?=`pkg-config --libs jansson`
CURL_CFLAGS?=`pkg-config --cflags libcurl`
CURL_LIBS?=`pkg-config --libs libcurl`
CFLAGS+=-Wall -fPIC -D_GNU_SOURCE -g -O0 -std=c99 $(JANSSON_CFLAGS) $(CURL_CFLAGS)
LDFLAGS+=-shared $(JANSSON_LIBS) $(CURL_LIBS) -lssl
SOURCES=ui.c util.c vk-api.c vkontakte.c

OBJECTS_GTK2=$(SOURCES:.c=_gtk2.o)
OBJECTS_GTK3=$(SOURCES:.c=_gtk3.o)

ifneq ($(strip $(GTK2_CFLAGS)),)
	TARGETS+=$(OUT_GTK2)
endif
ifneq ($(strip $(GTK3_CFLAGS)),)
	TARGETS+=$(OUT_GTK3)
endif

ifndef TARGETS
	$(error Need at least on of GTK2 or GTK3 to build the plugin)
endif
$(info Building $(TARGETS))

.PHONY: install all clean
all: $(TARGETS)

install: all
	@if [ -f $(OUT_GTK2) ]; then \
		echo Installing $(PREFIX)/lib/deadbeef/$(OUT_GTK2); \
		install -D $(OUT_GTK2) $(PREFIX)/lib/deadbeef/$(OUT_GTK2); \
	fi
	@if [ -f $(OUT_GTK3) ]; then \
		echo Installing $(PREFIX)/lib/deadbeef/$(OUT_GTK3) \
		install -D $(OUT_GTK3) $(PREFIX)/lib/deadbeef/$(OUT_GTK3); \
	fi

$(OUT_GTK2): $(OBJECTS_GTK2)
	$(CC) $(OBJECTS_GTK2) $(LDFLAGS) $(GTK2_LIBS) -o $@

$(OUT_GTK3): $(OBJECTS_GTK3)
	$(CC) $(OBJECTS_GTK3) $(LDFLAGS) $(GTK3_LIBS) -o $@

%_gtk2.o: %.c
	$(CC) $(CFLAGS) $(GTK2_CFLAGS) $< -c -o $@

%_gtk3.o: %.c
	$(CC) $(CFLAGS) $(GTK3_CFLAGS) $< -c -o $@

clean:
	rm -f $(OBJECTS_GTK2) $(OBJECTS_GTK3) $(OUT_GTK2) $(OUT_GTK3)

