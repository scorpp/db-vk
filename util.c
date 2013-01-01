/*
 * util.c
 *
 *  Created on: Dec 9, 2012
 *  Author: scorp
 */
#include <curl/curl.h>
#include <glib.h>

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
    gchar *curl_err_buf;

    curl = curl_easy_init ();
    resp_str = g_string_sized_new (1024 * 3);

    curl_err_buf = g_malloc (CURL_ERROR_SIZE);

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
        g_free (curl_err_buf);
        curl_easy_cleanup (curl);
        return NULL;
    }

    g_free (curl_err_buf);
    curl_easy_cleanup (curl);

    // return as simple gchar *
    gchar *ret = resp_str->str;
    g_string_free (resp_str, FALSE);
    return ret;
}
