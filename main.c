// standard library
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// unix header
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "parser.h"
#include "shell_resource.h"
#include "builtins.h"

int mysh_init(void) {
	return mysh_init_resource();
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

int mysh_launch(char** args) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("mysh");
    }
    else if (pid == 0) { // child
        if (execvp(args[0], args) == -1) {
            perror("mysh");
        }

        exit(EXIT_FAILURE);
    }
    else {
        // parent

        int status;
        do {
            waitpid(pid, &status, WUNTRACED);
        } while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 0;
}

int mysh_execute(char** args) {
    if (args == NULL || args[0] == NULL) {
        return 0;
    }

    for (int i = 0; i < mysh_num_builtins(); ++i) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return mysh_launch(args);
}

#define MYSH_MAX_INPUT_BYTES (8096)

int mysh_loop(void) {
	char* input_buf = malloc(sizeof(char) * MYSH_MAX_INPUT_BYTES);
	if (input_buf == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
		exit(EXIT_FAILURE);
	}

	int status = 0;
	do {
		printf("%s$ ", *mysh_shell_dir());
		int read_err = mysh_read_line(input_buf, MYSH_MAX_INPUT_BYTES);
		if (read_err) {
			fprintf(stderr, "mysh: error occurred while reading input (input ignored).\n");
			continue;
		}

        char** args = mysh_split_line(input_buf);

        status = mysh_execute(args);

        free(args);
	} while(status == 0);

	free(input_buf);
	return status;
}

int mysh_terminate(void) {
	return mysh_release_resource();
}

int main(void) {
	int init_err = mysh_init();
	if (init_err) {
		fprintf(stderr, "mysh: error occurred in initialization process.\n");
		return EXIT_FAILURE;
	}

	int loop_err = mysh_loop();
	if (loop_err) {
		fprintf(stderr, "mysh: error occurred in loop process.\n");
		return EXIT_FAILURE;
	}

	int terminate_err = mysh_terminate();
	if (terminate_err) {
		fprintf(stderr, "mysh: error occurred in loop process.\n");
		return EXIT_FAILURE;
	}

	printf("mysh: byebye 👋\n");

	return EXIT_SUCCESS;
}