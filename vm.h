#ifndef skam_vm_h
#define skam_vm_h

#include "object.h"
#include "table.h"
#include "value.h"
#define FRAMES_MAX 64

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
    ObjClosure *closure;
    uint8_t *ip;
    Value *slots;
} CallFrame;

class VM {
  public:
    VM();
    ~VM();

    InterpretResult interpret(const char *source);
    InterpretResult interpretBinary(ObjFunction *function);
    void push(Value value);
    Value pop();

    CallFrame frames[64];
    int frameCount;

    Value stack[64 * 256];
    Value *stackTop;

    Table globals;
    Table strings;
    ObjString *initString;
    ObjUpvalue *openUpvalues;

    size_t bytesAllocated;
    size_t nextGC;
    Obj *objects;
    int grayCount;
    int grayCapacity;
    Obj **grayStack;

  private:
    void resetStack();
    void runtimeError(const char *format, ...);
    Value peek(int distance) const;
    bool isFalsey(Value value) const;
    bool bindMethod(ObjClass *klass, ObjString *name);
    bool invokeFromClass(ObjClass *klass, ObjString *name, int argCount);
    void defineMethod(ObjString *name);
    bool callValue(Value callee, int argCount);
    bool invoke(ObjString *name, int argCount);
    void defineNative(const char *name, NativeFn function);
    bool call(ObjClosure *closure, int argCount);
    void concatenate();
    ObjUpvalue *captureUpvalue(Value *local);
    void closeUpvalues(Value *last);
    InterpretResult run();
};

extern VM vm;

#endif