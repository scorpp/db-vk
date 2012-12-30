db-vk
=====
DeadBeef plugin for listening musing from vkontakte.com

Features
--------
It can search music across VK.com or display 'My Music'. Track is added to current playlist by double click or with popup menu.  
That's it for now.

Installation
------------
### Dependencies
 * gtk+ (2 or 3 - should correspond to GTK version your Deadbeef is built with)
 * json-glib
 * curl
 * cmake

### Building
Build it with

    cmake .
    make
and copy `vkontakte.so` to `~/.local/lib/deadbeef` like this:
    
    mkdir -p ~/.local/lib/deabeef
    cp vkontakte.so ~/.local/lib/deadbeef/
Restart Deadbeeef player for it to load the plugin, now check out `File` menu

Packages
--------
[Arch Linux AUR] (https://aur.archlinux.org/packages/deadbeef-plugin-vk/)  
[Ubuntu 12.10 (GTK2 only build)] (https://docs.google.com/folder/d/0B6EJOKa6vq1kbmd1cjJXZzNjbUU/edit) pick latest one
