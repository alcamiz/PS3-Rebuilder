#ifndef IRD_H
#define IRD_H

#include <stdint.h>
#include <sys/types.h>

#include <stdint.h>
#include <stdio.h>

#include "iso.h"
#include "util.h"
#include "fault.h"

#define MAGIC "3IRD"

typedef struct {
    uint8_t hash[0x10];

} region_hash_t;

typedef struct {
    uint64_t sector;
    uint8_t hash[0x10];

} file_hash_t;

typedef struct {
	uint8_t magic 		[4];
    uint8_t version 	[1];
    uint8_t title_id 	[9];

} ird_header_t;

typedef struct {
	uint8_t sys_ver 	[4];
	uint8_t disc_ver 	[5];
	uint8_t app_ver 	[5];

} ird_body_t;

typedef struct {
	uint8_t extra_dt	[4];
	uint8_t disc_dt 	[147];
	uint8_t uid			[4];
	uint8_t crc			[4];

} ird_footer_t;

typedef struct {

	uint32_t title_length;
	char *title;

	char title_id[10];
	char pup_version[5];
	char disc_version[6];
	char app_version[6];

	uint32_t region_count;
	uint32_t file_count;

	region_hash_t *region_hashes;
	file_hash_t *file_hashes;

	uint8_t pic[0x73];
	uint8_t data1[0x10];
	uint8_t data2[0x10];

	uint32_t uid;
	uint32_t crc;

	char *header_path;
	char *footer_path;

} ird_t;

error_state_t load_ird(ird_t *ird, const char *ird_path, const char *tmp_path);
error_state_t print_iso_list(ird_t *ird);
error_state_t print_verification(ird_t *ird, char *folder_path);
error_state_t rebuild_iso(ird_t *ird, char *folder_path, char *output_path);

#endif
