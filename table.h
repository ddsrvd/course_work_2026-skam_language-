#pragma once

#include "common.h"
#include "value.h"

// предварительное объявление для избежания циклических зависимостей
struct ObjString;

// запись в хэш-таблице (ключ-значение)
struct Entry {
    ObjString *key;
    Value value;
};

class Table {
  public:
    Table();

    // инициализация и ручное управление памятью для gc
    void init();
    void free();

    // основные операции
    bool get(ObjString *key, Value *value) const;
    bool set(ObjString *key, Value value);
    bool remove(ObjString *key); // заменили delete на remove, так как delete -
                                 // ключевое слово c++
    void addAll(const Table &from);

    // поиск строки по сырым символам (для интернирования)
    ObjString *findString(const char *chars, int length, uint32_t hash) const;

    // методы для интеграции со сборщиком мусора
    void removeWhite();
    void mark();

    int count;
    int capacity;
    Entry *entries;

    // скрытые вспомогательные методы
    Entry *findEntry(Entry *entriesArray, int currentCapacity,
                     ObjString *key) const;
    void adjustCapacity(int newCapacity);
};