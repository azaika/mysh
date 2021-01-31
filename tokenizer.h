#ifndef MYSH_TOKENIZER_H
#define MYSH_TOKENIZER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>

#include "mystring.h"
#include "redirect.h"

typedef enum {
	token_string,
	token_background,
	token_pipe,
	token_redirect
} mysh_token;

typedef struct {
	mysh_token token;
	void* data;
} mysh_tokenized_component;

void free_tokenized_component(mysh_tokenized_component* com) {
	if (com == NULL)
		return;
	
	if (com->token == token_string) {
		ms_free(com->data);
	}
	if (com->token == token_background) {
        // do nothing
	}
	if (com->token == token_pipe) {
		// do nothing
	}
	if (com->token == token_redirect) {
		ms_free(((mysh_redirect_data*)com->data)->filename);
		free(com->data);
	}

	free(com);
}

typedef struct {
	char* input;
	int pos;
	char last_char;
} mysh_cursor;

static int mysh_cursor_isend(mysh_cursor* cursor) {
	return cursor->last_char == '\0';
}

static char mysh_cursor_consume(mysh_cursor* cursor) {
	assert(cursor != NULL);

	if (cursor->last_char == 0)
		return 0;

	cursor->last_char = cursor->input[cursor->pos];
	if (cursor->last_char == EOF) {
		cursor->last_char = 0;
	}
	++cursor->pos;
	
	return cursor->last_char;
}

static char mysh_cursor_rollback(mysh_cursor* cursor) {
	assert(cursor != NULL);
	assert(cursor->pos > 0);

	--cursor->pos;
	cursor->last_char = cursor->input[cursor->pos];
	if (cursor->last_char == EOF) {
		cursor->last_char = 0;
	}
	
	return cursor->last_char;
}

static bool mysh_isdelim(char c) {
	const char* delims = " \t\r\n\a";

	for (int i = 0; delims[i] != 0; ++i)
		if (c == delims[i])
			return true;
	
	return false;
}

static char mysh_cursor_skip_delims(mysh_cursor* cursor) {
	char c;
	do {
		c = mysh_cursor_consume(cursor);
	} while(c != '\0' && mysh_isdelim(c));

	return c;
}

static char mysh_cursor_get_escaped(mysh_cursor* cursor, bool ignore_last) {
	if ((ignore_last ? mysh_cursor_consume(cursor) : cursor->last_char) == '\\') {
		if (mysh_cursor_consume(cursor) == 0)
			return 0;
		
		switch (cursor->last_char) {
			case 'n':
				return '\n';
			case 't':
				return '\t';
			default:
				return cursor->last_char;
		}
	}
	else
		return cursor->last_char;
}

static const char* mysh_expand_env(mysh_cursor* cursor) {
	mysh_string* var_name = ms_new();
	ms_init(var_name, "");

	char c = mysh_cursor_consume(cursor);
	while (isalnum(c) || c == '_') {
		ms_push(var_name, c);
		c = mysh_cursor_consume(cursor);
	}

	return getenv(var_name->ptr);
}

static mysh_tokenized_component* mysh_tokenize_string(mysh_cursor* cursor) {
	mysh_tokenized_component* ret = (mysh_tokenized_component*)malloc(sizeof(mysh_tokenized_component));
	if (ret == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
	}

	mysh_string* s = ms_new();
	if (mysh_cursor_isend(cursor)) {
		ms_init(s, "");
		ret->token = token_string;
		ret->data = s;
		return ret;
	}

	bool is_quoted = cursor->last_char == '"' || cursor->last_char == '\'';
	char quote = (cursor->last_char == '"' ? '"' : '\'');

	char c = (is_quoted ? mysh_cursor_consume(cursor) : cursor->last_char);
	while (!mysh_cursor_isend(cursor) && (is_quoted ? cursor->last_char != quote : !mysh_isdelim(c))) {
		if (c == '\\') {
			ms_push(s, mysh_cursor_get_escaped(cursor, false));
		}
		else if (c == '$') {
			const char *var = mysh_expand_env(cursor);
			ms_append_raw(s, (var != NULL ? var : ""));
		}
		else if (c == '<' || c == '>') {
			mysh_cursor_rollback(cursor);
			break;
		}
		else if (!is_quoted && (c == '"' || c == '\'')) {
			ms_free(s);
			return NULL;
		}
		else {
			ms_push(s, c);
		}

		c = mysh_cursor_consume(cursor);
	};

	ret->token = token_string;
	ret->data = s;

	return ret;
}

