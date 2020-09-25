#include <stdlib.h>
#include <stdio.h>

int mysh_init(void) {
    return 0;
}

int mysh_loop(void) {
    return 0;   
}

int mysh_terminate(void) {
    return 0;   
}

int main(void) {
    int init_err = mysh_init();
    if (init_err) {
        printf("mysh: error occured in initialization process.\n");
        return EXIT_FAILURE;
    }

    int loop_err = mysh_loop();
    if (loop_err) {
        printf("mysh: error occured in loop process.\n");
        return EXIT_FAILURE;
    }

    int terminate_err = mysh_terminate();
    if (terminate_err) {
        printf("mysh: error occured in loop process.\n");
        return EXIT_FAILURE;
    }

    printf("mysh: byebye 👋\n");

    return EXIT_SUCCESS;
}