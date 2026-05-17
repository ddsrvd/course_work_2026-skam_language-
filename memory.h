#pragma once

#include "common.h"
#include "object.h"
#include <cstddef>

void *reallocate(void *pointer, size_t oldSize, size_t newSize);

// Выделяет память под указанное количество элементов типа T.
template <typename T> T *allocate(size_t count = 1) {

    void *memory = reallocate(nullptr, 0, sizeof(T) * count);
    return static_cast<T *>(memory);
}

// Освобождает память, занятую одним объектом типа T.
template <typename T> void freePointer(T *pointer) {
    // Передаем 0 как новый размер — это аналог free.
    reallocate(pointer, sizeof(T), 0);
}

// Рассчитывает новую вместимость массива
constexpr int growCapacity(int capacity) {
    if (capacity < 8) {
        return 8;
    } else {
        return capacity * 2;
    }
}

// Увеличивает или уменьшает размер существующего массива.
template <typename T> T *growArray(T *pointer, int oldCount, int newCount) {
    size_t oldSize = sizeof(T) * oldCount;
    size_t newSize = sizeof(T) * newCount;

    void *newMemory = reallocate(pointer, oldSize, newSize);
    return static_cast<T *>(newMemory);
}

// Удаляет динамический массив.
template <typename T> void freeArray(T *pointer, int oldCount) {
    size_t oldSize = sizeof(T) * oldCount;
    reallocate(pointer, oldSize, 0);
}

// Garbage Collection

// Помечает объект как "живой" (достижимый)
void markObject(Obj *object);

// Помечает значение как "живое"
void markValue(Value value);

// Запускает процесс поиска и удаления недостижимых объектов
void collectGarbage();

// Принудительно удаляет ВСЕ объекты (вызывается при завершении работы VM)
void freeObjects();
