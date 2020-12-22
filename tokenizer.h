#ifndef MYSH_TOKENIZER_H
#define MYSH_TOKENIZER_H

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
	token_pipe,
	token_redirect
} mysh_token;

typedef struct {
	mysh_token token;
	void* data;
} mysh_tokenized_component;

typedef struct {
	int ffd;
	int tfd;
	mysh_string* str;
} mysh_redirect_component;

void free_tokenized_component(mysh_tokenized_component* com) {
	if (com == NULL)
		return;
	
	if (com->token == token_string) {
		ms_free(com->data);
	}
	if (com->token == token_jobcontrol) {
		ms_free(com->data);
	}
	if (com->token == token_pipe) {
		// do nothing
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
} mysh_cursor;

static int mysh_cursor_isend(mysh_cursor* parser) {
	return parser->last_char == '\0';
}

static char mysh_cursor_consume(mysh_cursor* parser) {
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

static char mysh_cursor_rollback(mysh_cursor* parser) {
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

static char mysh_cursor_skip_delims(mysh_cursor* parser) {
	char c;
	do {
		c = mysh_cursor_consume(parser);
	} while(c != '\0' && mysh_isdelim(c));

	return c;
}

static char mysh_cursor_get_escaped(mysh_cursor* parser, bool ignore_last) {
	if ((ignore_last ? mysh_cursor_consume(parser) : parser->last_char) == '\\') {
		if (mysh_cursor_consume(parser) == 0)
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

static const char* mysh_expand_env(mysh_cursor* parser) {
	mysh_string* var_name = ms_new();
	ms_init(var_name, "");

	char c = mysh_cursor_consume(parser);
	while (isalnum(c) || c == '_') {
		ms_push(var_name, c);
		c = mysh_cursor_consume(parser);
	}

	return getenv(var_name->ptr);
}

static mysh_tokenized_component* mysh_tokenize_string(mysh_cursor* parser) {
	mysh_tokenized_component* ret = (mysh_tokenized_component*)malloc(sizeof(mysh_tokenized_component));
	if (ret == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
	}

	mysh_string* s = ms_new();
	if (mysh_cursor_isend(parser)) {
		ms_init(s, "");
		ret->token = token_string;
		ret->data = s;
		return ret;
	}

	bool is_quoted = parser->last_char == '"' || parser->last_char == '\'';
	char quote = (parser->last_char == '"' ? '"' : '\'');

	char c = (is_quoted ? mysh_cursor_consume(parser) : parser->last_char);
	while (!mysh_cursor_isend(parser) && (is_quoted ? parser->last_char != quote : !mysh_isdelim(c))) {
		if (c == '\\') {
			ms_push(s, mysh_cursor_get_escaped(parser, false));
		}
		else if (c == '$') {
			const char *var = mysh_expand_env(parser);
			ms_append_raw(s, (var != NULL ? var : ""));
		}
		else if (c == '<' || c == '>') {
			mysh_cursor_rollback(parser);
			break;
		}
		else if (!is_quoted && (c == '"' || c == '\'')) {
			ms_free(s);
			return NULL;
		}
		else {
			ms_push(s, c);
		}

		c = mysh_cursor_consume(parser);
	};

	ret->token = token_string;
	ret->data = s;

	return ret;
}

static mysh_tokenized_component* mysh_tokenize_pipe(mysh_cursor* parser) {
	if (parser->last_char != '|') {
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

static mysh_tokenized_component* mysh_tokenize_jobcontrol(mysh_cursor* parser) {
	mysh_string* s = ms_new();

	switch (parser->last_char) {
		case ';':
			ms_assign_raw(s, ";");
			break;
		case '&':
			if (mysh_cursor_consume(parser) == '&') {
				ms_assign_raw(s, "&&");
			}
			else {
				ms_assign_raw(s, "&");
				mysh_cursor_rollback(parser);
			}
			break;
		default:
			ms_free(s);
			return NULL;
	}

	mysh_tokenized_component* ret = (mysh_tokenized_component*)malloc(sizeof(mysh_tokenized_component));
	if (ret == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
	}

	ret->token = token_jobcontrol;
	ret->data = s;

	return ret;
}

static mysh_tokenized_component* mysh_tokenize_redirect(mysh_cursor* parser) {
	mysh_string* s = ms_new();

	int ffd = -1, tfd = -1;
	while (isdigit(parser->last_char)) {
		if (ffd == -1) {
			ffd = 0;
		}

		ffd *= 10;
		ffd += parser->last_char - '0';
		mysh_cursor_consume(parser);
	}

	switch (parser->last_char) {
		case '>':
			if (mysh_cursor_consume(parser) == '>') {
				ms_assign_raw(s, ">>");
			}
			else if (parser->last_char == '&') {
				ms_assign_raw(s, ">&");
				while (isdigit(mysh_cursor_consume(parser))) {
					if (tfd == -1) {
						tfd = 0;
					}

					tfd *= 10;
					tfd += parser->last_char - '0';
				}

				if (tfd == -1) {
					mysh_cursor_rollback(parser);
				}
			}
			else {
				ms_assign_raw(s, ">");
				mysh_cursor_rollback(parser);
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

	mysh_tokenized_component* ret = (mysh_tokenized_component*)malloc(sizeof(mysh_tokenized_component));
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

#endif // MYSH_TOKENIZER_H