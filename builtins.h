#ifndef MYSH_BUILTINS_H
#define MYSH_BUILTINS_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "shell_resource.h"

static const char* builtin_str[] = {
    "cd",
    "exit"
};

static int mysh_cd(mysh_resource* res, char** args);
static int mysh_exit(mysh_resource* res, char** args);

static int (*const builtin_func[]) (mysh_resource*, char**) = {
    mysh_cd,
    mysh_exit
};

static int mysh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char*);
}

int mysh_cd(mysh_resource* res, char** args) {
	if (args[1] == NULL) {
		return 0;
	}

	int err = chdir(args[1]);
	if (err) {
		perror("mysh");
	}
	else {
        mysh_set_curdir_name(res, get_current_dir_name());
	}

    return 0;
}

int mysh_exit(mysh_resource* res, char** args) {
    return 1;
}

#endif // MYSH_BUILTINS_H