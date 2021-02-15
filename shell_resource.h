#ifndef MYSH_SHELL_RESOURCE_H
#define MYSH_SHELL_RESOURCE_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>

#include "mystring.h"

typedef struct mysh_resource_tag {
    mysh_string current_dir;
    mysh_string home_dir;
    struct termios original_termios;
    int terminal_fd;
    bool is_interactive;
    pid_t group_id;
    void* first_job;
} mysh_resource;

static void mysh_set_curdir_name(mysh_resource* shell) {
    char* name = getcwd(NULL, 0);
    if (name == NULL) {
        return;
    }

    size_t name_len = strlen(name);
    if (name_len < shell->home_dir.length) {
        ms_assign_raw(&shell->current_dir, name);
        return;
    }

    int flag = 1;
    size_t i;
    for (i = 0; i < shell->home_dir.length; ++i) {
        if (name[i] != shell->home_dir.ptr[i]) {
            flag = 0;
            break;
        }
    }

    if (flag) {
        ms_reserve(&shell->current_dir, name_len - i + 1);
        shell->current_dir.length = name_len - i + 1;

        shell->current_dir.ptr[0] = '~';
        for (size_t j = 0; j < name_len - i; ++j) {
            shell->current_dir.ptr[1 + j] = name[i + j];
        }

        shell->current_dir.ptr[name_len - i + 1] = '\0';
    }
    else {
        ms_assign_raw(&shell->current_dir, name);
    }

    free(name);
}

static void mysh_release_resource(mysh_resource* shell) {
    ms_relase(&shell->current_dir);
    ms_relase(&shell->home_dir);
}

#endif // MYSH_SHELL_RESOURCE_H