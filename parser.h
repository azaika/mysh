#ifndef MYSH_PARSER_H
#define MYSH_PARSER_H

#include "mystring.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>

typedef enum {
	token_string,
	token_jobcontrol,
	token_redirect
} mysh_token;

typedef struct {
	mysh_token token;
	void* data;
} mysh_parsed_component;

typedef struct {
	int ffd;
	int tfd;
	mysh_string* str;
} mysh_redirect_component;

void free_parsed_component(mysh_parsed_component* com) {
	if (com == NULL)
		return;
	
	if (com->token == token_string) {
		ms_free(com->data);
	}
	if (com->token == token_jobcontrol) {
		ms_free(com->data);
	}
	if (com->token == token_redirect) {
		ms_free(((mysh_redirect_component*)com->data)->str);
		free(com->data);
	}

	free(com);
}

typedef struct {
	char* input;
	int pos;
	char last_char;
} mysh_parser;

static int mysh_parse_isend(mysh_parser* parser) {
	return parser->last_char == '\0';
}

static char mysh_consume(mysh_parser* parser) {
	assert(parser != NULL);

	if (parser->last_char == 0)
		return 0;

	parser->last_char = parser->input[parser->pos];
	if (parser->last_char == EOF) {
		parser->last_char = 0;
	}
	++parser->pos;
	
	return parser->last_char;
}

static char mysh_rollback(mysh_parser* parser) {
	assert(parser != NULL);
	assert(parser->pos > 0);

	--parser->pos;
	parser->last_char = parser->input[parser->pos];
	if (parser->last_char == EOF) {
		parser->last_char = 0;
	}
	
	return parser->last_char;
}

static bool mysh_isdelim(char c) {
	const char* delims = " \t\r\n\a";

	for (int i = 0; delims[i] != 0; ++i)
		if (c == delims[i])
			return true;
	
	return false;
}

static char mysh_parse_skip_delims(mysh_parser* parser) {
	char c;
	do {
		c = mysh_consume(parser);
	} while(c != '\0' && mysh_isdelim(c));

	return c;
}

static char mysh_parse_escaped(mysh_parser* parser, bool ignore_last) {
	if ((ignore_last ? mysh_consume(parser) : parser->last_char) == '\\') {
		if (mysh_consume(parser) == 0)
			return 0;
		
		switch (parser->last_char) {
			case 'n':
				return '\n';
			case 't':
				return '\t';
			default:
				return parser->last_char;
		}
	}
	else
		return parser->last_char;
}

static const char* mysh_expand_env(mysh_parser* parser) {
	mysh_string* var_name = ms_new();
	ms_init(var_name, "");
	
	char c = mysh_consume(parser);
	while (isalnum(c) || c == '_') {
		ms_push(var_name, c);
		mysh_consume(parser);
	}

	return getenv(var_name->ptr);
}

static mysh_parsed_component* mysh_parse_string(mysh_parser* parser) {
	mysh_parsed_component* ret = (mysh_parsed_component*)malloc(sizeof(mysh_parsed_component));
	if (ret == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
	}

	mysh_string* s = ms_new();
	if (mysh_parse_isend(parser)) {
		ms_init(s, "");
		ret->token = token_string;
		ret->data = s;
		return ret;
	}

	bool is_quoted = parser->last_char == '"' || parser->last_char == '\'';
	char quote = (parser->last_char == '"' ? '"' : '\'');

	char c = (is_quoted ? mysh_consume(parser) : parser->last_char);
	while (!mysh_parse_isend(parser) && (is_quoted ? parser->last_char != quote : !mysh_isdelim(c))) {
		if (c == '\\') {
			ms_push(s, mysh_parse_escaped(parser, false));
		}
		else if (c == '$') {
			const char *var = mysh_expand_env(parser);
			ms_append_raw(s, (var != NULL ? var : ""));
		}
		else if (c == '<' || c == '>') {
			mysh_rollback(parser);
			break;
		}
		else if (!is_quoted && (c == '"' || c == '\'')) {
			ms_free(s);
			return NULL;
		}
		else {
			ms_push(s, c);
		}

		c = mysh_consume(parser);
	};

	ret->token = token_string;
	ret->data = s;

	return ret;
}

