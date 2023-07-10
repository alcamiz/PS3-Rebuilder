#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#ifndef SFO_H
#define SFO_H

#include <stdint.h>
#include <sys/types.h>

#include "fault.h"

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t key_table_start;
    uint32_t data_table_start;
    uint32_t tables_entries;

} sfo_header_t;

typedef struct {
    uint16_t key_offset;
    uint16_t data_fmt;
    uint32_t data_len;
    uint32_t data_max_len;
    uint32_t data_offset;

} sfo_index_table_entry_t;

typedef struct {
    char title_id[10];
    char sys_ver[5];
    char disc_ver[6];
    char app_ver[6];

    uint32_t mgz_sig;

} sfo_t;

error_state_t load_sfo(sfo_t *sfo, char *sfo_path);
error_state_t print_sfo(sfo_t *sfo);

#endif
