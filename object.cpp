#include "object.h"
#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h" // нужен для доступа к vm.objects и vm.strings

#include <cstdio>
#include <cstring>

// внутренний шаблонный аллокатор для всех объектов
template <typename T> static T *allocateObject(ObjType type) {
    // выделяем память нужного размера
    Obj *object = static_cast<Obj *>(reallocate(nullptr, 0, sizeof(T)));
    object->type = type;
    object->isMarked = false;

    // привязываем новый объект к глобальной цепочке gc
    object->next = vm.objects;
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    std::printf("%p allocate %zu for %d\n", static_cast<void *>(object),
                sizeof(T), type);
#endif

    return reinterpret_cast<T *>(object);
}

ObjBoundMethod *newBoundMethod(Value receiver, ObjClosure *method) {
    ObjBoundMethod *bound = allocateObject<ObjBoundMethod>(OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjClass *newClass(ObjString *name) {
    ObjClass *klass = allocateObject<ObjClass>(OBJ_CLASS);
    klass->name = name;
    klass->methods.init(); // используем плюсовый метод таблицы
    return klass;
}

ObjClosure *newClosure(ObjFunction *function) {
    // выделяем массив для upvalues (замыканий)
    ObjUpvalue **upvalues = allocate<ObjUpvalue *>(function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = nullptr;
    }

    ObjClosure *closure = allocateObject<ObjClosure>(OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjFunction *newFunction() {
    ObjFunction *function = allocateObject<ObjFunction>(OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = nullptr;
    function->chunk.init(); // используем плюсовый метод чанка
    return function;
}

ObjInstance *newInstance(ObjClass *klass) {
    ObjInstance *instance = allocateObject<ObjInstance>(OBJ_INSTANCE);
    instance->klass = klass;
    instance->fields.init();
    return instance;
}

ObjNative *newNative(NativeFn function) {
    ObjNative *native = allocateObject<ObjNative>(OBJ_NATIVE);
    native->function = function;
    return native;
}

// создание строки и запись ее в таблицу уникальных строк
static ObjString *allocateString(char *chars, int length, uint32_t hash) {
    ObjString *string = allocateObject<ObjString>(OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    // защищаем строку от удаления сборщиком мусора во время записи в таблицу
    vm.push(OBJ_VAL(string));
    vm.strings.set(string, NIL_VAL());
    vm.pop();

    return string;
}

static uint32_t hashString(const char *key, int length) {
    // алгоритм хэширования fnv-1a
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= static_cast<uint8_t>(key[i]);
        hash *= 16777619;
    }
    return hash;
}

ObjString *takeString(char *chars, int length) {
    uint32_t hash = hashString(chars, length);

    // ищем, нет ли уже такой строки в памяти
    ObjString *interned = vm.strings.findString(chars, length, hash);
    if (interned != nullptr) {
        // если есть, то забранный массив символов нам не нужен — удаляем его
        freeArray<char>(chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString *copyString(const char *chars, int length) {
    uint32_t hash = hashString(chars, length);

    ObjString *interned = vm.strings.findString(chars, length, hash);
    if (interned != nullptr)
        return interned;

    // выделяем память и копируем символы
    char *heapChars = allocate<char>(length + 1);
    std::memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(heapChars, length, hash);
}

ObjUpvalue *newUpvalue(Value *slot) {
    ObjUpvalue *upvalue = allocateObject<ObjUpvalue>(OBJ_UPVALUE);
    upvalue->closed = NIL_VAL();
    upvalue->location = slot;
    upvalue->next = nullptr;
    return upvalue;
}

static void printFunction(ObjFunction *function) {
    if (function->name == nullptr) {
        std::printf("<script>");
        return;
    }
    std::printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
    case OBJ_BOUND_METHOD:
        printFunction(AS_BOUND_METHOD(value)->method->function);
        break;
    case OBJ_CLASS:
        std::printf("%s", AS_CLASS(value)->name->chars);
        break;
    case OBJ_CLOSURE:
        printFunction(AS_CLOSURE(value)->function);
        break;
    case OBJ_FUNCTION:
        printFunction(AS_FUNCTION(value));
        break;
    case OBJ_INSTANCE:
        std::printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
        break;
    case OBJ_NATIVE:
        std::printf("<native fn>");
        break;
    case OBJ_STRING:
        std::printf("%s", AS_CSTRING(value));
        break;
    case OBJ_UPVALUE:
        std::printf("upvalue");
        break;
    }
}
