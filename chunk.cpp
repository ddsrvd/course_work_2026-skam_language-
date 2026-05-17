#include "chunk.h"
#include "memory.h"
#include "vm.h"
#include <cstdlib>

Chunk::Chunk() { init(); }

void Chunk::init() { // инициализация
    count = 0;
    capacity = 0;
    code = nullptr;
    lines = nullptr;
    constants.init();
}

void Chunk::free() {
    freeArray<uint8_t>(code, capacity);
    freeArray<int>(lines, capacity);
    constants.free();
    init(); // cбрасываем в исходное состояние
}

void Chunk::write(uint8_t byte, int line) {
    if (capacity < count + 1) {
        int oldCapacity = capacity;
        capacity = growCapacity(oldCapacity);
        code = growArray<uint8_t>(code, oldCapacity, capacity);
        lines = growArray<int>(lines, oldCapacity, capacity);
    }

    code[count] = byte;
    lines[count] = line;
    count++;
}

int Chunk::addConstant(Value value) {

    vm.push(value);

    constants.write(value);

    vm.pop();

    return constants.getCount() - 1;
}
