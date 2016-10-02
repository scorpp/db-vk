//
// Created by scorp on 10/2/16.
//

#ifndef DB_VK_UTIL_H
#define DB_VK_UTIL_H
G_BEGIN_DECLS

#include <glib.h>


gchar *http_get_string (const gchar *url, GError **error);

/**
 * Replaces in the string str all the occurrences of the source string from with the destination string to.
 * The lengths of the strings from and to may differ. The string to may be of any length, but the string
 * from must be of non-zero length - the penalty for providing an empty string for the from parameter is an
 * infinite loop. In addition, none of the three parameters may be NULL.
 *
 * http://creativeandcritical.net/str-replace-c
 * @return The post-replacement string, or NULL if memory for the new string could not be allocated. Does
 *         not modify the original string. The memory for the returned post-replacement string may be
 *         deallocated with the standard library function free when it is no longer required.
 */
char *repl_str(const char *str, const char *from, const char *to);

G_END_DECLS
#endif //DB_VK_UTIL_H
