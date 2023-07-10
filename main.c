#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <assert.h>
#include <argp.h>
#include <string.h>

#include "cwalk.h"
#include "util.h"
#include "iso.h"
#include "ird.h"
#include "sfo.h"
#include "net.h"
#include "fault.h"

struct values {
    char *ird_path;
    char *file_name;

    char *in_dir;
    char *out_dir;

    bool get_pup;
};

static int parse_opt (int key, char *arg, struct argp_state *state) {
    struct values *vals = (struct values *) state->input;
    struct stat sb;
    switch (key) {
        case 'f':
            if (vals->file_name != NULL)
                argp_failure(state, 1, 0, "Only one filename can be supplied");
            vals->file_name = malloc(0x420);
            strncpy(vals->file_name, arg, 0x420);
            break;
        case 'o':
            if (vals->out_dir != NULL)
                argp_failure(state, 1, 0, "Only one output folder can be supplied");
            vals->out_dir = malloc(0x420);
            cwk_path_normalize(arg, vals->out_dir, 0x420);
            if (stat(vals->out_dir, &sb) != 0)
                argp_failure(state, 1, 0, "Can't open supplied output folder");
            if (!S_ISDIR(sb.st_mode))
                argp_failure(state, 1, 0, "Output path is not a folder");
            break;
        case 'r':
            if (vals->ird_path != NULL)
                argp_failure(state, 1, 0, "Only one ird file can be used");
            vals->ird_path = malloc(0x420);
            cwk_path_normalize(arg, vals->ird_path, 0x420);
            if (stat(vals->ird_path, &sb) != 0)
                argp_failure(state, 1, 0, "Can't open ird file");
            if (!S_ISDIR(sb.st_mode))
                argp_failure(state, 1, 0, "IRD file is not a folder");
            break;
        case 'p':
            vals->get_pup = true;
            break;

        case ARGP_KEY_ARG:            
            if (vals->in_dir != NULL)
                argp_failure(state, 1, 0, "Too many arguments");
            vals->in_dir = malloc(0x420);
            cwk_path_normalize(arg, vals->in_dir, 0x420);
            if (stat(vals->in_dir, &sb) != 0)
                argp_failure(state, 1, 0, "Can't open supplied input folder");
            if (!S_ISDIR(sb.st_mode))
                argp_failure(state, 1, 0, "Input path is not a folder");
            break;
        case ARGP_KEY_END:
            if (vals->in_dir == NULL)
                argp_failure(state, 1, 0, "No JB folder was supplied to rebuild");
            break;
    }
    return 0;
}

int main (int argc, char** argv) {
    error_state_t ret_val;
    char *sfo_path, *pup_path, *tmp_path, *iso_path, *err_msg;

    struct stat st = {0};
    sfo_t sfo;
    ird_t ird;

    struct argp_option options[] = {
        { "filename", 'f', "NAME", 0, "Set filename for ISO"},
        { "output", 'o', "OUT_PATH", 0, "Set output folder"},
        { "ird", 'r', "IRD_PATH", 0, "Manually supply IRD file"},
        { "pup", 'p', 0, 0, "Download/replace PUP file from online archive"},
        {0}
    };
    struct values vals = {NULL, NULL};
    struct argp argp = { options, parse_opt, "JB_FOLDER", "Rebuild JB Folder dumps into proper ISOs" };

    argp_parse(&argp, argc, argv, 0, 0, &vals);
    cwk_path_normalize(vals.in_dir, vals.in_dir, MAX_PATH_LEN);

    sfo_path = malloc(MAX_PATH_LEN);
    if (sfo_path == NULL) {
        ret_val = ALLOC_ERROR;
        goto exec_error;
    }
    snprintf(sfo_path, MAX_PATH_LEN, "%s/%s", vals.in_dir, SFO_REL_PATH);

    ret_val = load_sfo(&sfo, sfo_path);
    if (ret_val != EXIT_OK) {
        goto exec_error;
    }

    tmp_path = malloc(MAX_PATH_LEN);
    if (tmp_path == NULL) {
        ret_val = ALLOC_ERROR;
        goto exec_error;
    }
    snprintf(tmp_path, MAX_PATH_LEN, "%s/%08X", TMP_DIR, sfo.mgz_sig);

    if (stat(TMP_DIR, &st) == -1) {
        mkdir(TMP_DIR, 0700);
    }

    if (stat(tmp_path, &st) == -1) {
        mkdir(tmp_path, 0700);
    }

    if (vals.out_dir == NULL) {
        vals.out_dir = malloc(MAX_PATH_LEN);
        getcwd(vals.out_dir, MAX_PATH_LEN);
        cwk_path_normalize(vals.out_dir, vals.out_dir, MAX_PATH_LEN);
    }

    if (vals.file_name == NULL) {

        vals.file_name = malloc(15);
        memcpy(vals.file_name, sfo.title_id, 4);

        vals.file_name[4] = '-';
        memcpy(vals.file_name+5, sfo.title_id+4, 5);
        memcpy(vals.file_name+10, ".iso", 4);
        vals.file_name[14] = '\0';
    }

    if (vals.ird_path == NULL) {
        vals.ird_path = malloc(MAX_PATH_LEN);
        if (vals.ird_path == NULL) {
            ret_val = ALLOC_ERROR;
            goto exec_error;
        }

        snprintf(vals.ird_path, MAX_PATH_LEN, "%s/%s", tmp_path, "ird.bin");
        ret_val = download_ird(&sfo, vals.ird_path);
        if (ret_val != EXIT_OK) {
            goto exec_error;
        }
    }

    if (vals.get_pup) {
        pup_path = malloc(MAX_PATH_LEN);
        if (pup_path == NULL) {
            ret_val = ALLOC_ERROR;
            goto exec_error;
        }
        snprintf(pup_path, MAX_PATH_LEN, "%s/%s", vals.in_dir, PUP_DIR);

        if (stat(pup_path, &st) == -1) {
            mkdir(pup_path, 0700);
        }

        snprintf(pup_path, MAX_PATH_LEN, "%s/%s", vals.in_dir, PUP_REL_PATH);
        ret_val = download_pup(&sfo, pup_path);
        if (ret_val != EXIT_OK) {
            goto exec_error;
        }
    }
    printf("Here\n");

    ret_val = load_ird(&ird, vals.ird_path, tmp_path);
    if (ret_val != EXIT_OK) {
        goto exec_error;
    }

    ret_val = print_verification(&ird, vals.in_dir);
    if (ret_val != EXIT_OK) {
        goto exec_error;
    }

    iso_path = malloc(MAX_PATH_LEN);
    if (iso_path == NULL) {
        ret_val = ALLOC_ERROR;
        goto exec_error;
    }
    snprintf(iso_path, MAX_PATH_LEN, "%s/%s", vals.out_dir, vals.file_name);

    ret_val = rebuild_iso(&ird, vals.in_dir, iso_path);
    if (ret_val != EXIT_OK) {
        goto exec_error;
    }

    return EXIT_SUCCESS;

    exec_error:
        printf("%d\n", ret_val);
        get_error_message(&err_msg, ret_val);
        printf("< ERROR > %s\n", err_msg);
        return EXIT_FAILURE;
}
