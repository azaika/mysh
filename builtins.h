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

static int mysh_cd(char** args);
static int mysh_exit(char** args);

static int (*const builtin_func[]) (char**) = {
    mysh_cd,
    mysh_exit
};

static int mysh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char*);
}

int mysh_cd(char** args) {
	if (args[1] == NULL) {
		return 0;
	}

	int err = chdir(args[1]);
	if (err) {
		perror("mysh");
	}
	else {
        char** p = mysh_shell_dir();
        *p = get_current_dir_name();
	}

    return 0;
}
int mysh_exit(char** args) {
    puts("exit()");
    return 0;
}

#endif // MYSH_BUILTINS_H