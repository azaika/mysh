#ifndef MYSH_PARSER_H
#define MYSH_PARSER_H

#include "tokenizer.h"

static mysh_tokenized_component** mysh_parse_input(char* line) {
	int capacity = 8;
	int size = 0;
	mysh_tokenized_component** components = (mysh_tokenized_component**)malloc(sizeof(void*) * capacity);
	if (components == NULL) {
		return NULL;
	}

	mysh_cursor parser;
	parser.input = line;
	parser.pos = 0;

	while (!mysh_cursor_isend(&parser)) {
		char c = mysh_cursor_skip_delims(&parser);

		if (c == 0) {
			break;
		}

		mysh_tokenized_component* com;
		if (c == '|'){
			com = mysh_tokenize_pipe(&parser);
		} if (c == ';' || c == '&') {
			com = mysh_tokenize_jobcontrol(&parser);
		}
		else if (isdigit(c)) {
			int pos = parser.pos;
			com = mysh_tokenize_redirect(&parser);
			if (com == NULL) {
				parser.pos = pos;
				parser.last_char = c;
				com = mysh_tokenize_string(&parser);
			}
		}
		else if (c == '<' || c == '>') {
			com = mysh_tokenize_redirect(&parser);
		}
		else {
			com = mysh_tokenize_string(&parser);
		}

		if (com == NULL) {
			for (int i = 0; i < size; ++i) {
				free_tokenized_component(components[i]);
			}

			return NULL;
		}

		if (capacity < size + 1) {
			capacity *= 2;
			components = (mysh_tokenized_component**)realloc(components, sizeof(void*) * capacity);
		}
		components[size] = com;
		++size;
	}

	if (capacity < size + 1) {
		capacity *= 2;
		components = (mysh_tokenized_component**)realloc(components, sizeof(void*) * capacity);
	}
	components[size] = NULL;

	return components;
}

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