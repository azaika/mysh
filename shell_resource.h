#ifndef MYSH_SHELL_RESOURCE_H
#define MYSH_SHELL_RESOURCE_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mystring.h"

typedef struct {
    mysh_string current_dir;
    mysh_string home_dir;
} mysh_resource;

static mysh_resource mysh_shell_resource;

static void mysh_set_curdir_name(const char* name) {
    if (name == NULL) {
        return;
    }

    size_t name_len = strlen(name) + 1;
    if (name_len < mysh_shell_resource.home_dir.length) {
        ms_assign_raw(&mysh_shell_resource.current_dir, name);
        return;
    }

    int flag = 1;
    size_t i;
    for (i = 0; i + 1 < mysh_shell_resource.home_dir.length; ++i) {
        if (name[i] != mysh_shell_resource.home_dir.ptr[i]) {
            flag = 0;
            break;
        }
    }

    if (flag) {
        ms_extend(&mysh_shell_resource.current_dir, name_len - i + 1);
        mysh_shell_resource.current_dir.length = name_len - i + 1;

        mysh_shell_resource.current_dir.ptr[0] = '~';
        for (size_t j = 0; j < name_len - i; ++j) {
            mysh_shell_resource.current_dir.ptr[1 + j] = name[i + j];
        }

        mysh_shell_resource.current_dir.ptr[name_len - i + 1] = '\0';
    }
    else {
        ms_assign_raw(&mysh_shell_resource.current_dir, name);
    }
}

static int mysh_init_resource() {
    int error = 0;
    ms_init(&mysh_shell_resource.current_dir, "");
    ms_init(&mysh_shell_resource.home_dir, getenv("HOME"));
    mysh_set_curdir_name(get_current_dir_name());

    return error;
}

static int mysh_release_resource() {
    ms_relase(&mysh_shell_resource.current_dir);
    ms_relase(&mysh_shell_resource.home_dir);
    return 0;
}

static const char* mysh_get_shell_curdir() {
    return mysh_shell_resource.current_dir.ptr;
}

#endif // MYSH_SHELL_RESOURCE_H