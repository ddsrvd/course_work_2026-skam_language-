
#include "object.h"
#include "memory.h"
#include "vm.h"
#include <iostream>
#include <cstring>


static Obj* allocateObject(size_t size, ObjType type);

static ObjString* allocateString(char* chars, int length);
// принимает готовую строку из heap
ObjString* takeString(char* chars, int length) {
    return allocateString(chars, length);
}

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

ObjString* copyString(const char* chars, int length) {
    char* heapChars = ALLOCATE(char, length + 1);

    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(heapChars, length);
}

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(nullptr, 0, size);

    object->type = type;

    object->next = vm.objects;
    vm.objects = object;

    return object;
}

static ObjString* allocateString(char* chars, int length) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);

    string->length = length;
    string->chars = chars;

    return string;
}

void printObject(Value value) {

    switch (OBJ_TYPE(value)) {

        case OBJ_STRING: {
            ObjString* string = AS_STRING(value);

            std::cout.write(
                string->chars,
                string->length
            );

            break;
        }
    }
}
