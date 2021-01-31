#ifndef MYSH_PROCESS_H
#define MYSH_PROCESS_H

#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include <unistd.h>
#include <signal.h>

#include "mystring.h"
#include "redirect.h"
#include "tokenizer.h"
#include "shell_resource.h"

struct mysh_process_tag;

struct mysh_process_tag {
    struct mysh_process_tag* next;
    char** argv;
    int argc;
    mysh_redirect_data* redirects;
    int num_redirects;
    
    bool is_completed;
    bool is_stopped;
    pid_t pid;
    int status;
};

typedef struct mysh_process_tag mysh_process;

static mysh_process* mysh_new_process() {
    mysh_process* proc = (mysh_process*)malloc(sizeof(mysh_process));
    if (proc == NULL) {
        fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
    }

    proc->argv = NULL;
    proc->argc = 0;
    proc->redirects = NULL;
    proc->num_redirects = 0;
    proc->next = NULL;
    proc->is_completed = false;
    proc->is_stopped = false;
    proc->pid = 0;

    return proc;
}

static void mysh_release_process(mysh_process* proc) {
    assert(proc != NULL);

    if (proc->next != NULL) {
        mysh_release_process(proc->next);
    }

    for (int i = 0; i < proc->num_redirects; ++i) {
        mysh_release_redirect(&proc->redirects[i]);
    }
    
    for (int i = 0; i < proc->argc; ++i) {
        free(proc->argv[i]);
    }

    free(proc->redirects);
    free(proc->argv);
    free(proc);
}

static void mysh_exec_process(mysh_resource* res, mysh_process* proc, pid_t group_id, int in_fd, int out_fd, int err_fd, bool is_foreground) {
    if (res->is_interactive) {
        pid_t pid = getpid();

        if (res->group_id == 0) {
            res->group_id = pid;
        }

        setpgid(pid, res->group_id);
        if (is_foreground) {
            tcsetpgrp(res->terminal_fd, res->group_id);
        }

        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
    }

    if (in_fd != STDIN_FILENO) {
        if (dup2(in_fd, STDIN_FILENO) < 0) {
            perror("mysh: failed to duplicate FD");
            exit(EXIT_FAILURE);
        }
        close(in_fd);
    }
    if (out_fd != STDOUT_FILENO) {
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);
    }
    if (err_fd != STDERR_FILENO) {
        dup2(err_fd, STDERR_FILENO);
        close(err_fd);
    }

    for (int i = 0; i < proc->num_redirects; ++i) {
        mysh_redirect_data* red = &proc->redirects[i];
        dup2(red->ffd, red->tfd);

        if (red->kind != redirect_fd) {
            if (close(red->ffd) < 0) {
                perror("mysh: failed to close FD");
                exit(EXIT_FAILURE);
            }
        }
    }

    execvp(proc->argv[0], proc->argv);
    perror("mysh: failed to call execvp()");
    exit(EXIT_FAILURE);
}

#endif // MYSH_PROCESS_H