/*
 * util.c
 *
 *  Created on: Dec 9, 2012
 *  Author: scorp
 */
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#if (__STDC_VERSION__ >= 199901L)
#include <stdint.h>
#endif

#include "common-defs.h"


static size_t
http_write_data (char *ptr, size_t size, size_t nmemb, void *userdata) {
    g_string_append_len ((GString *) userdata, ptr, size * nmemb);
    return size * nmemb;
}

gchar *
http_get_string (const gchar *url, GError **error) {
    CURL *curl;
    GString *resp_str;
    char curl_err_buf[CURL_ERROR_SIZE];

    curl = curl_easy_init ();
    resp_str = g_string_sized_new (1024 * 3);


    trace ("Requesting URL %s\n", url);

    curl_easy_setopt (curl, CURLOPT_URL, url);
    curl_easy_setopt (curl, CURLOPT_USERAGENT, "DeadBeef");
    curl_easy_setopt (curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    // enable up to 10 redirects
    curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt (curl, CURLOPT_MAXREDIRS, 10);
    // setup handlers
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, http_write_data);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, resp_str);
    curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_err_buf);

    int status = curl_easy_perform (curl);
    if (status != 0) {
        trace ("Curl error: %s\n", curl_err_buf);
        *error = g_error_new (g_quark_from_static_string ("vk plugin curl error"),
                              status,
                              "%s",
                              curl_err_buf);
    }

    curl_easy_cleanup (curl);

    // return NULL in case of curl error, char buffer otherwise
    return g_string_free (resp_str, status != 0);
}

char *
repl_str(const char *str, const char *from, const char *to) {

    /* Adjust each of the below values to suit your needs. */

    /* Increment positions cache size initially by this number. */
    size_t cache_sz_inc = 16;
    /* Thereafter, each time capacity needs to be increased,
     * multiply the increment by this factor. */
    const size_t cache_sz_inc_factor = 3;
    /* But never increment capacity by more than this number. */
    const size_t cache_sz_inc_max = 1048576;

    char *pret, *ret = NULL;
    const char *pstr2, *pstr = str;
    size_t i, count = 0;
#if (__STDC_VERSION__ >= 199901L)
    uintptr_t *pos_cache_tmp, *pos_cache = NULL;
#else
    ptrdiff_t *pos_cache_tmp, *pos_cache = NULL;
#endif
    size_t cache_sz = 0;
    size_t cpylen, orglen, retlen, tolen, fromlen = strlen(from);

    /* Find all matches and cache their positions. */
    while ((pstr2 = strstr(pstr, from)) != NULL) {
        count++;

        /* Increase the cache size when necessary. */
        if (cache_sz < count) {
            cache_sz += cache_sz_inc;
            pos_cache_tmp = realloc(pos_cache, sizeof(*pos_cache) * cache_sz);
            if (pos_cache_tmp == NULL) {
                goto end_repl_str;
            } else pos_cache = pos_cache_tmp;
            cache_sz_inc *= cache_sz_inc_factor;
            if (cache_sz_inc > cache_sz_inc_max) {
                cache_sz_inc = cache_sz_inc_max;
            }
        }

        pos_cache[count-1] = pstr2 - str;
        pstr = pstr2 + fromlen;
    }

    orglen = pstr - str + strlen(pstr);

    /* Allocate memory for the post-replacement string. */
    if (count > 0) {
        tolen = strlen(to);
        retlen = orglen + (tolen - fromlen) * count;
    } else	retlen = orglen;
    ret = malloc(retlen + 1);
    if (ret == NULL) {
        goto end_repl_str;
    }

    if (count == 0) {
        /* If no matches, then just duplicate the string. */
        strcpy(ret, str);
    } else {
        /* Otherwise, duplicate the string whilst performing
         * the replacements using the position cache. */
        pret = ret;
        memcpy(pret, str, pos_cache[0]);
        pret += pos_cache[0];
        for (i = 0; i < count; i++) {
            memcpy(pret, to, tolen);
            pret += tolen;
            pstr = str + pos_cache[i] + fromlen;
            cpylen = (i == count-1 ? orglen : pos_cache[i+1]) - pos_cache[i] - fromlen;
            memcpy(pret, pstr, cpylen);
            pret += cpylen;
        }
        ret[retlen] = '\0';
    }

    end_repl_str:
    /* Free the cache and return the post-replacement string,
     * which will be NULL in the event of an error. */
    free(pos_cache);
    return ret;
}
