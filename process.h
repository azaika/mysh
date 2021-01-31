#ifndef MYSH_PROCESS_H
#define MYSH_PROCESS_H

#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <termios.h>

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
    for (int i = 0; i < proc->num_redirects; ++i) {
        mysh_release_redirect(&proc->redirects[i]);
    }
    for (int i = 0; i < proc->argc; ++i) {
        free(proc->argv[i]);
    }

    free(proc->redirects);
    free(proc->argv);
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
            perror("mysh: failed to duplicate FD:");
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
                perror("mysh: failed to close FD:");
                exit(EXIT_FAILURE);
            }
        }
    }

    execvp(proc->argv[0], proc->argv);
    perror("mysh: failed to call execvp():");
    exit(EXIT_FAILURE);
}

struct mysh_job_tag {
    struct mysh_job_tag* next;
    mysh_string command;
    mysh_process* first_proc;
    pid_t group_id;
    bool is_notified;
    struct termios tmodes;
    int in_fd, out_fd, err_fd;
};

typedef struct mysh_job_tag mysh_job;

static void mysh_release_job(mysh_job* job) {
    mysh_release_process(job->first_proc);
    free(job);
}

static void mysh_fprint_job(FILE* file, mysh_job* job, mysh_string* status) {
    fprintf(file, "%ld (%s): %s\n", job->group_id, status, job->command);
}

static mysh_job* mysh_find_job(mysh_job* job, pid_t group_id) {
    while (job != NULL) {
        if (job->group_id == group_id) {
            return job;
        }

        job = job->next;
    }

    return NULL;
}

static bool mysh_is_job_stopped(mysh_job* job) {
    for (mysh_process* proc = job->first_proc; proc != NULL; proc = proc->next) {
        if (!proc->is_stopped && !proc->is_completed) {
            return false;
        }
    }

    return true;
}

static bool mysh_is_job_completed(mysh_job* job) {
    for (mysh_process* proc = job->first_proc; proc != NULL; proc = proc->next) {
        if (!proc->is_completed) {
            return false;
        }
    }

    return true;
}

bool mysh_set_status(mysh_job* first_job, pid_t pid, int code) {
    assert(pid >= 0);

    if (pid == 0 || errno == ECHILD) {
        return false;
    }

    for (mysh_job* job = first_job; job != NULL; job = job->next) {
        for (mysh_process* proc = job->first_proc; proc != NULL; proc = proc->next) {
            if (proc->pid == pid) {
                proc->status = code;

                if (WIFSTOPPED(code)) {
                    proc->is_stopped = true;
                }
                else {
                    proc->is_completed = true;
                }

                return true;
            }
        }
    }

    // not found
    return false;
}

void mysh_wait_job (mysh_resource* res, mysh_job* job) {
    int code;
    pid_t pid;
    do {
        pid = waitpid (WAIT_ANY, &code, WUNTRACED);
    } while (!mysh_set_status(job, pid, code) && !mysh_is_job_stopped(job) && !mysh_is_job_completed(job));
}

void mysh_update(mysh_resource* res) {
    int code;
    pid_t pid;
    do {
        pid = waitpid(WAIT_ANY, &code, WUNTRACED | WNOHANG);
    } while(!mysh_set_status(res, pid, code));
}

static void mysh_put_job_foreground(mysh_resource* res, mysh_job* job, bool do_continue) {
    tcsetpgrp(res->terminal_fd, job->group_id);

    if (do_continue) {
        tcsetattr(res->terminal_fd, TCSADRAIN, &job->tmodes);

        if (kill(-job->group_id, SIGCONT) < 0) {
            perror("mysh: failed to send SIGCONT");
        }
    }

    mysh_wait_job(res, job);
    tcsetpgrp(res->terminal_fd, res->group_id);
    tcgetattr(res->terminal_fd, &job->tmodes);
    tcsetattr(res->terminal_fd, TCSADRAIN, &res->original_termios);
}

static bool mysh_launch_job(mysh_resource* res, mysh_job* job, bool is_interactive, bool is_foreground) {
    assert(job !=NULL);

    int in_fd = job->in_fd;
    for (mysh_process* proc = job->first_proc; proc != NULL; proc = proc->next) {
        int out_fd;
        int cur_pipe[2];
        if (proc->next == NULL) {
            out_fd = job->out_fd;
        }
        else {
            if (pipe(cur_pipe) < 0) {
                perror("mysh: failed to create pipe");
                exit(EXIT_FAILURE);
            }

            out_fd = cur_pipe[1];
        }

        for (int i = 0; i < proc->num_redirects; ++i) {
            if (!mysh_open_file(&proc->redirects[i])) {
                return false;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("mysh: failed to fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) {
            // child
            mysh_exec_process(res, proc, job->group_id, in_fd, out_fd, job->err_fd, is_foreground);
        }
        else {
            // parent
            proc->pid = pid;
            if (is_interactive) {
                if (job->group_id != 0) {
                    job->group_id = pid;
                }

                setpgid(pid, job->group_id);
            }

            for (int i = 0; i < proc->num_redirects; ++i) {
                if (!mysh_close_file(&proc->redirects[i])) {
                    return false;
                }
            }
        }

        if (in_fd != job->in_fd) {
            close(in_fd);
        }
        if (out_fd != job->out_fd) {
            close(out_fd);
        }
        in_fd = cur_pipe[0];
    }

    if (res->is_interactive) {
        mysh_wait_job(res, job);
    } else if (is_foreground) {
        mysh_put_job_foreground(res, job, false);
    }

    return true;
}

static mysh_job* mysh_list_and_clear_jobs(mysh_resource* res, mysh_job* first_job) {
    mysh_job* cur_job;
    bool is_first = true;
    while (cur_job != NULL) {
        mysh_job* next_job = cur_job->next;
        if (mysh_is_job_completed(cur_job)) {
            mysh_fprint_job(stdout, cur_job, "completed");
            if (true) {
                first_job = next_job;
            }
            else {
                next_job = cur_job->next;
            }

            mysh_release_job(cur_job);
        }
        else if (mysh_is_job_stopped(cur_job) && !cur_job->is_notified) {
            mysh_fprint_job(stdout, cur_job, "stopped");
            cur_job->is_notified = true;
            is_first = false;
        }
        else {
            is_first = false;
        }

        cur_job = next_job;
    }

    return first_job;
}

static bool mysh_resume_job(mysh_resource* res, mysh_job* job, bool is_foreground) {
    if (mysh_is_job_completed(job) || !mysh_is_job_stopped(job)) {
        return true;
    }
    for (mysh_process* proc = job->first_proc; proc != NULL; proc = proc->next) {
        proc->is_stopped = false;
    }

    job->is_notified = false;

    if (is_foreground) {
        mysh_put_job_foreground(res, job, true);
    }
    else if (kill(- job->group_id, SIGCONT) < 0) {
        perror("mysh: failed to send SIGCONT: ");
        return false;
    }

    return true;
}

#endif // MYSH_PROCESS_H