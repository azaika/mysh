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

bool mysh_init(mysh_resource* shell) {
    ms_init(&shell->home_dir, getenv("HOME"));
	mysh_set_curdir_name(shell);

    if (errno < 0) {
        perror("mysh: couldn't get $HOME");
        return false;
    }
    
	shell->first_job = NULL;
    shell->terminal_fd = STDIN_FILENO;
    shell->is_interactive = isatty(shell->terminal_fd);

    if (shell->is_interactive) {
		while(true) {
			shell->group_id = getpgrp();
			if (tcgetpgrp(shell->terminal_fd) == shell->group_id) {
				break;
			}

			kill(-shell->group_id, SIGTTIN);
		}

		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTTOU, SIG_IGN);

        shell->group_id = getpid();
        if (setpgid(shell->group_id, shell->group_id) < 0) {
            perror("mysh: couldn't set the shell in its own process group");
            exit(EXIT_FAILURE);
        }
        if (tcsetpgrp(shell->terminal_fd, shell->group_id) < 0) {
            perror("mysh: couldn't make the shell foreground");
            return false;
        }

        if (tcgetattr(shell->terminal_fd, &shell->original_termios) < 0) {
            perror("mysh: failed to get original termios");
			return false;
        }

		return true;
    }

	return false;
}

int is_terminal_char(char c) {
	return c == '\n' || c == EOF;
}

bool mysh_read_line(char* buf, size_t buf_size) {
	size_t cur = 0;
	while (1) {
		char c = getchar();

		if (cur + 1 == buf_size && !is_terminal_char(c)) {
			fprintf(stderr, "mysh: input must be less than 8096 bytes.");
			while (!is_terminal_char(c)) {
				c = getchar();
			}
			
			return false;
		}

		if (c == '\n') {
			buf[cur] = '\0';
			break;
		}

		if (c == EOF) {
			buf[cur] = EOF;
			break;
		}

		buf[cur] = c;
		++cur;
	}

	return true;
}

#define MYSH_MAX_INPUT_BYTES (8096)

int mysh_loop(mysh_resource* shell) {
	char* input_buf = malloc(sizeof(char) * MYSH_MAX_INPUT_BYTES);
	if (input_buf == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
		exit(EXIT_FAILURE);
	}

	int status = 0;
	do {
		printf("%s$ ", shell->current_dir.ptr);
		if (!mysh_read_line(input_buf, MYSH_MAX_INPUT_BYTES)) {
			fprintf(stderr, "mysh: error occurred while reading input (input ignored).\n");
			continue;
		}

		if (input_buf[0] == EOF) {
			exit(EXIT_SUCCESS);
		}

		mysh_process* proc = mysh_new_process();
		bool is_foreground;
		if (!mysh_parse_input(input_buf, proc, &is_foreground)) {
			continue;
		}

		bool is_builtin = false;
		for (int i = 0; i < mysh_num_builtins(); ++i) {
			assert(proc->argv != NULL);
			if (strcmp(proc->argv[0], builtin_str[i]) == 0) {
				if (proc->next != NULL) {
					fprintf(stderr, "mysh: builtin functions cannot call with other command\n");
				}
				else if ((builtin_func[i])(shell, proc->argv) != 0) {
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
		
		mysh_job* job = shell->first_job;
		if (shell->first_job == NULL) {
			shell->first_job = mysh_new_job();
			job = shell->first_job;
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
		job->group_id = 0;
		job->termios = shell->original_termios;
		
		ms_assign_raw(&job->command, input_buf);

		mysh_launch_job(shell, job, is_foreground);
	} while(status == 0);

	free(input_buf);
	return 0;
}

bool mysh_terminate(mysh_resource* shell) {
	for (mysh_job* job = shell->first_job; job != NULL; job = job->next) {
		kill(-job->group_id, SIGTERM);
	}

	mysh_release_resource(shell);
	return true;
}

int main(void) {
	mysh_resource shell;
	memset(&shell, 0, sizeof(shell));
	if (!mysh_init(&shell)) {
		fprintf(stderr, "mysh: error occurred in initialization process.\n");
		return EXIT_FAILURE;
	}
	
	int loop_err = mysh_loop(&shell);
	if (loop_err) {
		fprintf(stderr, "mysh: error occurred in loop process.\n");
		mysh_terminate(&shell);
		return EXIT_FAILURE;
	}

	if (!mysh_terminate(&shell)) {
		fprintf(stderr, "mysh: error occurred in loop process.\n");
		return EXIT_FAILURE;
	}

	printf("mysh: byebye 👋\n");

	return EXIT_SUCCESS;
}