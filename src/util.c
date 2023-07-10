#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <mbedtls/md5.h>
#include <zlib.h>

#include "util.h"
#include "fault.h"

error_state_t utf16_to_utf8(uint16_t *stw, uint8_t *stb) {

    if (stw == NULL || stb == NULL) {
        return ARG_ERROR;
    }

    while(SWAP16(stw[0])) {

        if((SWAP16(stw[0]) & 0xFF80) == 0) {
            *(stb++) = SWAP16(stw[0]) & 0xFF;

        } else if((SWAP16(stw[0]) & 0xF800) == 0) {
            *(stb++) = ((SWAP16(stw[0])>>6) & 0xFF) | 0xC0; *(stb++) = (SWAP16(stw[0]) & 0x3F) | 0x80;

        } else if((SWAP16(stw[0]) & 0xFC00) == 0xD800 && (SWAP16(stw[1]) & 0xFC00) == 0xDC00 ) {
            *(stb++)= (((SWAP16(stw[0]) + 64)>>8) & 0x3) | 0xF0; *(stb++)= (((SWAP16(stw[0])>>2) + 16) & 0x3F) | 0x80; 
            *(stb++)= ((SWAP16(stw[0])>>4) & 0x30) | 0x80 | ((SWAP16(stw[1])<<2) & 0xF); *(stb++)= (SWAP16(stw[1]) & 0x3F) | 0x80;
            stw++;

        } else {
            *(stb++)= ((SWAP16(stw[0])>>12) & 0xF) | 0xE0; *(stb++)= ((SWAP16(stw[0])>>6) & 0x3F) | 0x80; *(stb++)= (SWAP16(stw[0]) & 0x3F) | 0x80;
        } 
        stw++;
    }
    *stb = 0;
    return EXIT_OK;
}

error_state_t calc_checksum(uint8_t *checksum, char *file_path) {

    error_state_t ret_val;
    int md5_val;
    size_t obtained;
    void *buffer;
    FILE *file;
    mbedtls_md5_context ctx;

    if (checksum == NULL || file_path == NULL) {
        ret_val = ARG_ERROR;
        goto exit_early;
    }

    file = fopen(file_path, "r");
    if (file == NULL) {
        ret_val = F_OPEN_ERROR;
        goto exit_early;
    }

    mbedtls_md5_init(&ctx);
    md5_val = mbedtls_md5_starts_ret(&ctx);
    if (md5_val != 0) {
        ret_val = MD5_START_ERROR;
        goto exit_early;
    }

    buffer = malloc(BUFF_SIFE);
    if (buffer == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_md5;
    }

    obtained = BUFF_SIFE;

    while (obtained == BUFF_SIFE) {
        obtained = fread(buffer, sizeof(uint8_t), BUFF_SIFE, file);
        if (obtained < 0) {
            ret_val = F_READ_ERROR;
            goto exit_normal;
        }
        if (obtained == 0) break;
        md5_val = mbedtls_md5_update_ret(&ctx, buffer, obtained);
        if (md5_val != 0) {
            ret_val = MD5_UPDT_ERROR;
            goto exit_normal;
        }
    }
    md5_val = mbedtls_md5_finish_ret(&ctx, checksum);
    if (md5_val != 0) {
        ret_val = MD5_END_ERROR;
        goto exit_normal;
    }

    ret_val = EXIT_OK;

    exit_normal:
        free(buffer);
    exit_md5:
        mbedtls_md5_free(&ctx);
    exit_file:
        fclose(file);
    exit_early:
        return ret_val;
}

