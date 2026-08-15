/*******************************************************************
 * File:        url_fetch_posix
 * Purpose:     libcurl-based HTTP transport for the POSIX oras build
 * Author:      Gerph
 ******************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <curl/curl.h>

#include "oras_types.h"
#include "url_fetch.h"

typedef struct fetch_state_s
{
    url_fetch_response_t *response;
    unsigned char *body;
    size_t body_used;
    size_t body_allocated;
    char *error;
    size_t error_size;
    int failed;
} fetch_state_t;

/*************************************************** Gerph *********
 Function:      append_bytes
 Description:   Append data to a growable buffer, expanding it as needed
 Parameters:    buffer-> pointer to the buffer pointer, grown as required
                used-> pointer to the count of bytes currently used
                allocated-> pointer to the count of bytes currently allocated
                data-> the bytes to append
                length = number of bytes to append
 Returns:       1 if successful, 0 if failed
 ******************************************************************/
static int append_bytes(unsigned char **buffer, size_t *used, size_t *allocated,
                        const unsigned char *data, size_t length)
{
    unsigned char *grown;
    size_t new_allocated;

    if (*used + length > *allocated)
    {
        new_allocated = *allocated == 0 ? 4096 : *allocated * 2;
        while (new_allocated < *used + length)
        {
            new_allocated *= 2;
        }
        grown = oras_reallocarray(*buffer, new_allocated, 1);
        if (grown == NULL)
        {
            return 0;
        }
        *buffer = grown;
        *allocated = new_allocated;
    }
    memcpy(*buffer + *used, data, length);
    *used += length;
    return 1;
}

/*************************************************** Gerph *********
 Function:      body_callback
 Description:   libcurl CURLOPT_WRITEFUNCTION: accumulate response body
                bytes, discarding anything captured for an earlier
                (redirected-away-from) response
 Parameters:    data-> the bytes received
                size = element size (always 1)
                nmemb = element count
                userdata-> the fetch_state_t for this request
 Returns:       number of bytes accepted; anything less aborts the transfer
 ******************************************************************/
static size_t body_callback(char *data, size_t size, size_t nmemb, void *userdata)
{
    fetch_state_t *state;
    size_t length;

    state = (fetch_state_t *)userdata;
    length = size * nmemb;
    if (state->failed)
    {
        return length;
    }
    if (!append_bytes(&state->body, &state->body_used, &state->body_allocated,
                      (const unsigned char *)data, length))
    {
        oras_set_error(state->error, state->error_size, "No memory for response body");
        state->failed = 1;
        return 0;
    }
    return length;
}

/*************************************************** Gerph *********
 Function:      header_callback
 Description:   libcurl CURLOPT_HEADERFUNCTION: record response headers,
                restarting the recorded set whenever a new status line
                is seen so that only the final hop of a redirect chain
                is kept
 Parameters:    data-> the header line received (not NUL terminated)
                size = element size (always 1)
                nmemb = element count
                userdata-> the fetch_state_t for this request
 Returns:       number of bytes accepted; anything less aborts the transfer
 ******************************************************************/
static size_t header_callback(char *data, size_t size, size_t nmemb, void *userdata)
{
    fetch_state_t *state;
    size_t length;
    char *line;
    char *colon;
    char *value;

    state = (fetch_state_t *)userdata;
    length = size * nmemb;
    if (state->failed || length == 0)
    {
        return length;
    }
    if (length >= 5 && strncmp(data, "HTTP/", 5) == 0)
    {
        /* A new response block is starting (either the real response, or
           one more hop of a redirect chain): discard anything recorded
           for an earlier hop and start again. */
        oras_free_response(state->response);
        return length;
    }
    line = oras_strndup(data, length);
    if (line == NULL)
    {
        oras_set_error(state->error, state->error_size, "No memory for response header");
        state->failed = 1;
        return 0;
    }
    while (strlen(line) > 0 &&
           (line[strlen(line) - 1] == '\r' || line[strlen(line) - 1] == '\n'))
    {
        line[strlen(line) - 1] = '\0';
    }
    colon = strchr(line, ':');
    if (colon == NULL || line[0] == '\0')
    {
        /* Blank line (end of headers) or a malformed line; nothing to record */
        free(line);
        return length;
    }
    *colon++ = '\0';
    while (*colon == ' ' || *colon == '\t')
    {
        ++colon;
    }
    value = colon;
    if (!oras_set_annotation((oras_annotation_t **)&state->response->headers,
                             &state->response->header_count,
                             line, value, state->error, state->error_size))
    {
        free(line);
        state->failed = 1;
        return 0;
    }
    free(line);
    return length;
}

int url_fetch_execute(const url_fetch_request_t *request,
                      url_fetch_response_t *response,
                      char *error,
                      size_t error_size)
{
    CURL *curl;
    CURLcode result;
    struct curl_slist *header_list;
    fetch_state_t state;
    long status_code;
    size_t i;
    char header_line[1024];

    memset(response, 0, sizeof(*response));
    memset(&state, 0, sizeof(state));
    state.response = response;
    state.error = error;
    state.error_size = error_size;

    curl = curl_easy_init();
    if (curl == NULL)
    {
        oras_set_error(error, error_size, "Unable to initialise libcurl");
        return 0;
    }

    header_list = NULL;
    for (i = 0; i < request->header_count; ++i)
    {
        if (snprintf(header_line, sizeof(header_line), "%s: %s",
                     request->headers[i].name, request->headers[i].value) >=
            (int)sizeof(header_line))
        {
            oras_set_error(error, error_size, "Request header too long");
            curl_slist_free_all(header_list);
            curl_easy_cleanup(curl);
            return 0;
        }
        header_list = curl_slist_append(header_list, header_line);
    }
    if (request->body_size != 0)
    {
        /* Registries generally reject the "Expect: 100-continue" curl adds
           by default for larger request bodies; suppress it. */
        header_list = curl_slist_append(header_list, "Expect:");
    }

    curl_easy_setopt(curl, CURLOPT_URL, request->url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "oras-posix/0.1");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, body_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &state);

    if (strcmp(request->method, "GET") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }
    else if (strcmp(request->method, "HEAD") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }
    else if (strcmp(request->method, "POST") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)request->body_size);
    }
    else if (strcmp(request->method, "PUT") == 0 || strcmp(request->method, "DELETE") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request->method);
        if (request->body_size != 0)
        {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)request->body_size);
        }
    }
    else
    {
        oras_set_error(error, error_size, "Unsupported HTTP method '%s'", request->method);
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        return 0;
    }

    result = curl_easy_perform(curl);
    if (result != CURLE_OK || state.failed)
    {
        if (!state.failed)
        {
            oras_set_error(error, error_size, "HTTP transfer failed: %s",
                           curl_easy_strerror(result));
        }
        free(state.body);
        oras_free_response(response);
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        return 0;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    response->status_code = (int)status_code;
    response->body = state.body;
    response->body_size = state.body_used;

    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    return 1;
}
