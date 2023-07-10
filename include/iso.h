#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#ifndef ISO_H
#define ISO_H

#include <stdint.h>
#include <sys/types.h>

#include <stdio.h>
#include <mbedtls/md5.h>

#include "fault.h"

#define MAX_FOLDERS 0x1000
#define BP(a,b) [(b) - (a) + 1]

enum file_state {EMPTY, MISSING, SZ_MISMATCH, MD5_MISMATCH, VERIFIED};
extern const char *state_info[5];

typedef struct {
    uint8_t vol_desc_type            BP(1, 1);
    uint8_t std_identifier           BP(2, 6);
    uint8_t vol_desc_version         BP(7, 7);
    uint8_t unused1                  BP(8, 8);
    uint8_t system_id                BP(9, 40);
    uint8_t volume_id                BP(41, 72);
    uint8_t unused2                  BP(73, 80);
    uint8_t vol_space_size           BP(81, 88);
    uint8_t unused3                  BP(89, 120);
    uint8_t vol_set_size             BP(121, 124);
    uint8_t vol_seq_number           BP(125, 128);
    uint8_t block_size               BP(129, 132);
    uint8_t path_table_size          BP(133, 140);
    uint8_t l_path_table_pos         BP(141, 144);
    uint8_t opt_l_path_table_pos     BP(145, 148);
    uint8_t m_path_table_pos         BP(149, 152);
    uint8_t opt_m_path_table_pos     BP(153, 156);
    uint8_t root_dir_record          BP(157, 190);
    uint8_t vol_set_id               BP(191, 318);
    uint8_t publisher_id             BP(319, 446);
    uint8_t data_prep_id             BP(447, 574);
    uint8_t application_id           BP(575, 702);
    uint8_t copyright_file_id        BP(703, 739);
    uint8_t abstract_file_id         BP(740, 776);
    uint8_t bibliographic_file_id    BP(777, 813);
    uint8_t vol_creation_time        BP(814, 830);
    uint8_t vol_modification_time    BP(831, 847);
    uint8_t vol_expiration_time      BP(848, 864);
    uint8_t vol_effective_time       BP(865, 881);
    uint8_t file_structure_version   BP(882, 882);
    uint8_t reserved1                BP(883, 883);
    uint8_t app_use                  BP(884, 1395);
    uint8_t reserved2                BP(1396, 2048);

} ecma119_pri_vol_desc_t;

typedef struct {
    uint8_t len_dr                   BP(1, 1);
    uint8_t len_xa                   BP(2, 2);
    uint8_t block                    BP(3, 10);
    uint8_t length                   BP(11, 18);
    uint8_t recording_time           BP(19, 25);
    uint8_t flags                    BP(26, 26);
    uint8_t file_unit_size           BP(27, 27);
    uint8_t interleave_gap_size      BP(28, 28);
    uint8_t vol_seq_number           BP(29, 32);
    uint8_t len_fi                   BP(33, 33);

} ecma119_dir_record_t;

typedef struct {
    uint8_t len_di                   BP(1, 1);
    uint8_t len_xa                   BP(2, 2);
    uint8_t block                    BP(3, 6);
    uint8_t parent                   BP(7, 8);

} ecma119_path_table_record_t;

typedef struct {
    uint32_t volume_size;
    uint16_t block_size;

    uint32_t path_table_size;
    uint32_t path_table_location;

} pri_vol_desc_t;

typedef struct path_table_record_s {
    struct path_table_record_s *parent;

    uint32_t block_offset;
    uint16_t parent_idx;
    uint8_t record_length;
    uint8_t len_di;
    
    char *dir_id;

} path_table_record_t;

typedef struct dir_record_s {
    struct dir_record_s *lead_extent;
    path_table_record_t *parent;

    off_t file_offset;
    uint32_t block_offset;

    uint32_t extent_length;
    uint8_t record_length;
    off_t total_length;

    uint8_t flags;
    uint8_t len_fi;

    char *file_id;
    uint8_t hash[0x10];

    mbedtls_md5_context *ctx;
    enum file_state state;

} dir_record_t;

typedef struct {
	pri_vol_desc_t *desc;

	FILE *header;
	FILE *footer;

} parse_info_t;

typedef struct {
    path_table_record_t **table;
    uint16_t length;

} dir_table_t;

typedef struct {
    dir_record_t **table;
    uint32_t length;

} file_table_t;

typedef struct linked_path_s {
    path_table_record_t *cur_dir;
    struct linked_path_s *next_dir;

} linked_path_t;

uint32_t ecma_int32(uint8_t *iso_num);
uint16_t ecma_int16(uint8_t *iso_num);

error_state_t build_path(char *buffer, int buffer_size, dir_record_t *record);
error_state_t init_traverse(parse_info_t *info, 
                  const char *header_path, const char *footer_path);

void sort_dir_list(dir_table_t *dir_list);
void sort_file_list(file_table_t *file_list);

error_state_t build_dir_list(dir_table_t *table_wrapper, parse_info_t *info);
error_state_t build_file_list(file_table_t *table_wrapper, parse_info_t *info,
                    dir_table_t *dir_wrapper, uint32_t max_file_count);

void free_path_record(path_table_record_t *rec);
void free_dir_record(dir_record_t *rec);

#endif