static mysh_tokenized_component* mysh_tokenize_pipe(mysh_cursor* cursor) {
	if (cursor->last_char != '|') {
		return NULL;
	}
	
	mysh_tokenized_component* ret = (mysh_tokenized_component*)malloc(sizeof(mysh_tokenized_component));
	if (ret == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
	}

	ret->token = token_pipe;
	ret->data = NULL;

	return ret;
}

static mysh_tokenized_component* mysh_tokenize_background(mysh_cursor* cursor) {
	mysh_tokenized_component* ret = (mysh_tokenized_component*)malloc(sizeof(mysh_tokenized_component));
	if (ret == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
	}

	ret->token = token_background;
	ret->data = NULL;

	return ret;
}

// filename will not be set
static mysh_tokenized_component* mysh_tokenize_redirect(mysh_cursor* cursor) {
	int ffd = -1, tfd = -1;
	while (isdigit(cursor->last_char)) {
		if (ffd == -1) {
			ffd = 0;
		}

		ffd *= 10;
		ffd += cursor->last_char - '0';
		mysh_cursor_consume(cursor);
	}

	mysh_redirect kind;
	switch (cursor->last_char) {
		case '>':
			if (mysh_cursor_consume(cursor) == '>') {
				kind = redirect_out_append;
				tfd = ffd;
				ffd = -1;
			}
			else if (cursor->last_char == '&') {
				kind = redirect_fd;
				while (isdigit(mysh_cursor_consume(cursor))) {
					if (tfd == -1) {
						tfd = 0;
					}

					tfd *= 10;
					tfd += cursor->last_char - '0';
				}

				if (tfd == -1) {
					return NULL;
				}
				if (ffd == -1) {
					ffd = 1;
				}
			}
			else {
				kind = redirect_out;
				tfd = ffd;
				ffd = -1;
				mysh_cursor_rollback(cursor);
			}
			break;
		case '<':
			kind = redirect_in;
			break;
		default:
			return NULL;
	}

	mysh_redirect_data* data = (mysh_redirect_data*)malloc(sizeof(mysh_redirect_data));

	mysh_tokenized_component* ret = (mysh_tokenized_component*)malloc(sizeof(mysh_tokenized_component));
	if (data == NULL || ret == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
	}

	data->ffd = ffd;
	data->tfd = tfd;
	data->filename = ms_new();
	data->kind = kind;

	ret->token = token_redirect;
	ret->data = data;

	return ret;
}

static mysh_tokenized_component** mysh_tokenize(char* line, int* written_size) {
	assert(line != NULL);
	assert(written_size != NULL);

	int capacity = 8;
	int size = 0;
	mysh_tokenized_component** components = (mysh_tokenized_component**)malloc(sizeof(void*) * capacity);
	if (components == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
		exit(EXIT_FAILURE);
	}

	mysh_cursor cursor;
	cursor.last_char = -1;
	cursor.input = line;
	cursor.pos = 0;

	while (!mysh_cursor_isend(&cursor)) {
		char c = mysh_cursor_skip_delims(&cursor);

		if (c == 0) {
			break;
		}

		mysh_tokenized_component* com;
		if (c == '|') {
			com = mysh_tokenize_pipe(&cursor);
		}
		else if (c == '&') {
			com = mysh_tokenize_background(&cursor);
		}
		else if (isdigit(c)) {
			int pos = cursor.pos;
			com = mysh_tokenize_redirect(&cursor);
			if (com == NULL) {
				cursor.pos = pos;
				cursor.last_char = c;
				com = mysh_tokenize_string(&cursor);
			}
		}
		else if (c == '<' || c == '>') {
			com = mysh_tokenize_redirect(&cursor);
		}
		else {
			com = mysh_tokenize_string(&cursor);
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

	*written_size = size;

	return components;
}

#endif // MYSH_TOKENIZER_H