#ifndef MYSH_SHELL_RESOURCE_H
#define MYSH_SHELL_RESOURCE_H

#include <stdlib.h>
#include <string.h>

#include <unistd.h>

typedef struct {
    char* current_dir;
    char* home_dir;
} mysh_resource;

static mysh_resource mysh_shell_resource;

static int mysh_init_resource() {
    int error = 0;
    error = error || (mysh_shell_resource.current_dir = get_current_dir_name()) == NULL;
    error = error || (mysh_shell_resource.home_dir = getenv("HOME")) == NULL;

    return error;
}

static int mysh_release_resource() {
    free(mysh_shell_resource.current_dir);
    return 0;
}

static char** mysh_shell_dir() {
    return &mysh_shell_resource.current_dir;
}

#endif // MYSH_SHELL_RESOURCE_H