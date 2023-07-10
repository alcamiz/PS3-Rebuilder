#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "util.h"
#include "sfo.h"
#include "fault.h"
#include "cwalk.h"

static
error_state_t calc_mgz_meta(sfo_t *sfo, uint32_t *sig) {

    error_state_t ret_val;
    uint32_t crc;

    if (sfo == NULL || sig == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

	crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, (const unsigned char*) sfo->title_id, 9);
	crc = crc32(crc, (const unsigned char*) sfo->sys_ver, 4);
	crc = crc32(crc, (const unsigned char*) sfo->disc_ver, 5);
	crc = crc32(crc, (const unsigned char*) sfo->app_ver, 5);

    *sig = crc;
    ret_val = EXIT_OK;

    exit_normal:
	    return ret_val;
}

error_state_t load_sfo(sfo_t *sfo, char *sfo_path) {

    error_state_t ret_val;
    size_t obtained;
    uint32_t key_table_size;
    FILE *sfo_file;

    sfo_header_t header;
    sfo_index_table_entry_t *index_table, *cur_entry;
    char *key_table, *cur_key, *cur_data;

    if (sfo == NULL || sfo_path == NULL) {
        ret_val = ARG_ERROR;
        goto exit_early;
    }

    sfo_file = fopen(sfo_path, "r");
    if (sfo_file == NULL) {
        ret_val = F_OPEN_ERROR;
        goto exit_early;
    }

    obtained = fread(&header, sizeof(header), 1, sfo_file);
    if (obtained != 1) {
        ret_val = F_READ_ERROR;
        goto exit_file;
    }

    key_table_size = header.data_table_start - header.key_table_start;

    index_table = malloc(sizeof(*index_table)*header.tables_entries);
    if (index_table == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_file;
    }

    obtained = fread(index_table, sizeof(*index_table), header.tables_entries, sfo_file);
    if (obtained != header.tables_entries) {
        ret_val = F_READ_ERROR;
        goto exit_index;
    }

    key_table = malloc(key_table_size);
    if (key_table == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_index;
    }

    obtained = fread(key_table, sizeof(*key_table), key_table_size, sfo_file);
    if (obtained != key_table_size) {
        ret_val = F_READ_ERROR;
        goto exit_normal;
    }

    for (int i = 0; i < header.tables_entries; i++) {
        cur_entry = index_table + i;
        cur_key = key_table + cur_entry->key_offset;

        cur_data = malloc(cur_entry->data_len);
        if (cur_data == NULL) {
            ret_val = ALLOC_ERROR;
            goto exit_normal;
        }

        if (fseek(sfo_file, header.data_table_start + cur_entry->data_offset, SEEK_SET) != 0) {
            ret_val = F_SEEK_ERROR;
            goto exit_loop;
        }

        obtained = fread(cur_data, sizeof(char), cur_entry->data_len, sfo_file);
        if (obtained != cur_entry->data_len) {
            ret_val = F_READ_ERROR;
            goto exit_loop;
        }
        
        if (strcmp(cur_key, "TITLE_ID") == 0) {
            memcpy((char *) sfo->title_id, cur_data, 9);
            sfo->title_id[9] = '\0';
        }

        else if (strcmp(cur_key, "PS3_SYSTEM_VER") == 0) {
            memcpy((char *) sfo->sys_ver, cur_data+1, 4);
            sfo->sys_ver[4] = '\0';
        }

        else if (strcmp(cur_key, "VERSION") == 0) {
            memcpy((char *) sfo->disc_ver, cur_data, 5);
            sfo->disc_ver[5] = '\0';
        }

        else if (strcmp(cur_key, "APP_VER") == 0) {
            memcpy((char *) sfo->app_ver, cur_data, 5);
            sfo->title_id[5] = '\0';
        }

        free(cur_data);
    }

    ret_val = calc_mgz_meta(sfo, &sfo->mgz_sig);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_loop:
        free(cur_data);
    exit_normal:
        free(key_table);
    exit_index:
        free(index_table);
    exit_file:
        fclose(sfo_file);
    exit_early:
        return ret_val;
}

error_state_t print_sfo(sfo_t *sfo) {
    if (sfo == NULL) return ARG_ERROR;
    printf("%s %s %s %s\n", sfo->title_id, sfo->sys_ver, sfo->disc_ver, sfo->app_ver);
    return EXIT_OK;
}
