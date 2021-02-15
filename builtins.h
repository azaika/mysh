#ifndef MYSH_BUILTINS_H
#define MYSH_BUILTINS_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "shell_resource.h"
#include "job.h"

static const char* builtin_str[] = {
    "cd",
    "exit",
	"jobs",
    "fg",
    "bg"
};

static int mysh_cd(mysh_resource* shell, char** argv);
static int mysh_exit(mysh_resource* shell, char** argv);
static int mysh_jobs(mysh_resource* shell, char** argv);
static int mysh_fg(mysh_resource* shell, char** argv);
static int mysh_bg(mysh_resource* shell, char** argv);

static int (*const builtin_func[]) (mysh_resource*, char**) = {
    mysh_cd,
    mysh_exit,
	mysh_jobs,
    mysh_fg,
    mysh_bg
};

static int mysh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char*);
}

int mysh_cd(mysh_resource* shell, char** argv) {
	if (argv[1] == NULL) {
		return 0;
	}

	int err = chdir(argv[1]);
	if (err) {
		perror("mysh");
	}
	else {
        mysh_set_curdir_name(shell, get_current_dir_name());
	}

    return 0;
}

int mysh_exit(mysh_resource* shell, char** argv) {
    return 1;
}

int mysh_jobs(mysh_resource* shell, char** argv) {
	if (shell->first_job == NULL) {
        return 0;
    }

    mysh_update_status(shell->first_job);
    
    mysh_job* cur_job = shell->first_job;
    mysh_job* prev_job = NULL;
    int idx = 1;
    while (cur_job != NULL) {
        mysh_job* next_job = cur_job->next;

        if (mysh_is_job_completed(cur_job)) {
            if (!cur_job->is_notified) {
                mysh_fprint_job(stdout, cur_job, "completed", idx);
            }
            else {
                --idx;
            }

            if (prev_job == NULL) {
                shell->first_job = next_job;
            }
            else {
                prev_job->next = next_job;
            }

            mysh_release_job(cur_job);
        }
        else if (mysh_is_job_stopped(cur_job)) {
            mysh_fprint_job(stdout, cur_job, "stopped", idx);
            prev_job = cur_job;
        }
        else {
            mysh_fprint_job(stdout, cur_job, "running", idx);
            prev_job = cur_job;
        }

        ++idx;
        cur_job = next_job;
    }

	return 0;
}

int mysh_fg(mysh_resource* shell, char** argv) {
    int idx = 0;
    if (argv[1] != NULL) {
        int x = atoi(argv[1]);
        if (x <= 0) {
            printf("mysh: fg: no such job\n");
            return 0;
        }

        idx = x - 1;
    }

    mysh_job* job = shell->first_job;
    for (int i = 0; i < idx && job != NULL; ++i) {
        job = job->next;
    }

    if (job == NULL) {
        printf("mysh: fg: no such job\n");
        return 0;
    }

    if (mysh_is_job_completed(job)) {
        printf("mysh: fg: job has completed");
    }
    else {
        mysh_resume_job(shell, job, true);
    }

	return 0;
}

int mysh_bg(mysh_resource* shell, char** argv) {
    int idx = 0;
    if (argv[1] != NULL) {
        int x = atoi(argv[1]);
        if (x <= 0) {
            printf("mysh: bg: no such job\n");
            return 0;
        }

        idx = x - 1;
    }

    mysh_job* job = shell->first_job;
    for (int i = 0; i < idx && job != NULL; ++i) {
        job = job->next;
    }

    if (job == NULL) {
        printf("mysh: bg: no such job\n");
        return 0;
    }

    if (mysh_is_job_completed(job)) {
        printf("mysh: bg: job has completed\n");
    }
    else {
        mysh_resume_job(shell, job, false);
    }

	return 0;
}

#endif // MYSH_BUILTINS_H