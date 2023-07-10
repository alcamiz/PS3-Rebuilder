#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "util.h"
#include "iso.h"

const char *state_info[5] = { "", "Missing", "Size Mismatch", "Checksum Mismatch", "Verified"};

uint32_t ecma_int32(uint8_t *iso_num) {
    return (uint32_t) ((iso_num[0] & 0xff)
                    | ((iso_num[1] & 0xff) << 8)
                    | ((iso_num[2] & 0xff) << 16)
                    | ((iso_num[3] & 0xff) << 24));
}

uint16_t ecma_int16(uint8_t *iso_num) {
    return (uint16_t) ((iso_num[0] & 0xff) 
                    | ((iso_num[1] & 0xff) << 8));
}

static
bool ecma_is_dir(dir_record_t *record) {
    return (record->flags & 0x2);
}

static
bool ecma_has_extent(dir_record_t *record) {
    return (record->flags & 0x80);
}

static
error_state_t ecma_to_desc(pri_vol_desc_t *record, ecma119_pri_vol_desc_t *ecma_record) {

    if (record == NULL || ecma_record == NULL) {
        return ARG_ERROR;
    }

    record->volume_size = ecma_int32(&ecma_record->vol_space_size[0]);
    record->block_size = ecma_int16(&ecma_record->block_size[0]);

    record->path_table_size = ecma_int32(&ecma_record->path_table_size[0]);
    record->path_table_location = ecma_int32(&ecma_record->l_path_table_pos[0]);

    return EXIT_OK;
}

static
error_state_t ecma_to_path(path_table_record_t *record, ecma119_path_table_record_t *ecma_record) {

    if (record == NULL || ecma_record == NULL) {
        return ARG_ERROR;
    }

    record->parent = NULL;
    record->block_offset = ecma_int32(&ecma_record->block[0]);
    record->parent_idx = ecma_int16(&ecma_record->parent[0]);

    record->record_length = sizeof(ecma119_path_table_record_t) + ecma_record->len_di[0];
    if (record->record_length % 2) record->record_length += 1;

    record->len_di = ecma_record->len_di[0];
    record->dir_id = NULL;

    return EXIT_OK;
}

static
error_state_t ecma_to_dir(dir_record_t *record, ecma119_dir_record_t *ecma_record, uint16_t block_size) {

    if (record == NULL || ecma_record == NULL) {
        return ARG_ERROR;
    }

    record->parent = NULL;
    record->lead_extent = NULL;

    record->file_offset = 0;
    record->block_offset = ecma_int32(&ecma_record->block[0]);
    record->extent_length = ecma_int32(&ecma_record->length[0]);
    record->total_length = 0;

    record->record_length = ecma_record->len_dr[0];

    record->flags = ecma_record->flags[0];
    record->len_fi = ecma_record->len_fi[0];

    record->file_id = NULL;
    record->ctx = NULL;
    record->state = EMPTY;

    return EXIT_OK;
}

static
error_state_t retrieve_vol_desc(pri_vol_desc_t *descriptor,
                                FILE *header, off_t position) {

    error_state_t ret_val;
    size_t obtained;
    ecma119_pri_vol_desc_t ecma_descriptor;

    if (descriptor == NULL || header == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    if (fseeko(header, position, SEEK_SET) != 0) {
        ret_val = F_SEEK_ERROR;
        goto exit_normal;
    }

    obtained = fread(&ecma_descriptor, sizeof(ecma_descriptor), 1, header);
    if (obtained != 1) {
        ret_val = F_READ_ERROR;
        goto exit_normal;
    }

    ret_val = ecma_to_desc(descriptor, &ecma_descriptor);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }

    ret_val = EXIT_OK;
    exit_normal:
        return ret_val;
}

static
int retrieve_path_record(path_table_record_t *record,
                        parse_info_t *info, off_t position) {

    int ret_val;
    size_t obtained;
    uint8_t *utf8_name;
    uint16_t *utf16_name;
    ecma119_path_table_record_t ecma_record;

    if (record == NULL || info == NULL) {
        ret_val = ARG_ERROR;
        goto exit_early;
    }

    if (fseeko(info->header, position, SEEK_SET) != 0) {
        ret_val = F_SEEK_ERROR;
        goto exit_early;
    }

    obtained = fread(&ecma_record, sizeof(ecma_record), 1, info->header);
    if (obtained != 1) {
        ret_val = F_READ_ERROR;
        goto exit_early;
    }

    ret_val = ecma_to_path(record, &ecma_record);
    if (ret_val != EXIT_OK) {
        goto exit_early;
    }

    utf16_name = malloc(record->len_di + 2);
    if (utf16_name == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_early;
    }
    utf16_name[record->len_di / 2] = 0;

    utf8_name = malloc(record->len_di*2 + 1);
    if (utf8_name == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_normal;
    }

    if (fseeko(info->header, position + sizeof(ecma_record), SEEK_SET) != 0) {
        ret_val = F_SEEK_ERROR;
        goto exit_name;
    }

    obtained = fread(utf16_name, sizeof(uint8_t), record->len_di, info->header);
    if (obtained != record->len_di) {
        ret_val = F_READ_ERROR;
        goto exit_name;
    }

    ret_val = utf16_to_utf8(utf16_name, utf8_name);
    if (ret_val != EXIT_OK) {
        goto exit_name;
    }

    record->dir_id = (char *) utf8_name;
    record->len_di = strlen(utf8_name);

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_name:
        free(utf8_name);
    exit_normal:
        free(utf16_name);
    exit_early:
        return ret_val;
}

