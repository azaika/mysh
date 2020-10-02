#ifndef MYSH_PARSER_H
#define MYSH_PARSER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// caller must free() return value
static char** mysh_split_line(char* line) {
    const int num_args_unit = 32;
    const char* delims = " \t\r\n\a";

	int buf_size = num_args_unit;
	int pos = 0;

	char** args = (char**)malloc(buf_size * sizeof(char*));
	if (!args) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
		exit(EXIT_FAILURE);
	}

	char* arg = strtok(line, delims);
	while (arg != NULL) {
		args[pos] = arg;
		++pos;

		if (pos >= buf_size) {
			buf_size += num_args_unit;
			args = (char**)realloc(args, buf_size * sizeof(char*));
			if (!args) {
				fprintf(stderr, "mysh: error occurred in allocation.\n");
				exit(EXIT_FAILURE);
			}
		}

        arg = strtok(NULL, delims);
	}

    args[pos] = NULL;

    return args;
}

#endif // MYSH_PARSER_H