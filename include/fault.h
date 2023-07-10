#ifndef FAULT_H
#define FAULT_H

#include <stddef.h>
#include <sys/types.h>

typedef enum {

    EXIT_OK,
    UNKNOWN_ERROR,

    ARG_ERROR,
    ALLOC_ERROR,
    ENCODING_ERROR,

    F_OPEN_ERROR,
    F_READ_ERROR,
    F_WRITE_ERROR,
    F_SEEK_ERROR,
    F_SIZE_ERROR,

    FG_OPEN_ERROR,
    FG_READ_ERROR,
    FG_WRITE_ERROR,
    FG_SEEK_ERROR,

    MD5_START_ERROR,
    MD5_UPDT_ERROR,
    MD5_END_ERROR,

    CURL_INIT_ERROR,
    CURL_PERF_ERROR,

    RECORD_FIT_ERROR,
    RECORD_ECMA_ERROR,

    PATH_BUFFER_ERROR,
    FILE_LIST_BUFFER_ERROR,

    ERROR_COUNT,

} error_state_t;

extern const char *error_info[ERROR_COUNT];
void get_error_message(char **msg, error_state_t error_state);

#endif
