#ifndef MYSH_PARSER_H
#define MYSH_PARSER_H

#include "tokenizer.h"
#include "process.h"
#include "builtins.h"

#include <assert.h>
#include <stdio.h>

static bool mysh_parse_tokens(mysh_tokenized_component** coms, int size, mysh_process* top, bool* is_foreground) {
	assert(coms != NULL);
	assert(size > 0);
	assert(top != NULL);

	*is_foreground = true;

	mysh_process* cur = top;
	for (int i = 0; i < size; ++i) {
		if (coms[i]->token == token_background) {
			if (i + 1 != size) {
				mysh_release_process(top);
				fprintf(stderr, "mysh: '&' must be placed in end of line\n");
				return false;
			}
			else {
				*is_foreground = false;
			}
		}
		if (coms[i]->token == token_string) {
			if (cur->num_redirects != 0) {
				mysh_release_process(top);
				fprintf(stderr, "mysh: program arguments must appear before redirects\n");
				return false;
			}

			++cur->argc;
			if (cur->argv == NULL) {
				cur->argv = (char**)malloc(sizeof(char*));
			}
			else {
				cur->argv = (char**)realloc(cur->argv, sizeof(char*) * (cur->argc + 1));
			}

			if (cur->argv == NULL) {
				fprintf(stderr, "mysh: error occurred in allocation.\n");
        		exit(EXIT_FAILURE);
			}

			cur->argv[cur->argc - 1] = (char*)malloc(sizeof(char) * (((mysh_string*)coms[i]->data)->length + 1));
			if (cur->argv[cur->argc - 1] == NULL) {
				fprintf(stderr, "mysh: error occurred in allocation.\n");
        		exit(EXIT_FAILURE);
			}

			cur->argv[cur->argc - 1] = ms_into_chars((mysh_string*)coms[i]->data);

			cur->argv[cur->argc] = NULL;
		}
		if (coms[i]->token == token_pipe) {
			cur->next = mysh_new_process();
			cur = cur->next;
		}
		if (coms[i]->token == token_redirect) {
			if (cur->argc == 0) {
				mysh_release_process(top);
				fprintf(stderr, "mysh: please specify program name\n");
				return false;
			}

			++cur->num_redirects;
			if (cur->redirects == NULL) {
				cur->redirects = (mysh_redirect_data*)malloc(sizeof(mysh_redirect_data));
			}
			else {
				cur->redirects = (mysh_redirect_data*)realloc(cur->redirects, sizeof(mysh_redirect_data*) * cur->num_redirects);
			}

			if (cur->redirects == NULL) {
				fprintf(stderr, "mysh: error occurred in allocation.\n");
        		exit(EXIT_FAILURE);
			}

			mysh_redirect_data* red = (mysh_redirect_data*)coms[i]->data;

			cur->redirects[cur->num_redirects - 1].kind = red->kind;
			cur->redirects[cur->num_redirects - 1].ffd = red->ffd;
			cur->redirects[cur->num_redirects - 1].tfd = red->tfd;
			if (red->kind != redirect_fd) {
				if (i + 1 == size || coms[i + 1]->token != token_string) {
					fprintf(stderr, "mysh: please specify output file of redirection\n");
					return false;
				}
				ms_assign(red->filename, (mysh_string*)coms[i + 1]->data);

				++i;
			}
			if (red->kind == redirect_out && red->tfd == -1) {
				red->tfd = 0;
			}
			
			if (!ms_is_empty(red->filename)) {
				cur->redirects[cur->num_redirects - 1].filename = ms_new();
				ms_assign(cur->redirects[cur->num_redirects - 1].filename, red->filename);
			}
			else {
				cur->redirects[cur->num_redirects - 1].filename = red->filename;
			}
		}
	}

	return true;
}

static bool mysh_parse_input(char* line, mysh_process* proc, bool* is_foreground) {
	assert(line != NULL);
	assert(proc != NULL);
	assert(is_foreground != NULL);

	int size = 0;
	mysh_tokenized_component** components = mysh_tokenize(line, &size);

	if (components == NULL || size <= 0) {
		return false;
	}

	return mysh_parse_tokens(components, size, proc, is_foreground);
}

#endif // MYSH_PARSER_H