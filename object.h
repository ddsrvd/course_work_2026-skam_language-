#ifndef SKAM_OBJECT_H
#define SKAM_OBJECT_H

#include "common.h"
#include "chunk.h"
#include "value.h"

// =========================
// Типы объектов VM
// =========================

enum ObjType {
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
};

// =========================
// Базовый объект
// =========================

struct Obj {
    ObjType type;
    Obj* next = nullptr; // linked list для GC
};

struct ObjFunction {
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString* name;
};

using NativeFn = Value (*)(int argCount, Value* args);

struct ObjNative {
    Obj obj;
    NativeFn function;
};

// =========================
// Строковый объект
// =========================

struct ObjString {
    Obj obj;

    int length;
    char* chars;
    uint32_t hash;
};

ObjFunction* newFunction();
ObjNative* newNative(NativeFn function);

// =========================
// Макросы для работы с объектами
// =========================

#ifndef AS_OBJ
#error AS_OBJ_NOT_DEFINED
#endif

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

// Проверка типа объекта
inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) &&
           AS_OBJ(value)->type == type;
}

#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
// Проверка строки
#define IS_STRING(value) isObjType(value, OBJ_STRING)

// Приведение типов
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars) 

// =========================
// Функции строк
// =========================

// Копирует строку в heap
ObjString* copyString(const char* chars, int length);

// Забирает готовый heap buffer
ObjString* takeString(char* chars, int length);

// Печать объекта
void printObject(Value value);

#endif