error_state_t zero_out_file(FILE *in_file, off_t size) {

    error_state_t ret_val;
    off_t write_total;
    size_t obtained, write_size;
    char *buffer;

    if (in_file == NULL) {
        ret_val = ARG_ERROR;
        goto exit_early;
    }

    if (size == 0) {
        ret_val = EXIT_OK;
        goto exit_early;
    }

    buffer = calloc(BUFF_SIFE, sizeof(*buffer));
    if (buffer == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_early;
    }

    write_total = 0;
    write_size = 0;
    obtained = 0;

    while (write_total != size) {
        write_size = min(BUFF_SIFE, size - write_total);
        obtained = fwrite(buffer, sizeof(*buffer), write_size, in_file);
        if (obtained < 0) {
            ret_val = F_WRITE_ERROR;
            goto exit_normal;
        }
        write_total += write_size;
        if (obtained != write_size) break;
    }

    ret_val = EXIT_OK;

    exit_normal:
        free(buffer);
    exit_early:
        return ret_val;
}

error_state_t write_file_to_file(FILE *in_file, FILE *out_file, off_t size, off_t *total_written) {

    error_state_t ret_val;
    size_t obtained, rw_size;
    off_t rw_total;
    char *buffer;

    if (in_file == NULL || out_file == NULL || total_written == NULL) {
        ret_val = ARG_ERROR;
        goto exit_early;
    }

    buffer = malloc(BUFF_SIFE);
    if (buffer == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_early;
    }

    rw_total = 0;
    rw_size = 0;

    while (rw_total < size) {
        rw_size = min(BUFF_SIFE, size - rw_total);
        obtained = fread(buffer, sizeof(*buffer), rw_size, in_file);

        if (obtained < 0) {
            ret_val = F_READ_ERROR;
            goto exit_normal;
        }

        if (obtained < rw_size) {
            rw_size = obtained;
            size = rw_total + rw_size;
        }

        obtained = fwrite(buffer, sizeof(*buffer), rw_size, out_file);
        if (obtained != rw_size) {
            ret_val = F_WRITE_ERROR;
            goto exit_normal;
        }
        rw_total += rw_size;
    }

    *total_written = rw_total;
    ret_val = EXIT_OK;

    exit_normal:
        free(buffer);
    exit_early:
        return ret_val;
}

error_state_t decompress_to_path(gzFile in_file, char *out_path, off_t size, off_t *total_written) {

    error_state_t ret_val;
    size_t obtained, rw_size;
    off_t rw_total;
    char *buffer;
    FILE *out_file;

    if (in_file == NULL || out_path == NULL || total_written == NULL) {
        ret_val = ARG_ERROR;
        goto exit_early;
    }

    buffer = malloc(BUFF_SIFE);
    if (buffer == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_early;
    }

    out_file = fopen(out_path, "w");
    if (out_file == NULL) {
        ret_val = F_OPEN_ERROR;
        goto exit_buffer;
    }

    rw_total = 0;
    rw_size = 0;

    while (rw_total < size) {
        rw_size = min(BUFF_SIFE, size - rw_total);

        obtained = gzread(in_file, buffer, rw_size);
        if (obtained < 0) {
            ret_val = FG_READ_ERROR;
            goto exit_normal;
        }

        if (obtained < rw_size) {
            rw_size = obtained;
            size = rw_total + rw_size;
        }

        obtained = fwrite(buffer, sizeof(*buffer), rw_size, out_file);
        if (obtained != rw_size) {
            ret_val = F_WRITE_ERROR;
            goto exit_normal;
        }
        rw_total += rw_size;
    }

    *total_written = rw_total;
    ret_val = EXIT_OK;

    exit_normal:
        fclose(out_file);
    exit_buffer:
        free(buffer);
    exit_early:
        return ret_val;
}

void free_linked_object(linked_object_t *link) {
    linked_object_t *next_link;
    while (link != NULL) {
        free(link->object);
        next_link = link->next_link;
        free(link);
        link = next_link;
    }
}

void free_list_items(void **list, int len, void (*f)(void *)) {
    for (int i = 0; i < len; i++) {
        f(list[i]);
        free(list[i]);
    }
}

int64_t min(int64_t a, int64_t b) {
    return (a < b)? a : b;
}

int64_t max(int64_t a, int64_t b) {
    return (a > b)? a : b;
}
