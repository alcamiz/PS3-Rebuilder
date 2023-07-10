#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <zlib.h>

#include "ird.h"
#include "iso.h"
#include "util.h"
#include "cwalk.h"

static
error_state_t handle_ird_var(gzFile ird_file, int size,
                        uint32_t *length, char **buffer_wrap, bool large) {

    error_state_t ret_val;
    int read_size, obtained;
    char *buffer;
    uint8_t tmp_hold;

    if (length == NULL || buffer_wrap == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    if (large) {
        obtained = gzread(ird_file, length, sizeof(*length));
        if (obtained != sizeof(*length)) {
            ret_val = FG_READ_ERROR;
            goto exit_normal;
        }

    } else {
        obtained = gzread(ird_file, &tmp_hold, sizeof(tmp_hold));
        if (obtained != sizeof(tmp_hold)) {
            ret_val = FG_READ_ERROR;
            goto exit_normal;
        }
        *length = tmp_hold;
    }

    read_size = (*length)*size;
    buffer = malloc(read_size+1);
    if (buffer == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_normal;
    }
    buffer[read_size] = '\0';

    obtained = gzread(ird_file, buffer, read_size);
    if (obtained != read_size) {
        ret_val = FG_READ_ERROR;
        goto exit_early;
    }
    *buffer_wrap = buffer;

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_early:
        free(buffer);
    exit_normal:
        return ret_val;
}

static
error_state_t handle_ird_compressed(gzFile ird_file, const char *tmp_path,
                            const char *name, uint32_t *length,
                            char **path_wrap) {

    error_state_t ret_val;
    int read_size, obtained, path_len;
    off_t written;
    gzFile out_file;
    char *out_path;

    if (ird_file == NULL || tmp_path == NULL || name == NULL ||
            length == NULL || path_wrap == NULL) {
        
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    obtained = gzread(ird_file, length, sizeof(*length));
    if (obtained != sizeof(*length)) {
        ret_val = FG_READ_ERROR;
        goto exit_normal;
    }

    out_path = malloc(MAX_PATH_LEN);
    if (out_path == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_normal;
    }

    path_len = snprintf(out_path, MAX_PATH_LEN, "%s/%s.binu", tmp_path, name);

    if (path_len < 0) {
        ret_val = ENCODING_ERROR;
        goto exit_path;
    }

    if (path_len >= MAX_PATH_LEN) {
        ret_val = PATH_BUFFER_ERROR;
        goto exit_path;
    }

    ret_val = decompress_to_path(ird_file, out_path, *length, &written);
    if (ret_val != EXIT_OK) {
        goto exit_path;
    }

    if (written != *length) {
        ret_val = F_SIZE_ERROR;
        goto exit_path;
    }

    out_file = gzopen(out_path, "r");
    if (out_file == NULL) {
        ret_val = FG_OPEN_ERROR;
        goto exit_path;
    }

    out_path[path_len-1] = '\0';
    ret_val = decompress_to_path(out_file, out_path, INT64_MAX, &written);
    if (ret_val != EXIT_OK) {
        goto exit_file;
    }

    gzclose(out_file);
    *path_wrap = out_path;

    ret_val = EXIT_OK;    
    goto exit_normal;

    exit_file:
        gzclose(out_file);
    exit_path:
        free(out_path);
    exit_normal:
        return ret_val;
}

static
error_state_t bin_to_ird(ird_t *ird, ird_header_t *header, ird_body_t *body,
                  ird_footer_t *footer) {

    error_state_t ret_val;
    int offset;
    size_t read_size;

    if (ird == NULL || header == NULL || body == NULL || footer == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    read_size = sizeof(header->title_id);
    memcpy(ird->title_id, &header->title_id, read_size);
    ird->title_id[read_size] = '\0';

    read_size = sizeof(body->sys_ver);
    memcpy(ird->pup_version, &body->sys_ver, read_size);
    ird->title_id[read_size] = '\0';

    read_size = sizeof(body->disc_ver);
    memcpy(ird->disc_version, &body->disc_ver, read_size);
    ird->title_id[read_size] = '\0';

    read_size = sizeof(body->app_ver);
    memcpy(ird->app_version, &body->app_ver, read_size);
    ird->title_id[read_size] = '\0';

    offset = 0;

    if (header->version[0] == 9) {
        read_size = sizeof(ird->pic);
        memcpy(ird->pic, ((uint8_t *) &footer->disc_dt) + offset, read_size);
        offset += read_size;
    }

    read_size = sizeof(ird->data1);
    memcpy(ird->data1, ((uint8_t *) &footer->disc_dt) + offset, read_size);
    offset += read_size;

    read_size = sizeof(ird->data2);
    memcpy(ird->data2, ((uint8_t *) &footer->disc_dt) + offset, read_size);
    offset += read_size;

    if (header->version[0] < 9) {
        read_size = sizeof(ird->pic);
        memcpy(ird->pic, ((uint8_t *) &footer->disc_dt) + offset, read_size);
        offset += read_size;
    }

    ird->uid = ecma_int32((uint8_t *) &footer->uid);
    ird->crc = ecma_int32((uint8_t *) &footer->crc);

    ret_val = EXIT_OK;
    exit_normal:
        return ret_val;
}

error_state_t load_ird(ird_t *ird, const char *ird_path, const char *tmp_path) {

    error_state_t ret_val;
    int read_size, obtained;
    uint32_t title_len, header_len, footer_len, reg_count, file_count;

    ird_header_t ird_header;
    ird_body_t ird_body;
    ird_footer_t ird_footer;

    char *title;
    char *header_path;
    char *footer_path;

    gzFile ird_file;
    region_hash_t *region_hashes;
    file_hash_t *file_hashes;

    if (ird == NULL || ird_path == NULL || tmp_path == NULL) {
        ret_val = ARG_ERROR;
        goto exit_early;
    }

    ird_file = gzopen(ird_path, "r");
    if (ird_file == NULL) {
        ret_val = FG_OPEN_ERROR;
        goto exit_early;
    }

    read_size = sizeof(ird_header);
    obtained = gzread(ird_file, &ird_header, read_size);
    if (obtained != read_size) {
        ret_val = FG_READ_ERROR;
        goto exit_normal;
    }

    ret_val = handle_ird_var(ird_file, sizeof(*title), &title_len, &title, false);
    if (ret_val != EXIT_OK) {
        goto exit_title;
    }

    read_size = sizeof(ird_body);
    obtained = gzread(ird_file, &ird_body, read_size);
    if (obtained != read_size) {
        ret_val = FG_READ_ERROR;
        goto exit_title;
    }

    if (ird_header.version[0] == 7) {
        if (gzseek(ird_file, sizeof(uint32_t), SEEK_CUR) != 0) {
            ret_val = FG_SEEK_ERROR;
            goto exit_title;
        }
    }

    ret_val = handle_ird_compressed(ird_file, tmp_path, "header", &header_len, &header_path);
    if (ret_val != EXIT_OK) {
        goto exit_title;
    }

    ret_val = handle_ird_compressed(ird_file, tmp_path, "footer", &footer_len, &footer_path);
    if (ret_val != EXIT_OK) {
        goto exit_header;
    }

    ret_val = handle_ird_var(ird_file, sizeof(*region_hashes), &reg_count, &region_hashes, false);
    if (ret_val != EXIT_OK) {
        goto exit_footer;
    }

    ret_val = handle_ird_var(ird_file, sizeof(*file_hashes), &file_count, &file_hashes, true);
    if (ret_val != EXIT_OK) {
        goto exit_reg;
    }

    read_size = sizeof(ird_footer);
    obtained = gzread(ird_file, &ird_footer, read_size);
    if (obtained != read_size) {
        ret_val = FG_READ_ERROR;
        goto exit_hash;
    }

    ret_val = bin_to_ird(ird, &ird_header, &ird_body, &ird_footer);
    if (ret_val != EXIT_OK) {
        goto exit_hash;
    }

    ird->title = title;

    ird->region_count = reg_count;
    ird->file_count = file_count;

    ird->region_hashes = region_hashes;
    ird->file_hashes = file_hashes;

    ird->header_path = header_path;
    ird->footer_path = footer_path;

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_hash:
        free(file_hashes);
    exit_reg:
        free(region_hashes);
    exit_footer:
        free(footer_path);
    exit_header:
        free(header_path);
    exit_title:
        free(title);
    exit_normal:
        gzclose(ird_file);
    exit_early:
        return ret_val;
}

static
error_state_t attach_checksums(ird_t *ird, file_table_t *ft) {

    error_state_t ret_val;
    int hash_index;
    dir_record_t *cur_rec;
    file_hash_t *cur_hash;

    if (ird == NULL || ft == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    if (ird->file_hashes == NULL || ft->table == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    hash_index = 0;
    for (int index = 0; index < ft->length; index++) {

        cur_rec = ft->table[index];
        cur_hash = &(ird->file_hashes[hash_index]);
        if (cur_rec->lead_extent != NULL) continue;

        if (cur_rec->block_offset != cur_hash->sector) {
            ret_val = RECORD_ECMA_ERROR;
            goto exit_normal;
        }

        memcpy(&cur_rec->hash, &cur_hash->hash, 0x10);
        hash_index += 1;
    }

    ret_val = EXIT_OK;
    exit_normal:
        return ret_val;
}

static
error_state_t verify_files(parse_info_t *info, file_table_t *ft,
                  char *folder_path, bool *verified) {

    error_state_t ret_val;
    int stat_ret;
    bool all_ok;
    struct stat st;
    uint8_t checksum [0x10];

    dir_record_t *cur;
    char *rel_path, *full_path;

    if (info == NULL || ft == NULL || folder_path == NULL || verified == NULL) {
        ret_val = -1;
        goto exit_normal;
    }

    all_ok = true;

    for (int index = 0; index < ft->length; index++) {
        cur = ft->table[index];

        if (cur->lead_extent != NULL) continue;

        rel_path = malloc(MAX_PATH_LEN);
        if (rel_path == NULL) {
            ret_val = ALLOC_ERROR;
            goto exit_normal;
        }

        ret_val = build_path(rel_path, MAX_PATH_LEN, cur);
        if (ret_val != EXIT_OK) {
            goto exit_rel;
        }

        full_path = malloc(MAX_PATH_LEN);
        if (full_path == NULL) {
            ret_val = ALLOC_ERROR;
            goto exit_rel;
        }

        cwk_path_join(folder_path, rel_path, full_path, MAX_PATH_LEN);
        free(rel_path);

        stat_ret = stat(full_path, &st);
        
        if (stat_ret == -1 || !S_ISREG(st.st_mode)) {
            cur->state = MISSING;
            free(full_path);
            all_ok = false;
            continue;
        }

        if (st.st_size != cur->total_length) {
            cur->state = SZ_MISMATCH;
            free(full_path);
            all_ok = false;
            continue;
        }

        ret_val = calc_checksum(checksum, full_path);
        if (ret_val != EXIT_OK) {
            goto exit_full;
        }
        free(full_path);

        if (memcmp(checksum, &cur->hash, 0x10) != 0) {
            cur->state = MD5_MISMATCH;
            all_ok = false;
            continue;
        }
        cur->state = VERIFIED;
    }

    *verified = all_ok;
    ret_val = EXIT_OK;
    goto exit_normal;

    exit_rel:
        free(rel_path);
        goto exit_normal;
    exit_full:
        free(full_path);
        goto exit_normal;
    exit_normal:
        return ret_val;
}

error_state_t print_iso_list(ird_t *ird) {

    error_state_t ret_val;
    parse_info_t info;
    dir_table_t dt;
    file_table_t ft;

    path_table_record_t *cur_record;
    dir_record_t *cur_file;

    ret_val = init_traverse(&info, ird->header_path, ird->footer_path);
    if (ret_val != -1) {
        goto exit_normal;
    }

    ret_val = build_dir_list(&dt, &info);
    if (ret_val != -1) {
        goto exit_normal;
    }
    sort_dir_list(&dt);

    printf("Directories:\n");
    for (int index = 0; index < dt.length; index++) {
        path_table_record_t *cur_record = dt.table[index];
        printf("\t%u %s\n", cur_record->block_offset, cur_record->dir_id);
    }

    ret_val = build_file_list(&ft, &info, &dt, ird->file_count);
    if (ret_val != -1) {
        goto exit_normal;
    }
    sort_file_list(&ft);

    printf("Files:\n");
    for (int index = 0; index < ft.length; index++) {
        cur_file = ft.table[index];
        if (cur_file->lead_extent != NULL) continue;
        printf("\t%u %s\n", cur_file->block_offset, cur_file->file_id);
    }

    ret_val = EXIT_OK;
    exit_normal:
        return ret_val;
}

static
error_state_t print_validity_report(file_table_t *ft) {

    error_state_t ret_val;
    char *path;

    if (ft == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    printf("\n< Validity Report >\n");
    for (int index = 0; index < ft->length; index++) {
        dir_record_t *cur = ft->table[index];
        if (cur->lead_extent != NULL) continue;

        if (cur->state != VERIFIED) {
            path = malloc(MAX_PATH_LEN);
            if (path == NULL) {
                ret_val = ALLOC_ERROR;
                goto exit_normal;
            }

            ret_val = build_path(path, MAX_PATH_LEN, cur);
            if (ret_val != EXIT_OK) {
                goto exit_early;
            }

            printf("\t%s: %s\n", path, state_info[cur->state]);
            free(path);
        }
    }
    printf("\n");

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_early:
        free(path);
    exit_normal:
        return ret_val;
}

error_state_t print_verification(ird_t *ird, char *folder_path) {

    error_state_t ret_val;
    parse_info_t info;
    dir_table_t dt;
    file_table_t ft;
    bool all_ok;

    if (ird == NULL || folder_path == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    ret_val = init_traverse(&info, ird->header_path, ird->footer_path);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }

    ret_val = build_dir_list(&dt, &info);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }
    sort_dir_list(&dt);

    ret_val = build_file_list(&ft, &info, &dt, ird->file_count);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }
    sort_file_list(&ft);

    ret_val = attach_checksums(ird, &ft);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }

    ret_val = verify_files(&info, &ft, folder_path, &all_ok);
    if (ret_val != EXIT_OK) {
        printf("F5\n");
        goto exit_normal;
    }

    if (all_ok) {
        printf("\n< No issues to report >\n\n");
        ret_val = EXIT_OK;
        goto exit_normal;
    }

    ret_val = print_validity_report(&ft);
    if (ret_val != EXIT_OK) {
        printf("F6\n");
        goto exit_normal;
    }

    ret_val = EXIT_OK;
    exit_normal:
        return ret_val;
}

error_state_t rebuild_iso(ird_t *ird, char *folder_path, char *output_path) {

    error_state_t ret_val;
    uint16_t block_size;
    parse_info_t info;
    dir_table_t dt;
    file_table_t ft;
    off_t obtained;
    int str_ret;

    char *full_path, *rel_path;
    dir_record_t *cur_record;
    FILE *iso_file, *cur_file;

    if (ird == NULL || folder_path == NULL || output_path == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    ret_val = init_traverse(&info, ird->header_path, ird->footer_path);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }

    block_size = info.desc->block_size;

    ret_val = build_dir_list(&dt, &info);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }
    sort_dir_list(&dt);

    ret_val = build_file_list(&ft, &info, &dt, ird->file_count*2);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }
    sort_file_list(&ft);

    iso_file = fopen(output_path, "w");
    if (iso_file == NULL) {
        ret_val = F_OPEN_ERROR;
        goto exit_normal;
    };

    if (fseeko(info.header, 0L, SEEK_SET) != 0) {
        ret_val = F_SEEK_ERROR;
        goto exit_normal;
    };

    ret_val = write_file_to_file(info.header, iso_file, INT64_MAX, &obtained);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }

    for (int index = 0; index < ft.length; index++) {
        cur_record = ft.table[index];
        printf("%s\n", cur_record->file_id);

        ret_val = zero_out_file(iso_file, cur_record->block_offset*block_size - ftell(iso_file));
        if (ret_val != EXIT_OK) {
            goto exit_normal;
        }

        assert(cur_record->block_offset*block_size == ftell(iso_file));

        rel_path = malloc(MAX_PATH_LEN);
        if (rel_path == NULL) {
            ret_val = ALLOC_ERROR;
            goto exit_normal;
        }

        ret_val = build_path(rel_path, MAX_PATH_LEN, cur_record);
        if (ret_val != EXIT_OK) {
            goto exit_rel;
        }

        full_path = malloc(MAX_PATH_LEN);
        if (full_path == NULL) {
            ret_val = ALLOC_ERROR;
            goto exit_rel;
        }

        str_ret = snprintf(full_path, MAX_PATH_LEN, "%s/%s", folder_path, rel_path);

        if (str_ret < 0) {
            ret_val = ENCODING_ERROR;
            goto exit_full;
        }

        if (str_ret >= MAX_PATH_LEN) {
            ret_val = PATH_BUFFER_ERROR;
            goto exit_full;
        }

        cur_file = fopen(full_path, "r");
        if (cur_file == NULL) {
            ret_val = F_OPEN_ERROR;
            goto exit_full;
        }

        if (fseeko(cur_file, cur_record->file_offset, SEEK_SET) != 0) {
            ret_val = F_SEEK_ERROR;
            goto exit_file;
        }

        ret_val = write_file_to_file(cur_file, iso_file, cur_record->extent_length, &obtained);
        if (ret_val != EXIT_OK) {
            goto exit_file;
        }
        
        if (obtained != cur_record->extent_length) {
            ret_val = F_SIZE_ERROR;
            goto exit_file;
        }

        fclose(cur_file);
        free(full_path);
        free(rel_path);
    }

    if (fseeko(info.footer, 0L, SEEK_SET) != 0) {
        ret_val = F_SEEK_ERROR;
        goto exit_normal;
    }

    ret_val = write_file_to_file(info.footer, iso_file, INT64_MAX, &obtained);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_file:
        fclose(cur_file);
    exit_rel:
        free(rel_path);
    exit_full:
        free(full_path);
    exit_normal:
        return ret_val;
}