static
error_state_t retrieve_dir_record(dir_record_t *record,
                        parse_info_t *info, off_t position) {

    error_state_t ret_val;
    size_t obtained;
    uint16_t block_size;
    off_t start_block, end_block, remainder;
    uint8_t *utf8_name;
    uint16_t *utf16_name;
    ecma119_dir_record_t ecma_record;

    if (record == NULL || info == NULL) {
        ret_val = ARG_ERROR;
        goto exit_early;
    }

    block_size = info->desc->block_size;
    start_block = position / block_size;
    end_block = (position + sizeof(ecma_record) - 1) / block_size;

    if (start_block != end_block) {
        ret_val = RECORD_FIT_ERROR;
        goto exit_early;
    }

    if (fseeko(info->header, position, SEEK_SET) != 0) {
        ret_val = F_SEEK_ERROR;
        goto exit_early;
    }

    obtained = fread(&ecma_record, sizeof(ecma_record), 1, info->header);
    if (obtained != 1) {
        ret_val = F_READ_ERROR;
        goto exit_early;
    }
    if (ecma_record.len_dr[0] == 0) {
        ret_val = RECORD_FIT_ERROR;
        goto exit_early;
    }

    ret_val = ecma_to_dir(record, &ecma_record, block_size);
    if (ret_val != EXIT_OK) {
        goto exit_early;
    }

    utf16_name = malloc(record->len_fi + 2);
    if (utf16_name == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_early;
    }
    utf16_name[record->len_fi / 2] = 0;

    utf8_name = malloc(record->len_fi*2 + 1);
    if (utf8_name == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_normal;
    }

    if (fseeko(info->header, position + sizeof(ecma_record), SEEK_SET) != 0) {
        ret_val = F_SEEK_ERROR;
        goto exit_name;
    }

    obtained = fread(utf16_name, sizeof(uint8_t), record->len_fi, info->header);
    if (obtained != record->len_fi) {
        ret_val = F_READ_ERROR;
        goto exit_name;
    }

    ret_val = utf16_to_utf8(utf16_name, utf8_name);
    if (ret_val != EXIT_OK) {
        goto exit_name;
    }

    record->file_id = (char *) utf8_name;
    record->len_fi = strlen(utf8_name);

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_name:
        free(utf8_name);
    exit_normal:
        free(utf16_name);
    exit_early:
        return ret_val;
}

