#ifndef SKAM_VALUE_H
#define SKAM_VALUE_H

#include "common.h"

struct Obj;

// =========================
// Типы значений
// =========================

enum ValueType {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ
};

// =========================
// Value
// =========================

struct Value {
    ValueType type;

    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
};

// =========================
// Проверка типов
// =========================

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

// =========================
// Извлечение значений
// =========================

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)
#define AS_OBJ(value)     ((value).as.obj)

// =========================
// Создание значений
// =========================

inline Value boolValue(bool value) {
    Value result;
    result.type = VAL_BOOL;
    result.as.boolean = value;
    return result;
}

inline Value nilValue() {
    Value result;
    result.type = VAL_NIL;
    result.as.number = 0;
    return result;
}

inline Value numberValue(double value) {
    Value result;
    result.type = VAL_NUMBER;
    result.as.number = value;
    return result;
}

inline Value objValue(Obj* object) {
    Value result;
    result.type = VAL_OBJ;
    result.as.obj = object;
    return result;
}

// =========================
// Макросы совместимости
// =========================

#define BOOL_VAL(value)   boolValue(value)
#define NIL_VAL           nilValue()
#define NUMBER_VAL(value) numberValue(value)
#define OBJ_VAL(object)   objValue((Obj*)object)

// =========================
// ValueArray
// =========================

struct ValueArray {
    int capacity = 0;
    int count = 0;
    Value* values = nullptr;
};

// =========================
// Функции
// =========================

bool valuesEqual(Value a, Value b);

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

void printValue(Value value);

#endif