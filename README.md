db-vk
=====
DeadBeef plugin for listening musing from vkontakte.com

Features
--------
It can only show list of your music. Track is added to current playlist by double click.  
That's it for now.

Installation
------------
### Dependencies
 * gtk+ (2 or 3 - should correspond to GTK version your Deadbeef is built with)
 * json-glib
 * curl

### Building
Build it with

    gcc -c vkontakte.c -o vkontakte.so -g -std=c99 -I. \
        `pkg-config --cflags --libs gtk+-3.0` \
        `pkg-config --cflags --libs json-glib-1.0` \
        `pkg-config --cflags --libs libcurl`
and copy `vkontakte.so` to `~/.local/lib/deadbeef` like this:
    
    mkdir -p ~/.local/lib/deabeef
    cp vkontakte.so ~/.local/lib/deadbeef/
Restart Deadbeeef player for it to load the plugin, now check out `File` menu