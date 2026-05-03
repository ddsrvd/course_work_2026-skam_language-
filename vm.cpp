#include "vm.h"
#include <iostream>
#include "compiler.h"

static VM vm;

static void resetStack() {
    vm.stackTop = vm.stack;
}

void initVM() {
    resetStack();
}

void freeVM() {}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

static Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}
static InterpretResult run() {

#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

#define BINARY_OP(op) \
    do { \
        double b = pop(); \
        double a = pop(); \
        push(a op b); \
    } while (false)

    for (;;) {
        uint8_t instruction = READ_BYTE();

        switch (instruction) {

            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }

            case OP_ADD: BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE: BINARY_OP(/); break;

            case OP_NEGATE:
                push(-pop());
                break;

            case OP_RETURN:
                std::cout << pop() << std::endl;
                return INTERPRET_OK;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}


InterpretResult interpret(const char* source) {
    compile(source);
    return INTERPRET_OK;
}