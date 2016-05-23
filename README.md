db-vk
=====
DeadBeef plugin for listening musing from vkontakte.com

For those speaking only Russian
--------
За помощью можно обращаться в <a href="http://vk.com/club53784333" target="_blank">группу на ВК</a>

Features
--------
  * Retrieve 'My Music' contents
  * Search VK.com for music
  * Removes duplicates in search results
  * Narrows search to specific phrase (in contrast to default behaviour which matches any single word from search query)
  * Allow searching in artist name or track title only
  * Copy track(s) URL to clipboard (for later download or whatever)

Track(s) can be added to current playlist by double click or with popup menu.  
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
and copy `vkontakte_gtk*.so` to `~/.local/lib/deadbeef` like this:
    
    mkdir -p ~/.local/lib/deadbeef
    cp vkontakte_gtk*.so ~/.local/lib/deadbeef/
Restart Deadbeeef player for it to load the plugin, now check out `File` menu

Packages
--------
[Arch Linux (AUR)] (https://aur.archlinux.org/packages/deadbeef-plugin-vk/)  
[Ubuntu 12.10 (GTK2 only build)] (https://github.com/scorpp/db-vk/releases). (Reported to work fine on Debian stable as well.) Package installs to /opt/deadbeef/ prefix as official Deadbeef package does. If you have a thirdparty package installed you may need to copy\symlink .so's to ~/.local/lib/deadbeef/ or /usr/lib/deadbeef/  
[Gentoo] (https://github.com/megabaks/stuff/tree/master/media-plugins/deadbeef-vk) (appreciations to @megabaks)

Contacts
--------
  * Found a bug or have a problem? Raise an issue here! (This method is appreciated)
  * Don't have a github account? Comment on <a href="http://vk.com/club53784333" target="_blank">VK group</a>
  * <a href="http://vk.com/scorpp" target="_blank">Me on vk.com</a>
  * <a href="http://gplus.to/scorpp" target="_blank">Me on Google+</a>
  * Email & GTalk: keryascorpio [at] gmail.com

I'll be glad to any kind of feedback from you! :)
