#include <stddef.h>
#include <sys/types.h>
#include <stdio.h>

#include "fault.h"

const char *error_info[ERROR_COUNT] = {

    "",
    "Unkwown internal error",

    "Incorrect argument used",
    "Allocation error",
    "Encoding error",

    "File open error",
    "File read error",
    "File write error",
    "File seek error",
    "File size error",

    "Gzip file open error",
    "Gzip file read error",
    "Gzip file write error",
    "Gzip file seek error",

    "MD5 initialization error",
    "MD5 updated error",
    "MD5 finalization error",

    "CURL initialization error",
    "CURL request error",

    "Directory record alignment issue",
    "Record violates ECMA standard",

    "Path buffer error",
    "File list buffer error",

};

void get_error_message(char **msg, error_state_t error_state) {

    if (error_state < 0 || error_state > ERROR_COUNT) {
        error_state = UNKNOWN_ERROR;
    }

    for (int i = 0; i < ERROR_COUNT; i++) {
        printf("%d: %s\n", i, error_info[i]);
    }

    *msg = (char *) error_info[error_state];
}
