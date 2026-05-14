#ifndef SKAM_VM_H
#define SKAM_VM_H

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

struct CallFrame {
    ObjClosure *closure;
    uint8_t *ip;
    Value *slots;
};

struct VM {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value stack[STACK_MAX];
    Value *stackTop = nullptr;
    Table globals;
    Table strings;
    ObjUpvalue *openUpvalues;

    Obj *objects = nullptr;
};

extern VM vm;

enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

void initVM();
void freeVM();

InterpretResult interpret(const char *source);

#endif