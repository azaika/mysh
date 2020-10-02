#ifndef MYSH_MYSTRING_H
#define MYSH_MYSTRING_H

#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct {
    char* ptr;
    size_t length;
    size_t capacity;
} mysh_string;

static void ms_init(mysh_string* s, const char* src) {
    if (s == NULL || src == NULL)
        return;

    size_t size = strlen(src) + 1;

    size_t cap = 8;
    while (cap < size) {
        cap *= 2;
    }
    
    s->ptr = (char*)malloc(cap * sizeof(char));
    s->length = size;
    s->capacity = cap;

    strcpy(s->ptr, src);
}

static void ms_assign(mysh_string* s1, const mysh_string* s2) {
    if (s1->ptr == NULL) {
        ms_init(s1, s2->ptr);
        return;
    }
    
    while (s1->capacity < s2->length) {
        s1->capacity *= 2;
    }

    s1->ptr = realloc(s1->ptr, s1->capacity * sizeof(char));
    s1->length = s2->length;

    strcpy(s1->ptr, s2->ptr);
}

static size_t ms_reserve(mysh_string* s, size_t new_cap) {
    assert(s != NULL);
    
    while (s->capacity < new_cap) {
        s->capacity *= 2;
    }

    if (s->ptr == NULL) {
        s->ptr = malloc(s->capacity * sizeof(char));
    }
    else {
        s->ptr = realloc(s->ptr, s->capacity * sizeof(char));
    }

    return s->capacity;
}

static void ms_shrink(mysh_string* s) {
    assert(s != NULL);

    while (s->length * 2 < s->capacity) {
        s->capacity /= 2;
    }

    s->ptr = realloc(s->ptr, s->capacity * sizeof(char));
}

static void ms_clear(mysh_string* s) {
    assert(s != NULL);

    s->ptr[0] = '\0';
    s->length = 0;
}

static void ms_relase(mysh_string* s) {
    assert(s != NULL);

    free(s->ptr);
    s->ptr = NULL;
    s->length = 0;
    s->capacity = 0;
}

#endif // MYSH_MYSTRING_H