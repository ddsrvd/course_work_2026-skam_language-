#ifndef SKAM_VM_H
#define SKAM_VM_H

#include "chunk.h"
#include "value.h"
#include "object.h"

#define STACK_MAX 256

struct VM {
    Chunk* chunk = nullptr;
    uint8_t* ip = nullptr;

    Value stack[STACK_MAX];
    Value* stackTop = nullptr;

    Obj* objects = nullptr;
};

extern VM vm;

enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

void initVM();
void freeVM();

InterpretResult interpret(const char* source);

#endif