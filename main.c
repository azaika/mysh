#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int mysh_init(void) {
    return 0;
}

int mysh_read_line(char* buf, size_t buf_size) {
    buf[0] = getchar();
    buf[1] = 0;
    return 0;
}

#define MAX_INPUT_BYTES (8096)

int mysh_loop(void) {
    char* input_buf = malloc(sizeof(char) * MAX_INPUT_BYTES);
    if (input_buf == NULL) {
        fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
    }

    int status = 1;
    do {
        printf("> ");
        int read_err = mysh_reasd_line(input_buf, MAX_INPUT_BYTES);
        if (read_err) {
            fprintf(stderr, "mysh: error occurred while reading input.\n");
            status = 1;
            break;
        }
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