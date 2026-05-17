#pragma once

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

// типы всех сложных объектов, живущих в куче
enum ObjType {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE
};

// базовый объект для сборщика мусора
struct Obj {
    ObjType type;
    bool isMarked;
    Obj *next; // указатель на следующий объект для цепочки GC
};

struct ObjFunction {
    Obj obj; // базовый объект всегда первый!
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString *name;
};

typedef Value (*NativeFn)(int argCount, Value *args);

struct ObjNative {
    Obj obj;
    NativeFn function;
};

struct ObjString {
    Obj obj;
    int length;
    char *chars;
    uint32_t hash;
};

struct ObjUpvalue {
    Obj obj;
    Value *location;
    Value closed;
    ObjUpvalue *next;
};

struct ObjClosure {
    Obj obj;
    ObjFunction *function;
    ObjUpvalue **upvalues;
    int upvalueCount;
};

struct ObjClass {
    Obj obj;
    ObjString *name;
    Table methods;
};

struct ObjInstance {
    Obj obj;
    ObjClass *klass;
    Table fields;
};

struct ObjBoundMethod {
    Obj obj;
    Value receiver;
    ObjClosure *method;
};

// получение и проверка типа
inline ObjType OBJ_TYPE(Value value) { return AS_OBJ(value)->type; }
inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

// проверки конкретных объектов
inline bool IS_BOUND_METHOD(Value value) {
    return isObjType(value, OBJ_BOUND_METHOD);
}
inline bool IS_CLASS(Value value) { return isObjType(value, OBJ_CLASS); }
inline bool IS_CLOSURE(Value value) { return isObjType(value, OBJ_CLOSURE); }
inline bool IS_FUNCTION(Value value) { return isObjType(value, OBJ_FUNCTION); }
inline bool IS_INSTANCE(Value value) { return isObjType(value, OBJ_INSTANCE); }
inline bool IS_NATIVE(Value value) { return isObjType(value, OBJ_NATIVE); }
inline bool IS_STRING(Value value) { return isObjType(value, OBJ_STRING); }

// безопасное извлечение объектов (reinterpret_cast работает молниеносно)
inline ObjBoundMethod *AS_BOUND_METHOD(Value value) {
    return reinterpret_cast<ObjBoundMethod *>(AS_OBJ(value));
}
inline ObjClass *AS_CLASS(Value value) {
    return reinterpret_cast<ObjClass *>(AS_OBJ(value));
}
inline ObjClosure *AS_CLOSURE(Value value) {
    return reinterpret_cast<ObjClosure *>(AS_OBJ(value));
}
inline ObjFunction *AS_FUNCTION(Value value) {
    return reinterpret_cast<ObjFunction *>(AS_OBJ(value));
}
inline ObjInstance *AS_INSTANCE(Value value) {
    return reinterpret_cast<ObjInstance *>(AS_OBJ(value));
}
inline NativeFn AS_NATIVE(Value value) {
    return reinterpret_cast<ObjNative *>(AS_OBJ(value))->function;
}
inline ObjString *AS_STRING(Value value) {
    return reinterpret_cast<ObjString *>(AS_OBJ(value));
}
inline char *AS_CSTRING(Value value) { return AS_STRING(value)->chars; }

// ФУНКЦИИ СОЗДАНИЯ ОБЪЕКТОВ

ObjBoundMethod *newBoundMethod(Value receiver, ObjClosure *method);
ObjClass *newClass(ObjString *name);
ObjClosure *newClosure(ObjFunction *function);
ObjFunction *newFunction();
ObjInstance *newInstance(ObjClass *klass);
ObjNative *newNative(NativeFn function);

ObjString *takeString(char *chars, int length);
ObjString *copyString(const char *chars, int length);

ObjUpvalue *newUpvalue(Value *slot);
void printObject(Value value);