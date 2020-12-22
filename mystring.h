#ifndef MYSH_MYSTRING_H
#define MYSH_MYSTRING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct mysh_string_tag {
    char* ptr;
    size_t length;
    size_t capacity;
} mysh_string;

static size_t ms_reserve(mysh_string* s, size_t len) {
    assert(s != NULL);
    
    while (s->capacity < len + 1) {
        s->capacity *= 2;
    }

    if (s->ptr == NULL) {
        s->ptr = malloc(s->capacity * sizeof(char));
    }
    else {
        s->ptr = realloc(s->ptr, s->capacity * sizeof(char));
    }

    if (s->ptr == NULL) {
        fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
    }

    return s->capacity;
}

static void ms_init(mysh_string* str, const char* src) {
    if (str == NULL || src == NULL || str->ptr != NULL)
        return;

    size_t len = strlen(src);

    size_t cap = 8;
    while (cap < len + 1) {
        cap *= 2;
    }
    
    str->ptr = (char*)malloc(cap * sizeof(char));
    if (str->ptr == NULL) {
        fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
    }

    str->length = len;
    str->capacity = cap;

    strcpy(str->ptr, src);
}

static mysh_string* ms_new() {
    mysh_string* s = (mysh_string*)malloc(sizeof(mysh_string));
    if (s == NULL) {
        fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
    }

    s->ptr = NULL;
    s->length = 0;
    s->capacity = 0;

    return s;
}

static void ms_assign(mysh_string* s1, const mysh_string* s2) {
    assert(s1 != NULL);

    if (s1->ptr == NULL) {
        ms_init(s1, s2->ptr);
        return;
    }

    ms_reserve(s1, s2->length);
    s1->length = s2->length;

    strcpy(s1->ptr, s2->ptr);
}

static void ms_assign_raw(mysh_string* str, const char* src) {
    assert(str != NULL);

    if (str->ptr == NULL) {
        ms_init(str, src);
        return;
    }

    size_t len = strlen(src);
    ms_reserve(str, len);
    str->length = len;

    strcpy(str->ptr, src);
}

static void ms_shrink(mysh_string* s) {
    assert(s != NULL);

    while (s->length * 2 < s->capacity) {
        s->capacity /= 2;
    }

    s->ptr = realloc(s->ptr, s->capacity * sizeof(char));

    if (s->ptr == NULL) {
        fprintf(stderr, "mysh: error occurred in allocation.\n");
        exit(EXIT_FAILURE);
    }
}

static void ms_clear(mysh_string* s) {
    assert(s != NULL);

    if (s->ptr == NULL) {
        ms_init(s, "");
    }
    else {
        s->ptr[0] = '\0';
        s->length = 0;
    }
}

static void ms_relase(mysh_string* s) {
    assert(s != NULL);

    if (s->ptr != NULL) {
        free(s->ptr);
    }

    s->ptr = NULL;
    s->length = 0;
    s->capacity = 0;
}

static void ms_free(mysh_string* s) {
    ms_relase(s);
    free(s);
}

static void ms_push(mysh_string* s, char c) {
    assert(s != NULL);

    if (s->ptr == NULL) {
        ms_init(s, "");
    }

    ms_reserve(s, s->length + 1);

    s->ptr[s->length] = c;
    s->ptr[s->length + 1] = '\0';

    ++s->length;
}

static void ms_append(mysh_string* s1, const mysh_string* s2) {
    assert(s1 != NULL && s2 != NULL);

    if (s2->ptr == NULL) {
        return;
    }
    if (s1->ptr == NULL) {
        ms_init(s1, s2->ptr);
        return;
    }

    ms_reserve(s1, s1->length + s2->length);

    strcpy(s1->ptr + s1->length, s2->ptr);

    s1->length += s2->length;
}

static void ms_append_raw(mysh_string* str, const char* src) {
    assert(str != NULL && src != NULL);

    if (str->ptr == NULL) {
        ms_init(str, src);
        return;
    }

    size_t len = strlen(src);
    ms_reserve(str, str->length + len);

    strcpy(str->ptr + str->length, src);

    str->length += len;
}

static char* ms_into_chars(mysh_string* str) {
    assert(str != NULL);

    if (str->ptr == NULL) {
        return NULL;
    }

    char* ptr = str->ptr;
    free(str);

    return ptr;
}

#endif // MYSH_MYSTRING_H