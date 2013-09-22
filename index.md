---
layout: master
lang: ru
title: DeaDBeef VKontakte plugin by scorpp
---
db-vk
=====
Плагин плеера DeaDBeeF для прослушивания музыки из vkontakte.com

## Фичи {#features}
Плагин умеет:
 
 * Получать списой "Моей музыки"
 * Искать музыку на VK.com
 * Чистить дубликаты в результатах поиска
 * Сужать поиск до фразы целиком (в то время как по умолчанию совпадением считается вхождение любого отдельного слова из запроса)
 * Позволяет искать только в имени исполнителя или названии трека
 * Копировать ссылки на треки (для последующего скачивания или чего-то ещё)

Треки можно добавлять в текущий плейлист двойным кликом либо через контекстное меню.  
Вот и всё.

## Пакеты {#packages}

 * [Arch Linux (AUR)](https://aur.archlinux.org/packages/deadbeef-plugin-vk/)  
 * [Ubuntu (х32 GTK2-only build)](https://github.com/scorpp/db-vk/releases). (Должно работать и на Debian stable.) Пакет ставит плагин в /opt/deadbeef/, куда ставится официальный пакет DeaDBeeF. Если у вас установлен плеер другой сборки, может понадобится скопировать или создать симлинк на плагин в ~/.local/lib/deadbeef/ или /usr/lib/deadbeef/  
 * [Gentoo](https://github.com/megabaks/stuff/tree/master/media-plugins/deadbeef-vk) (спасибо @megabaks)

К сожалению, сейчас нет возможности собирать пакеты для amd64.

## Сборка из исходников {#building-from-source}

### Зависимости
 * gtk+ (2 or 3 - версия должна соответствовать той версии GTK, с которой у вас собран Deadbeef)
 * json-glib
 * curl
 * cmake

### Сборка
Для сборки достаточно из каталога с исходниками выполнить

    cmake .
    make

и скопировать `vkontakte_gtk*.so` в `~/.local/lib/deadbeef` например так:
    
    mkdir -p ~/.local/lib/deadbeef
    cp vkontakte_gtk*.so ~/.local/lib/deadbeef/

Перезапустите Deadbeeef, чтобы загрузить плагин, и загляните в пункт меню "Файл".

## Контакты {#contacts}

 * Нашли ошибку или столкнулись с проблемой? [Заведите тикет!](https://github.com/scorpp/db-vk/issues) (This method is appreciated)
 * Нету аккаунта на github? Оставьте коментарий в <a href="http://vk.com/club53784333" target="_blank">группе на VK</a>
 * Я <a href="http://vk.com/scorpp" target="_blank">на vk.com</a>
 * Я <a href="http://gplus.to/scorpp" target="_blank">в Google+</a>
 * Email & GTalk: keryascorpio_at_gmail.com

Буду рад любым отзывам! :)
