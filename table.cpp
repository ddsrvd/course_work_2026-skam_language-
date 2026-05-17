#include "table.h"
#include "memory.h"
#include "object.h"
#include <cstring> // для memcmp

// максимальный уровень заполненности до расширения (75%)
constexpr double TABLE_MAX_LOAD = 0.75;

Table::Table() { init(); }

void Table::init() {
    count = 0;
    capacity = 0;
    entries = nullptr;
}

void Table::free() {

    freeArray<Entry>(entries, capacity);
    init();
}

Entry *Table::findEntry(Entry *entriesArray, int currentCapacity,
                        ObjString *key) const {
    // битовая оптимизация вместо modulo (%)
    uint32_t index = key->hash & (currentCapacity - 1);
    Entry *tombstone = nullptr;

    for (;;) {
        Entry *entry = &entriesArray[index];

        if (entry->key == nullptr) {
            if (IS_NIL(entry->value)) {
                // ячейка абсолютно пустая.
                // если мы видели надгробие по пути, возвращаем его для
                // переиспользования
                return tombstone != nullptr ? tombstone : entry;
            } else {
                // мы наткнулись на надгробие (ключ null, значение true)
                if (tombstone == nullptr)
                    tombstone = entry;
            }
        } else if (entry->key == key) {
            // нашли нужный ключ
            return entry;
        }

        // коллизия! идем к следующей ячейке по кругу
        index = (index + 1) & (currentCapacity - 1);
    }
}

bool Table::get(ObjString *key, Value *value) const {
    if (count == 0)
        return false;

    Entry *entry = findEntry(entries, capacity, key);
    if (entry->key == nullptr)
        return false;

    *value = entry->value;
    return true;
}

void Table::adjustCapacity(int newCapacity) {
    // выделяем новый чистый массив
    Entry *newEntries = allocate<Entry>(newCapacity);
    for (int i = 0; i < newCapacity; i++) {
        newEntries[i].key = nullptr;
        newEntries[i].value = NIL_VAL(); // используем функцию вместо макроса
    }

    // переносим живые данные, игнорируя надгробия
    count = 0;
    for (int i = 0; i < capacity; i++) {
        Entry *entry = &entries[i];
        if (entry->key == nullptr)
            continue;

        Entry *dest = findEntry(newEntries, newCapacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        count++;
    }

    // удаляем старый массив
    freeArray<Entry>(entries, capacity);

    entries = newEntries;
    capacity = newCapacity;
}

bool Table::set(ObjString *key, Value value) {
    // расширяемся, если таблица заполнена на 75%
    if (count + 1 > capacity * TABLE_MAX_LOAD) {
        int newCapacity = growCapacity(capacity);
        adjustCapacity(newCapacity);
    }

    Entry *entry = findEntry(entries, capacity, key);
    bool isNewKey = entry->key == nullptr;

    // увеличиваем счетчик только если ячейка была пустой (а не надгробием)
    if (isNewKey && IS_NIL(entry->value)) {
        count++;
    }

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool Table::remove(ObjString *key) {
    if (count == 0)
        return false;

    Entry *entry = findEntry(entries, capacity, key);
    if (entry->key == nullptr)
        return false;

    // устанавливаем надгробие вместо реального удаления
    entry->key = nullptr;
    entry->value = BOOL_VAL(true);
    return true;
}

void Table::addAll(const Table &from) {
    for (int i = 0; i < from.capacity; i++) {
        Entry *entry = &from.entries[i];
        if (entry->key != nullptr) {
            set(entry->key, entry->value);
        }
    }
}

ObjString *Table::findString(const char *chars, int length,
                             uint32_t hash) const {
    if (count == 0)
        return nullptr;

    uint32_t index = hash & (capacity - 1);
    for (;;) {
        Entry *entry = &entries[index];

        if (entry->key == nullptr) {
            // останавливаемся только если нашли чистую пустоту, а не надгробие
            if (IS_NIL(entry->value))
                return nullptr;
        } else if (entry->key->length == length && entry->key->hash == hash &&
                   std::memcmp(entry->key->chars, chars, length) == 0) {
            // нашли идеальное совпадение строк!
            return entry->key;
        }

        index = (index + 1) & (capacity - 1);
    }
}

void Table::removeWhite() {
    for (int i = 0; i < capacity; i++) {
        Entry *entry = &entries[i];
        if (entry->key != nullptr && !entry->key->obj.isMarked) {
            remove(entry->key);
        }
    }
}

void Table::mark() {
    for (int i = 0; i < capacity; i++) {
        Entry *entry = &entries[i];
        if (entry->key != nullptr) {
            // методы маркировки из memory.h
            markObject(reinterpret_cast<Obj *>(entry->key));
            markValue(entry->value);
        }
    }
}
