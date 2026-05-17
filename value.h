#pragma once

#include "common.h"
#include <cstring>

struct Obj;

// Типы данных, которые поддерживает наша виртуальная машина
enum ValueType {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ // Любые сложные данные (строки, классы, функции), живущие в куче
            // (heap)
};

// Главная структура данных
struct Value {
    ValueType type; // "Бирка" типа
    union {
        bool boolean;
        double number;
        Obj *obj;
    } as; // Сами данные
};

// Проверка типа (IS_...)
inline bool IS_BOOL(Value value) { return value.type == VAL_BOOL; }
inline bool IS_NIL(Value value) { return value.type == VAL_NIL; }
inline bool IS_NUMBER(Value value) { return value.type == VAL_NUMBER; }
inline bool IS_OBJ(Value value) { return value.type == VAL_OBJ; }

// Извлечение данных (AS_...)
inline bool AS_BOOL(Value value) { return value.as.boolean; }
inline double AS_NUMBER(Value value) { return value.as.number; }
inline Obj *AS_OBJ(Value value) { return value.as.obj; }

inline Value BOOL_VAL(bool value) {
    Value v;
    v.type = VAL_BOOL;
    v.as.boolean = value;
    return v;
}

inline Value NIL_VAL() {
    Value v;
    v.type = VAL_NIL;
    v.as.number = 0; // Зануляем память для порядка
    return v;
}

inline Value NUMBER_VAL(double value) {
    Value v;
    v.type = VAL_NUMBER;
    v.as.number = value;
    return v;
}

template <typename T> inline Value OBJ_VAL(T *object) {
    Value v;
    v.type = VAL_OBJ;
    // reinterpret_cast безопасно приведет ObjString*, ObjFunction* и т.д. к
    // базовому Obj*
    v.as.obj = reinterpret_cast<Obj *>(object);
    return v;
}

// ============================================================================
// КЛАСС МАССИВА ЗНАЧЕНИЙ (ValueArray)
// ============================================================================

class ValueArray {
  public:
    ValueArray();

    void init();
    void free();
    void write(Value value);

    int getCount() const { return count; }
    Value getValue(int index) const { return values[index]; }

    int capacity;
    int count;
    Value *values;
};

// глобальные функции для работы со значениями
void printValue(Value value);
bool valuesEqual(Value a, Value b);
