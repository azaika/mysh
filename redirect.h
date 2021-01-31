#ifndef MYSH_REDIRECT_H
#define MYSH_REDIRECT_H

#include <stdio.h>
#include <assert.h>

#include <fcntl.h>

#include "mystring.h"

typedef enum {
	redirect_out,
	redirect_in,
	redirect_out_append,
	redirect_fd
} mysh_redirect;

typedef struct {
	int ffd;
	int tfd;
	mysh_string* filename;
	mysh_redirect kind;
} mysh_redirect_data;

// open file if necessary
static bool mysh_open_file(mysh_redirect_data* red) {
    switch (red->kind) {
    case redirect_in:
        assert(!ms_is_empty(red->filename));

        red->tfd = 0;
        red->ffd = open(red->filename->ptr, O_RDONLY, 0666);
        if (red->ffd < 0) {
            perror("mysh: failed to open output file to redirect:");
            return false;
        }

        break;
    case redirect_out:
        assert(!ms_is_empty(red->filename));

        if (red->tfd == -1) {
            red->tfd = 1;
        }
        red->ffd = open(red->filename->ptr, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (red->ffd < 0) {
            perror("mysh: failed to open output file to redirect:");
            return false;
        }

        break;
    case redirect_out_append:
        assert(!ms_is_empty(red->filename));

        if (red->tfd == -1) {
            red->tfd = 1;
        }
        red->ffd = open(red->filename->ptr, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (red->ffd < 0) {
            perror("mysh: failed to open output file to redirect:");
            return false;
        }

        break;
    case redirect_fd:
        // do nothing
        return true;
    default:
        // unreachable
        exit(EXIT_FAILURE);
    }

    return true;
}

static bool mysh_close_file(mysh_redirect_data* red) {
    assert(red != NULL);

    if (red->ffd > 2) {
        if (close(red->ffd) < 0) {
            perror("mysh: failed to close FD:");
            return false;
        }
    }
    if (red->tfd > 2) {
        if (close(red->tfd) < 0) {
            perror("mysh: failed to close FD:");
            return false;
        }
    }

    return true;
}

static void mysh_release_redirect(mysh_redirect_data* red) {
    if (red->filename != NULL) {
        ms_free(red->filename);
    }

    free(red);
}

#endif // MYSH_REDIRECT_H