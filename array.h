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

void ApplyToArrayElements(struct Array *array, void(*operator)(void *)) {
    for (int i = 0; i < array->len; ++i)
        operator(array->data[i]);
}

void FreeCustomArrayElements(struct Array *array, void(*deallocator)(void *)) {
//    for (int i = 0; i < array->len; ++i) {
//        deallocator(array->data[i]);
//        array->data[i] = NULL;
//    }

    ApplyToArrayElements(array, deallocator);
    array->len = 0;
}

void FreeArrayElements(struct Array *array) {
    for (int i = 0; i < array->len; ++i) {
        free(array->data[i]);
        array->data[i] = NULL;
    }

    array->len = 0;
}

void ShallowCopy(struct Array *to, struct Array from) {
    to->data = from.data;
    to->len = from.len;
    to->size = from.size;
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
    *array = NewArray();
}

int Len(struct Array array) {
    return array.len;
}

void **At(struct Array array, int index) {
    return &array.data[index];
}

void **AtLenRel(struct Array array, int len_rel) {
    return &array.data[array.len + len_rel];
}

int All(struct Array array, int(*pred)(void *, void *), void *context) {
    int true = 1;
    for (int i = 0; i < array.len && true; ++i)
        true &= pred(array.data[i], context);
    return true;
}

void RemoveAt(struct Array *array, int index) {
    for (int j = index; j < array->len - 1; ++j) array->data[j] = array->data[j + 1];
    --array->len;
}

/* Return removed index */
int Remove(struct Array *array, void *data) {
    for (int i = 0; i < array->len; ++i) {
        if (array->data[i] == data) {
            RemoveAt(array, i);
            return i;
        }
    }

    return -1;
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