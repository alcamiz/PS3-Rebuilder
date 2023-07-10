#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <curl/curl.h>

#include "cwalk.h"
#include "net.h"
#include "util.h"
#include "sfo.h"
#include "fault.h"

static
size_t memory_callback(void *ptr, size_t size, size_t nmemb, void *data) {

    size_t total_size;
    memory_wrapper_t *mem;

    total_size = size * nmemb;
    mem = (memory_wrapper_t *) data;

    assert(mem->memory != NULL);
    assert(mem->size + total_size < BUFF_SIFE);

    memcpy(mem->memory + mem->size, ptr, total_size);
    mem->size += total_size;
    mem->memory[mem->size] = '\0';

    return total_size;
}

static
error_state_t url_to_memory(char *url, memory_wrapper_t *mem_wrapper) {

    error_state_t ret_val;
    CURL *curl;
    CURLcode res;

    if (url == NULL || mem_wrapper == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }
    
    curl = curl_easy_init();
    if (curl == NULL) {
        ret_val = CURL_INIT_ERROR;
        goto exit_normal;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) mem_wrapper);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        ret_val = CURL_PERF_ERROR;
        goto exit_normal;
    }

    ret_val = EXIT_OK;
    exit_normal:
        return ret_val;
}

static
error_state_t url_to_file(char *url, char *file_path) {

    error_state_t ret_val;
    CURL *curl;
    FILE *file;
    CURLcode res;

    if (url == NULL || file_path == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    curl = curl_easy_init();
    if (curl == NULL) {
        ret_val = CURL_INIT_ERROR;
        goto exit_normal;
    }

    file = fopen(file_path, "w");
    if (file == NULL) {
        ret_val = F_OPEN_ERROR;
        goto exit_normal;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) file);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(file);
    
    if (res != CURLE_OK) {
        remove(file_path);
        ret_val = CURL_PERF_ERROR;
        goto exit_normal;
    }

    ret_val = EXIT_OK;
    exit_normal:
        return ret_val;
}

error_state_t download_ird(sfo_t *sfo, char *ird_path) {

    error_state_t ret_val;
    int obtained;
    char *url;
    memory_wrapper_t *url_wrapper;

    if (sfo == NULL || ird_path == NULL) {
        ret_val = ARG_ERROR;
        goto exit_early;
    }

    url = malloc(MAX_PATH_LEN);
    if (url == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_early;
    }

    obtained = snprintf(url, MAX_PATH_LEN, "%s/%s=%08X", IRD_LINK, IRD_REQ, sfo->mgz_sig);
    if (obtained < 0) {
        ret_val = PATH_BUFFER_ERROR;
        goto exit_url;
    }

    url_wrapper = calloc(1, sizeof(*url_wrapper));
    if (url_wrapper == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_url;
    }

    url_wrapper->memory = malloc(BUFF_SIFE);
    if (url_wrapper->memory == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_wrapper;
    }

    ret_val = url_to_memory(url, url_wrapper);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }

    obtained = snprintf(url, MAX_PATH_LEN, "%s/%s", IRD_LINK, strtok(url_wrapper->memory, "\n"));
    if (obtained < 0) {
        ret_val = PATH_BUFFER_ERROR;
        goto exit_normal;
    }

    ret_val = url_to_file(url, ird_path);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }

    ret_val = EXIT_OK;
    exit_normal:
        free(url_wrapper->memory);
    exit_wrapper:
        free(url_wrapper);
    exit_url:
        free(url);
    exit_early:
        return ret_val;
}

error_state_t download_pup(sfo_t *sfo, char *pup_path) {

    error_state_t ret_val;
    int obtained;
    char *url;

    if (sfo == NULL || pup_path == NULL) {
        ret_val = ARG_ERROR;
        goto exit_early;
    }

    url = malloc(MAX_PATH_LEN);
    if (url == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_early;
    }

    obtained = snprintf(url, MAX_PATH_LEN, "%s/%s=%s", PUP_LINK, PUP_REQ, sfo->sys_ver);
    if (obtained < 0) {
        ret_val = PATH_BUFFER_ERROR;
        goto exit_normal;
    }

    ret_val = url_to_file(url, pup_path);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }

    ret_val = EXIT_OK;
    exit_normal:
        free(url);
    exit_early:
        return ret_val;
}
