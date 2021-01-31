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
	"jobs"
};

static int mysh_cd(mysh_resource* res, char** argv);
static int mysh_exit(mysh_resource* res, char** argv);
static int mysh_jobs(mysh_resource* res, char** argv);

static int (*const builtin_func[]) (mysh_resource*, char**) = {
    mysh_cd,
    mysh_exit,
	mysh_jobs
};

static int mysh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char*);
}

int mysh_cd(mysh_resource* res, char** argv) {
	if (argv[1] == NULL) {
		return 0;
	}

	int err = chdir(argv[1]);
	if (err) {
		perror("mysh");
	}
	else {
        mysh_set_curdir_name(res, get_current_dir_name());
	}

    return 0;
}

int mysh_exit(mysh_resource* res, char** argv) {
    return 1;
}

int mysh_jobs(mysh_resource* res, char** argv) {
	if (res->first_job == NULL) {
        return 0;
    }
    
    mysh_job* cur_job = res->first_job;
    bool is_first = true;
    while (cur_job != NULL) {
        mysh_job* next_job = cur_job->next;
        if (mysh_is_job_completed(cur_job)) {
            mysh_fprint_job(stdout, cur_job, "completed");
            if (is_first) {
                res->first_job = next_job;
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

	return 0;
}

#endif // MYSH_BUILTINS_H