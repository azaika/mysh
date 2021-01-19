#ifndef MYSH_SHELL_RESOURCE_H
#define MYSH_SHELL_RESOURCE_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>

#include "mystring.h"

typedef struct mysh_resource_tag {
    mysh_string current_dir;
    mysh_string home_dir;
} mysh_resource;

static mysh_resource mysh_shell_resource;

static void mysh_set_curdir_name(mysh_resource* res, const char* name) {
    if (name == NULL) {
        return;
    }

    size_t name_len = strlen(name);
    if (name_len < res->home_dir.length) {
        ms_assign_raw(&res->current_dir, name);
        return;
    }

    int flag = 1;
    size_t i;
    for (i = 0; i < res->home_dir.length; ++i) {
        if (name[i] != res->home_dir.ptr[i]) {
            flag = 0;
            break;
        }
    }

    if (flag) {
        ms_reserve(&res->current_dir, name_len - i + 1);
        res->current_dir.length = name_len - i + 1;

        res->current_dir.ptr[0] = '~';
        for (size_t j = 0; j < name_len - i; ++j) {
            res->current_dir.ptr[1 + j] = name[i + j];
        }

        res->current_dir.ptr[name_len - i + 1] = '\0';
    }
    else {
        ms_assign_raw(&res->current_dir, name);
    }
}

// returns false on error
static bool mysh_init_resource(mysh_resource* res) {
    ms_init(&res->current_dir, "");
    ms_init(&res->home_dir, getenv("HOME"));

    if (errno != 0) {
        perror("mysh");
        return false;
    }

    mysh_set_curdir_name(res, get_current_dir_name());

    if (errno != 0) {
        perror("mysh");
        return false;
    }

    return true;
}

static void mysh_release_resource(mysh_resource* res) {
    ms_relase(&res->current_dir);
    ms_relase(&res->home_dir);
}

#endif // MYSH_SHELL_RESOURCE_H