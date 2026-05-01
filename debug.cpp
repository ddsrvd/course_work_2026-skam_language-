#include <iostream>
#include <iomanip>
#include "debug.h"
#include "value.h"


void disassembleChunk(Chunk* chunk, const char* name) {
    std::cout << "==" << name << "==" << std::endl;

    for (int offset = 0; offset < chunk->count;){
        offset = disassembleInstruction(chunk, offset);
    }
}
static int constantInstruction(const char* name, Chunk* chunk, int offset){
    uint8_t constant = chunk->code[offset + 1];
    std::cout << name << constant;
    printValue(chunk->constants.values[constant]);
    std::cout << std::endl;
    return offset + 2;
}
static int simpleInstruction(const char* name, int offset) {
    std::cout << name << std::endl;
    return offset + 1;
}

int disassembleInstruction(Chunk* chunk, int offset) {
    std::cout << std::setfill('0') << std::setw(4) << offset << " " << std::endl;
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        std::cout << "    |";
    }
    else {
        std::cout << chunk->lines[offset];
    }
    uint8_t instruction = chunk->code[offset];
    switch (instruction){
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            std::cout << "Unknown opcode" << instruction << std::endl;
    }
}