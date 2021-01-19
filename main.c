// standard library
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// unix header
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <stdbool.h>

#include "parser.h"
#include "shell_resource.h"
#include "builtins.h"

bool mysh_init(mysh_resource* res) {
	return mysh_init_resource(res);
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
    else { // parent
        int status;
        do {
            waitpid(pid, &status, WUNTRACED);
        } while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 0;
}

int mysh_execute(mysh_resource* res, char** args) {
    if (args == NULL || args[0] == NULL) {
        return 0;
    }

    for (int i = 0; i < mysh_num_builtins(); ++i) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(res, args);
        }
    }

    return mysh_launch(args);
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
		printf("%s$ ", res->current_dir);
		int read_err = mysh_read_line(input_buf, MYSH_MAX_INPUT_BYTES);
		if (read_err) {
			fprintf(stderr, "mysh: error occurred while reading input (input ignored).\n");
			continue;
		}

		mysh_tokenized_component** coms = mysh_parse_input(input_buf);
		if (coms == NULL) {
			fprintf(stderr, "mysh: parse error (input ignored).\n");
		}
		for (int i = 0; coms[i] != NULL; ++i) {
			if (coms[i]->token == token_string) {
				printf("token_string: %s\n", ((mysh_string*)coms[i]->data)->ptr);
			}
			else if (coms[i]->token == token_jobcontrol) {
				printf("token_jobcontrol: %s\n", ((mysh_string*)coms[i]->data)->ptr);
			}
			else if (coms[i]->token == token_pipe) {
				printf("token_pipe: |\n");
			}
			else {
				printf("token_redirect: %s\n", ((mysh_redirect_data*)coms[i]->data)->str->ptr);
			}
			free_tokenized_component(coms[i]);
		}
		free(coms);
		continue;

        char** args = mysh_split_line(input_buf);

        status = mysh_execute(args);

        free(args);
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