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
    
    mkdir -p ~/.local/lib/deadbeef
    cp vkontakte.so ~/.local/lib/deadbeef/
Restart Deadbeeef player for it to load the plugin, now check out `File` menu

Packages
--------
[Arch Linux (AUR)] (https://aur.archlinux.org/packages/deadbeef-plugin-vk/)  
[Ubuntu 12.10 (GTK2 only build)] (https://docs.google.com/folder/d/0B6EJOKa6vq1kbmd1cjJXZzNjbUU/edit) pick latest one. (Reported to work fine on Debian stable as well.) Package installs to /opt/deadbeef/ prefix as official Deadbeef package does. If you have a thirdparty package installed you may need to copy\symlink .so's to ~/.local/lib/deadbeef/ or /usr/lib/deadbeef/

Contacts
--------
  * Found a bug or have a problem? Raise an issue here! (This method is appreciated)
  * <a href="http://vk.com/scorpp" target="_blank">Me on vk.com</a>
  * <a href="http://gplus.to/scorpp" target="_blank">Me on Google+</a>
  * Email & GTalk: keryascorpio [at] gmail.com

I'll be glad to any kind of feedback from you! :)
