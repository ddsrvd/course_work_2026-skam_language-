#include <cstdlib>

#include "memory.h"
#include "vm.h"


void* reallocate(void* pointer, size_t oldSize, size_t newSize){
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }
    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}
// освобождение heap-объекта
static void freeObject(Obj* object) {
    switch (object->type) {
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }

        case OBJ_NATIVE:{
            FREE(ObjNative, object);
            break;
        }

        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;

            // освобождаем массив символов
            FREE_ARRAY(char, string->chars, string->length + 1);

            // освобождаем сам объект
            FREE(ObjString, object);

            break;
        }
    }
}
// освобождение всех объектов VM
void freeObjects() {
    Obj* object = vm.objects;

    while (object != nullptr) {
        Obj* next = object->next;

        freeObject(object);

        object = next;
    }
}