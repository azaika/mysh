// standard library
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// unix header
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <stdbool.h>

#include "job.h"
#include "parser.h"
#include "shell_resource.h"
#include "builtins.h"

bool mysh_init(mysh_resource* res) {
    ms_init(&res->home_dir, getenv("HOME"));
	mysh_set_curdir_name(res, get_current_dir_name());

    if (errno < 0) {
        perror("mysh: couldn't get $HOME");
        return false;
    }
    
	res->first_job = NULL;
    res->terminal_fd = STDIN_FILENO;
    res->is_interactive = isatty(res->terminal_fd);

    if (res->is_interactive) {
        for (pid_t p = getpgrp(); p != tcgetpgrp(res->terminal_fd); p = getpgrp()) {
            kill(-res->group_id, SIGTTIN);
        }

		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTTOU, SIG_IGN);
		signal(SIGCHLD, SIG_IGN);

        res->group_id = getpid();
        if (setpgid(res->group_id, res->group_id) < 0) {
            perror("mysh: couldn't set the shell in its own process group");
            return false;
        }

        if (tcsetpgrp(res->terminal_fd, res->group_id) < -1) {
            perror("mysh: couldn't make the shell foreground");
            return false;
        }
        if (tcgetattr(res->terminal_fd, &res->original_termios) < 0) {
            perror("mysh: failed to get original termios");
			return false;
        }

		return true;
    }

	return false;
}

static void mysh_update(mysh_job* first_job) {
    int code;
    pid_t pid;
    do {
        pid = waitpid(WAIT_ANY, &code, WUNTRACED | WNOHANG);
    } while(!mysh_set_status(first_job, pid, code));
}

int is_terminal_char(char c) {
	return c == '\n' || c == EOF;
}

int mysh_read_line(char* buf, size_t buf_size) {
	size_t cur = 0;
	while (1) {
		char c = getchar();

		if (cur + 1 == buf_size && !is_terminal_char(c)) {
			fprintf(stderr, "mysh: input must be less than 8096 bytes.");
			while (!is_terminal_char(c)) {
				c = getchar();
			}
			
			return 1;
		}

		if (is_terminal_char(c)) {
			buf[cur] = '\0';
			break;
		}
		
		buf[cur] = c;
		++cur;
	}
	
	return 0;
}

#define MYSH_MAX_INPUT_BYTES (8096)

int mysh_loop(mysh_resource* res) {
	char* input_buf = malloc(sizeof(char) * MYSH_MAX_INPUT_BYTES);
	if (input_buf == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
		exit(EXIT_FAILURE);
	}

	int status = 0;
	do {
		printf("%s$ ", res->current_dir.ptr);
		int read_err = mysh_read_line(input_buf, MYSH_MAX_INPUT_BYTES);
		if (read_err) {
			fprintf(stderr, "mysh: error occurred while reading input (input ignored).\n");
			continue;
		}

		if (input_buf[0] == '\0') {
			exit(EXIT_SUCCESS);
		}

		mysh_process* proc = mysh_new_process();
		bool is_foreground;
		if (!mysh_parse_input(input_buf, proc, &is_foreground)) {
			fprintf(stderr, "mysh: parse error (input ignored).\n");
			continue;
		}

		bool is_builtin = false;
		for (int i = 0; i < mysh_num_builtins(); ++i) {
			assert(proc->argv != NULL);
			if (strcmp(proc->argv[0], builtin_str[i]) == 0) {
				if (proc->next != NULL) {
					fprintf(stderr, "mysh: builtin functions cannot call with other command\n");
				}
				else if ((builtin_func[i])(res, proc->argv) != 0) {
					exit(EXIT_SUCCESS);
				}

				mysh_release_process(proc);
				is_builtin = true;
				i = mysh_num_builtins();
			}
		}

		if (is_builtin) {
			continue;
		}
		
		mysh_job* job = res->first_job;
		if (res->first_job == NULL) {
			res->first_job = mysh_new_job();
			job = res->first_job;
		}
		else {
			while (job->next != NULL) {
				job = job->next;
			}

			job->next = mysh_new_job();
			job = job->next;
		}

		job->first_proc = proc;
		job->in_fd = STDIN_FILENO;
		job->out_fd = STDOUT_FILENO;
		job->err_fd = STDERR_FILENO;
		job->group_id = res->group_id;
		job->tmodes = res->original_termios;
		
		ms_assign_raw(&job->command, input_buf);

		mysh_launch_job(res, job, is_foreground);
	} while(status == 0);

	free(input_buf);
	return 0;
}

bool mysh_terminate(mysh_resource* res) {
	mysh_release_resource(res);
	return true;
}

int main(void) {
	mysh_resource res;
	if (!mysh_init(&res)) {
		fprintf(stderr, "mysh: error occurred in initialization process.\n");
		return EXIT_FAILURE;
	}
	
	int loop_err = mysh_loop(&res);
	if (loop_err) {
		fprintf(stderr, "mysh: error occurred in loop process.\n");
		return EXIT_FAILURE;
	}

	if (!mysh_terminate(&res)) {
		fprintf(stderr, "mysh: error occurred in loop process.\n");
		return EXIT_FAILURE;
	}

	printf("mysh: byebye 👋\n");

	return EXIT_SUCCESS;
}