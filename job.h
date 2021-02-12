#ifndef MYSH_JOB_H
#define MYSH_JOB_H

#include "mystring.h"
#include "redirect.h"
#include "tokenizer.h"
#include "shell_resource.h"
#include "process.h"

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <termios.h>

struct mysh_job_tag {
    struct mysh_job_tag* next;
    mysh_string command;
    mysh_process* first_proc;
    pid_t group_id;
    bool is_notified;
    struct termios termios;
    int in_fd, out_fd, err_fd;
};

typedef struct mysh_job_tag mysh_job;

static mysh_job* mysh_new_job() {
    mysh_job* job = (mysh_job*)malloc(sizeof(mysh_job));
    if (job == NULL) {
        fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
    }

    job->next = NULL;
    ms_init(&job->command, "");
    job->first_proc = NULL;
    job->group_id = 0;
    job->is_notified = false;
    job->in_fd = -1;
    job->out_fd = -1;
    job->err_fd = -1;

    return job;
}

static void mysh_release_job(mysh_job* job) {
    mysh_release_process(job->first_proc);
    free(job);
}

static void mysh_fprint_job(FILE* file, mysh_job* job, const char* status) {
    fprintf(file, "%d (%s): %s\n", job->group_id, status, job->command.ptr);
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

static bool mysh_set_status(mysh_job* first_job, pid_t pid, int status) {
    assert(pid >= 0);

    if (pid == 0 || errno == ECHILD) {
        return false;
    }

    for (mysh_job* job = first_job; job != NULL; job = job->next) {
        for (mysh_process* proc = job->first_proc; proc != NULL; proc = proc->next) {
            if (proc->pid == pid) {
                proc->status = status;

                if (WIFSTOPPED(status)) {
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

static void mysh_update_status(mysh_job* first_job) {
    int code;
    pid_t pid;
    do {
        pid = waitpid(WAIT_ANY, &code, WUNTRACED | WNOHANG);
    } while(pid >= 0 && mysh_set_status(first_job, pid, code));
}

static void mysh_wait_job(mysh_resource* shell, mysh_job* job) {
    int status;
    pid_t pid;
    do {
        pid = waitpid(WAIT_ANY, &status, WUNTRACED);
    } while (pid >= 0 && mysh_set_status(shell->first_job, pid, status) && !mysh_is_job_stopped(job) && !mysh_is_job_completed(job));
}

static void mysh_put_job_foreground(mysh_resource* shell, mysh_job* job, bool do_continue) {
    tcsetpgrp(shell->terminal_fd, job->group_id);

    if (do_continue) {
        tcsetattr(shell->terminal_fd, TCSADRAIN, &job->termios);

        if (kill(-job->group_id, SIGCONT) < 0) {
            perror("mysh: failed to send SIGCONT");
        }
    }

    mysh_wait_job(shell, job);

    tcsetpgrp(shell->terminal_fd, shell->group_id);

    tcgetattr(shell->terminal_fd, &job->termios);
    tcsetattr(shell->terminal_fd, TCSADRAIN, &shell->original_termios);
}

static bool mysh_launch_job(mysh_resource* shell, mysh_job* job, bool is_foreground) {
    assert(job != NULL);

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
            mysh_exec_process(shell, proc, job->group_id, in_fd, out_fd, job->err_fd, is_foreground);
        }
        else {
            // parent
            proc->pid = pid;
            if (shell->is_interactive) {
                if (job->group_id == 0) {
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

    if (!shell->is_interactive) {
        mysh_wait_job(shell, job);
    } else if (is_foreground) {
        mysh_put_job_foreground(shell, job, false);
    }

    return true;
}

static bool mysh_resume_job(mysh_resource* shell, mysh_job* job, bool is_foreground) {
    if (mysh_is_job_completed(job) || !mysh_is_job_stopped(job)) {
        return true;
    }
    for (mysh_process* proc = job->first_proc; proc != NULL; proc = proc->next) {
        proc->is_stopped = false;
    }

    job->is_notified = false;

    if (is_foreground) {
        mysh_put_job_foreground(shell, job, true);
    }
    else if (kill(- job->group_id, SIGCONT) < 0) {
        perror("mysh: failed to send SIGCONT: ");
        return false;
    }

    return true;
}

#endif // MYSH_JOB_H