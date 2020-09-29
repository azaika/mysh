#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int mysh_init(void) {
	return 0;
}

// caller must free() return value
char** mysh_split_line(char* line) {
    const int num_args_unit = 32;
    const char* delims = " \t\r\n\a";

	int buf_size = num_args_unit;
	int pos = 0;

	char** args = malloc(buf_size * sizeof(char*));
	if (!args) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
		exit(EXIT_FAILURE);
	}

	char* arg = strtok(line, delims);
	while (arg != NULL) {
		args[pos] = arg;
		++pos;

		if (pos >= buf_size) {
			buf_size += num_args_unit;
			args = realloc(args, buf_size * sizeof(char*));
			if (!args) {
				fprintf(stderr, "mysh: error occurred in allocation.\n");
				exit(EXIT_FAILURE);
			}
		}

        arg = strtok(NULL, delims);
	}

    args[pos] = NULL;

    return args;
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

const char* builtin_str[] = {
    "cd",
    "exit"
};

int mysh_cd(char** args) {
    puts("cd()");
    return 0;
}
int mysh_exit(char** args) {
    puts("exit()");
    return 0;
}

int (*const builtin_func[]) (char**) = {
    mysh_cd,
    mysh_exit
};

int mysh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char*);
}

#define MYSH_MAX_INPUT_BYTES (8096)

int mysh_loop(void) {
	char* input_buf = malloc(sizeof(char) * MYSH_MAX_INPUT_BYTES);
	if (input_buf == NULL) {
		fprintf(stderr, "mysh: error occurred in allocation.\n");
		exit(EXIT_FAILURE);
	}

	int status = 1;
	do {
		printf("> ");
		int read_err = mysh_read_line(input_buf, MYSH_MAX_INPUT_BYTES);
		if (read_err) {
			fprintf(stderr, "mysh: error occurred while reading input (input ignored).\n");
			continue;
		}

        char** args = mysh_split_line(input_buf);

        for (int i = 0; args[i] != NULL; ++i) {
            puts(args[i]);
        }

        free(args);
	} while(status);

	free(input_buf);
	return status;
}

int mysh_terminate(void) {
	return 0;
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