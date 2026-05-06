#ifndef skam_value_h
#define skam_value_h

#include "common.h"

enum ValueType{
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER
};

struct Value {
    ValueType type;
    union {
        bool boolean;
        double number;
    } as;

    Value(ValueType t, double v) : type(t) { as.number = v; }
    Value(ValueType t, bool b) : type(t) { as.boolean = b; }
    // Пустой конструктор для совместимости
    Value() = default; 
};


#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)

#define BOOL_VAL(value)   (Value(VAL_BOOL, (bool)(value)))
#define NIL_VAL       (Value(VAL_NIL, 0.0))
#define NUMBER_VAL(value) (Value(VAL_NUMBER, (double)(value)))

struct ValueArray {
    int capacity = 0;
    int count = 0;
    Value* values = nullptr;
};
bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);


#endif