static mysh_parsed_component* mysh_parse_jobcontrol(mysh_parser* parser) {
	mysh_string* s = ms_new();

	switch (parser->last_char) {
		case '|':
			ms_assign_raw(s, "|");
			break;
		case ';':
			ms_assign_raw(s, ";");
			break;
		case '&':
			if (mysh_consume(parser) == '&') {
				ms_assign_raw(s, "&&");
			}
			else {
				ms_assign_raw(s, "&");
				mysh_rollback(parser);
			}
			break;
		default:
			ms_free(s);
			return NULL;
	}

	mysh_parsed_component* ret = (mysh_parsed_component*)malloc(sizeof(mysh_parsed_component));
	if (ret == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
	}

	ret->token = token_jobcontrol;
	ret->data = s;

	return ret;
}

static mysh_parsed_component* mysh_parse_redirect(mysh_parser* parser) {
	mysh_string* s = ms_new();

	int ffd = -1, tfd = -1;
	while (isdigit(parser->last_char)) {
		if (ffd == -1) {
			ffd = 0;
		}

		ffd *= 10;
		ffd += parser->last_char - '0';
		mysh_consume(parser);
	}

	switch (parser->last_char) {
		case '>':
			if (mysh_consume(parser) == '>') {
				ms_assign_raw(s, ">>");
			}
			else if (parser->last_char == '&') {
				ms_assign_raw(s, ">&");
				while (isdigit(mysh_consume(parser))) {
					if (tfd == -1) {
						tfd = 0;
					}

					tfd *= 10;
					tfd += parser->last_char - '0';
				}

				if (tfd == -1) {
					mysh_rollback(parser);
				}
			}
			else {
				ms_assign_raw(s, ">");
				mysh_rollback(parser);
			}
			break;
		case '<':
			ms_assign_raw(s, "<");
			break;
		default:
			ms_free(s);
			return NULL;
	}

	mysh_redirect_component* data = (mysh_redirect_component*)malloc(sizeof(mysh_redirect_component));

	mysh_parsed_component* ret = (mysh_parsed_component*)malloc(sizeof(mysh_parsed_component));
	if (data == NULL || ret == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
	}

	data->ffd = ffd;
	data->tfd = tfd;
	data->str = s;

	ret->token = token_redirect;
	ret->data = data;

	return ret;
}

static mysh_parsed_component** mysh_parse_input(char* line) {
	int capacity = 8;
	int size = 0;
	mysh_parsed_component** components = (mysh_parsed_component**)malloc(sizeof(void*) * capacity);
	if (components == NULL) {
		return NULL;
	}

	mysh_parser parser;
	parser.input = line;
	parser.pos = 0;

	while (!mysh_parse_isend(&parser)) {
		char c = mysh_parse_skip_delims(&parser);

		if (c == 0) {
			break;
		}

		mysh_parsed_component* com;
		if (c == '|' || c == ';' || c == '&') {
			com = mysh_parse_jobcontrol(&parser);
		}
		else if (isdigit(c)) {
			int pos = parser.pos;
			com = mysh_parse_redirect(&parser);
			if (com == NULL) {
				parser.pos = pos;
				parser.last_char = c;
				com = mysh_parse_string(&parser);
			}
		}
		else if (c == '<' || c == '>') {
			com = mysh_parse_redirect(&parser);
		}
		else {
			com = mysh_parse_string(&parser);
		}

		if (com == NULL) {
			for (int i = 0; i < size; ++i) {
				free_parsed_component(components[i]);
			}

			return NULL;
		}

		if (capacity < size + 1) {
			capacity *= 2;
			components = (mysh_parsed_component**)malloc(sizeof(void*) * capacity);
		}
		components[size] = com;
		++size;
	}

	if (capacity < size + 1) {
		capacity *= 2;
		components = (mysh_parsed_component**)malloc(sizeof(void*) * capacity);
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