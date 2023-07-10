#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <stdio.h>
#include <zlib.h>

#include "fault.h"

#define SWAP16(x) (((x) >> 8) | ((x) << 8))

#define TMP_DIR "/tmp/ird_rebuild"
#define PUP_DIR "PS3_UPDATE"

#define SFO_REL_PATH "PS3_GAME/PARAM.SFO"
#define PUP_REL_PATH "PS3_UPDATE/PS3UPDAT.PUP"

#define MAX_PATH_LEN 4096
#define BUFF_SIFE 4096

typedef struct {
    char *memory;
    size_t size;

} memory_wrapper_t;

typedef struct linked_object_s {
    void *object;
    struct linked_object_s *next_link;

} linked_object_t;

error_state_t utf16_to_utf8(uint16_t *stw, uint8_t *stb);
error_state_t calc_checksum(uint8_t *checksum, char *file_path);

error_state_t zero_out_file(FILE *in_file, off_t size);
error_state_t write_file_to_file(FILE *in_file, FILE *out_file, off_t size, off_t *total_written);
error_state_t decompress_to_path(gzFile in_file, char *out_path, off_t size, off_t *total_written);

int64_t min(int64_t a, int64_t b);
int64_t max(int64_t a, int64_t b);

void free_linked_object(linked_object_t *link);
void free_list_items(void **list, int len, void (*f)(void *));

#endif
