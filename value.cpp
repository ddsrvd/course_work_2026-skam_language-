#include "value.h"
#include "memory.h"
#include "object.h" // Нужен для вызова printObject
#include <cstdio>   

// --- Реализация методов класса ValueArray ---

ValueArray::ValueArray() { init(); }

void ValueArray::init() {
    values = nullptr;
    capacity = 0;
    count = 0;
}

void ValueArray::free() {

    freeArray<Value>(values, capacity);
    init();
}

void ValueArray::write(Value value) {
    if (capacity < count + 1) {
        int oldCapacity = capacity;
        capacity = growCapacity(oldCapacity);

        values = growArray<Value>(values, oldCapacity, capacity);
    }

    values[count] = value;
    count++;
}

void printValue(Value value) {
    switch (value.type) {
    case VAL_BOOL:
        std::printf(AS_BOOL(value) ? "true" : "false");
        break;
    case VAL_NIL:
        std::printf("nil");
        break;
    case VAL_NUMBER:
        std::printf("%g", AS_NUMBER(value));
        break;
    case VAL_OBJ:
        printObject(value);
        break;
    }
}

bool valuesEqual(Value a, Value b) {
    // Если типы разные, то значения точно не равны (например, 1 не равно "1")
    if (a.type != b.type)
        return false;

    switch (a.type) {
    case VAL_BOOL:
        return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:
        return true; // nil всегда равен nil
    case VAL_NUMBER:
        return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_OBJ:
        return AS_OBJ(a) == AS_OBJ(b); // Для объектов пока сравниваем указатели
                                       // (адреса в памяти)
    default:
        return false; // Недостижимое место в коде
    }
}