error_state_t init_traverse(parse_info_t *info,
                  const char *header_path, const char *footer_path) {

    error_state_t ret_val;

    if (info == NULL || header_path == NULL || footer_path == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    info->header = fopen(header_path, "r");
    if (info->header == NULL) {
        ret_val = F_OPEN_ERROR;
        goto exit_normal;
    }

    info->footer = fopen(footer_path, "r");
    if (info->footer == NULL) {
        ret_val = F_OPEN_ERROR;
        goto exit_header;
    }

    info->desc = malloc(sizeof(*(info->desc)));
    if (info->desc == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_footer;
    }

    ret_val = retrieve_vol_desc(info->desc, info->header, 0x8800);
    if (ret_val != EXIT_OK) {
        goto exit_footer;
    }

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_footer:
        fclose(info->footer);
    exit_header:
        fclose(info->header);
    exit_normal:
        return ret_val;
}

error_state_t build_path(char *buffer, int buffer_size, dir_record_t *record) {

    error_state_t ret_val;
    int str_pos, path_len;
    path_table_record_t *cur_dir;
    linked_object_t *path_list, *cur_link;

    if (buffer == NULL || record == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    if (record->parent == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    path_list = malloc(sizeof(*path_list));
    if (path_list == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_normal;
    }

    cur_dir = record->parent;
    path_list->object = cur_dir;
    path_list->next_link = NULL;
    path_len = record->len_fi + 1;

    // Build directory linked list
    while (path_list->object != cur_dir->parent) {

        path_len += cur_dir->len_di + 1;
        if (path_len > buffer_size) {
            ret_val = PATH_BUFFER_ERROR;
            goto exit_linked;
        }

        cur_dir = cur_dir->parent;
        if (cur_dir == NULL) {
            ret_val = RECORD_ECMA_ERROR;
            goto exit_linked;
        }

        cur_link = malloc(sizeof(*cur_link));
        if (cur_link == NULL) {
            ret_val = ALLOC_ERROR;
            goto exit_linked;
        }

        cur_link->object = cur_dir;
        cur_link->next_link = path_list;
        path_list = cur_link;

    }

    // Build full directory path in buffer
    str_pos = 0;
    while (path_list != NULL) {
        cur_link = path_list;
        cur_dir = (path_table_record_t *) cur_link->object;
        memcpy(buffer + str_pos, cur_dir->dir_id, cur_dir->len_di);
        str_pos += cur_dir->len_di + 1;
        buffer[str_pos-1] = '/';
        path_list = cur_link->next_link;
        free(cur_link);
    }

    memcpy(buffer + str_pos, record->file_id, record->len_fi);
    str_pos += record->len_fi;
    buffer[str_pos] = '\0';

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_linked:
        free_linked_object(path_list);
    exit_normal:
        return ret_val;
}

static
int compare_path_records(const void *a, const void *b) {
    path_table_record_t *rec_a = *((path_table_record_t **) a);
    path_table_record_t *rec_b = *((path_table_record_t **) b);

    return rec_a->block_offset - rec_b->block_offset;
}

static
int compare_dir_records(const void *a, const void *b) {
    dir_record_t *rec_a = *((dir_record_t **) a);
    dir_record_t *rec_b = *((dir_record_t **) b);

    return rec_a->block_offset - rec_b->block_offset;
}

void sort_dir_list(dir_table_t *dir_list) {
    qsort((void *) dir_list->table, dir_list->length, 
            sizeof(path_table_record_t *), compare_path_records);
}

void sort_file_list(file_table_t *file_list) {
    qsort((void *) file_list->table, file_list->length, 
            sizeof(dir_record_t *), compare_dir_records);
}

static
error_state_t handle_extent_record(parse_info_t *info, path_table_record_t *parent,
                          uint32_t *num_extents, off_t *header_position,
                          dir_record_t *lead_extent, dir_record_t **file_list,
                          int max_extent_count) {

    error_state_t ret_val;
    off_t relative_offset, cur_offset;
    uint16_t block_size;
    uint32_t list_index;
    dir_record_t *cur_record;

    if (info == NULL || parent == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }
    
    if (num_extents == NULL || header_position == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    if (lead_extent == NULL || file_list == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    list_index = 0;
    block_size = info->desc->block_size;
    relative_offset = lead_extent->extent_length;
    cur_offset = *header_position + lead_extent->extent_length;

    do {
        if (list_index >= max_extent_count) {
            ret_val = FILE_LIST_BUFFER_ERROR;
            goto exit_early;
        }
        cur_record = malloc(sizeof(*cur_record));
        if (cur_record == NULL) {
            ret_val = ALLOC_ERROR;
            goto exit_early;
        }

        ret_val = retrieve_dir_record(cur_record, info, cur_offset);
        if (ret_val == RECORD_FIT_ERROR) {
            cur_offset += block_size - (cur_offset % block_size);
            continue;
        } else if (ret_val != EXIT_OK) {
            goto exit_early;
        }

        lead_extent->total_length += cur_record->extent_length;
        file_list[list_index] = cur_record;
        cur_record->parent = parent;
        cur_record->lead_extent = lead_extent;
        cur_record->file_offset = relative_offset;

        cur_record->len_fi -= 2;
        cur_record->file_id[cur_record->len_fi] = '\0';

        relative_offset += cur_record->extent_length;
        cur_offset += cur_record->record_length;
        list_index += 1;

    } while (ecma_has_extent(cur_record));

    *num_extents = list_index;
    *header_position += cur_offset;

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_early:
        free_list_items(file_list, list_index, free_dir_record);
    exit_normal:
        return ret_val;
}

static
error_state_t build_single_dir(parse_info_t *info, path_table_record_t *path_rec,
                     dir_record_t **file_list, uint32_t *file_count,
                     uint32_t max_file_count) {

    error_state_t ret_val;
    off_t current_offset, target_offset;
    uint16_t block_size;
    uint32_t list_index, num_extents;
    dir_record_t *dir_record, *cur_record;

    if (info == NULL || path_rec == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    if (file_list == NULL || file_count == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    block_size = info->desc->block_size;
    current_offset = path_rec->block_offset * block_size;

    dir_record = malloc(sizeof(*dir_record));
    if (dir_record == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_normal;
    }

    ret_val = retrieve_dir_record(dir_record, info, current_offset);
    if (ret_val != EXIT_OK) {
        goto exit_normal;
    }

    current_offset = dir_record->block_offset * block_size;
    target_offset = current_offset + dir_record->extent_length;
    list_index = 0;

    while (current_offset < target_offset) {

        cur_record = malloc(sizeof(*cur_record));
        if (cur_record == NULL) {
            ret_val = ALLOC_ERROR;
            goto exit_early;
        }

        ret_val = retrieve_dir_record(cur_record, info, current_offset);
        if (ret_val == RECORD_FIT_ERROR) {
            current_offset += block_size - (current_offset % block_size);
            continue;
        } else if (ret_val != EXIT_OK) {
            goto exit_early;
        }

        if (ecma_is_dir(cur_record)) {
            current_offset += cur_record->record_length;
            free_dir_record(cur_record);
            continue;
        }

        if (list_index >= max_file_count) {
            ret_val = FILE_LIST_BUFFER_ERROR;
            goto exit_early;
        }

        file_list[list_index] = cur_record;
        list_index += 1;

        cur_record->parent = path_rec;
        cur_record->len_fi -= 2;
        cur_record->file_id[cur_record->len_fi] = '\0';
        cur_record->total_length = cur_record->extent_length;

        if (ecma_has_extent(cur_record)) {
            ret_val = handle_extent_record(info, path_rec, &num_extents,
                            &current_offset, cur_record, file_list+list_index,
                            max_file_count-list_index);
            if (ret_val != EXIT_OK) {
                goto exit_early;
            }

        } else {
            current_offset += cur_record->record_length;
        }

        // char *tmp = malloc(MAX_PATH_LEN);
        // build_path(tmp, MAX_PATH_LEN, cur_record);
        // printf("Path: %s\n", tmp);
    }
    *file_count = list_index;

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_early:
        free_list_items(file_list, list_index, free_dir_record);
    exit_normal:
        return ret_val;
}

error_state_t build_dir_list(dir_table_t *table_wrapper, parse_info_t *info) {

    error_state_t ret_val;
    off_t cur_offset, target_offset;
    uint32_t table_index;
    path_table_record_t *table_entry = NULL;
    path_table_record_t **table = NULL;

    if (table_wrapper == NULL || info == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    cur_offset = info->desc->path_table_location * info->desc->block_size;
    target_offset = cur_offset + info->desc->path_table_size;
    table_index = 0;

    table = malloc(MAX_FOLDERS * sizeof(*table));
    if (table == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_normal;
    }

    while (cur_offset < target_offset) {
        table_entry = malloc(sizeof(*table_entry));
        if (table_entry == NULL) {
            ret_val = ALLOC_ERROR;
            goto exit_early;
        }

        ret_val = retrieve_path_record(table_entry, info, cur_offset);
        if (ret_val != EXIT_OK) {
            goto exit_entry;
        }

        table[table_index] = table_entry;
        table_entry->parent = table[table_entry->parent_idx - 1];
        cur_offset += table_entry->record_length;

        table_index += 1;
    }

    table = realloc(table, table_index * sizeof(*table));
    if (table == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_early;
    }

    table_wrapper->table = table;
    table_wrapper->length = table_index;

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_entry:
        free(table_entry);
    exit_early:
        free_list_items(table, table_index, free_path_record);
        free(table);
    exit_normal:
        return ret_val;
}

error_state_t build_file_list(file_table_t *table_wrapper, parse_info_t *info,
                    dir_table_t *dir_wrapper, uint32_t max_file_count) {

    int ret_val;
    uint32_t table_position, file_count;
    dir_record_t *table_entry = NULL;
    dir_record_t **table = NULL;

    if (table_wrapper == NULL || info == NULL || dir_wrapper == NULL) {
        ret_val = ARG_ERROR;
        goto exit_normal;
    }

    table = malloc(sizeof(*table) * max_file_count);
    if (table == NULL) {
        ret_val = ALLOC_ERROR;
        goto exit_normal;
    }

    table_position = 0;
    for (int index = 0; index < dir_wrapper->length; index++) {
        ret_val = build_single_dir(info, dir_wrapper->table[index],
                            table + table_position, &file_count,
                                max_file_count - table_position);
        if (ret_val != EXIT_OK) {
            goto exit_early;
        }

        table_position += file_count;
        if (table_position >= max_file_count) break;
    }

    table_wrapper->table = table;
    table_wrapper->length = table_position;

    ret_val = EXIT_OK;
    goto exit_normal;

    exit_early:
        free_list_items(table, table_position, free_dir_record);
        free(table);
    exit_normal:
        return ret_val;
}

void free_path_record(path_table_record_t *rec) {
    if (rec->dir_id != NULL) free(rec->dir_id);
}

void free_dir_record(dir_record_t *rec) {
    if (rec->file_id != NULL) free(rec->file_id);
}
