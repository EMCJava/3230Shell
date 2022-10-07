//
// Created by lys on 10/6/22.
//

#ifndef ARRAY_3230
#define ARRAY_3230

#include <stdlib.h>

struct Array {
    int size, len;
    void **data;
};

struct Array NewArray() {
    struct Array result;

    result.size = result.len = 0;
    result.data = NULL;

    return result;
}

void FreeArrayElements(struct Array *array) {
    for (int i = 0; i < array->len; ++i) {
        free(array->data[i]);
        array->data[i] = NULL;
    }

    array->len = 0;
}

int CountNull(struct Array array) {
    int count = 0;
    for (int i = 0; i < array.len; ++i) {
        if (array.data[i] == NULL)
            count++;
    }

    return count;
}

void FreeArray(struct Array *array) {
    free(array->data);
    array->data = NULL;
}

int Len(struct Array array) {
    return array.len;
}

void **At(struct Array array, int index) {
    return &array.data[index];
}

void **Find(struct Array array, int(*pred)(void *, void *), void *context) {
    for (int i = 0; i < array.len; ++i) {
        if (pred(array.data[i], context))
            return &array.data[i];
    }

    return NULL;
}

void PushBack(struct Array *array, void *data) {

    // reallocate
    if (array->len + 1 > array->size) {
        if (array->size == 0) {
            array->size = 1;
            array->data = malloc(array->size * sizeof(void *));
        } else {
            array->size *= 2;
            array->data = realloc(array->data, array->size * sizeof(void *));
        }
    }

    array->data[array->len++] = data;
}

#endif