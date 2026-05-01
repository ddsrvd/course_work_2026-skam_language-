#ifndef skam_chunk_h
#define skam_chunk_h

#include "common.h"
#include "value.h"

typedef enum { // all the functions will be there
    OP_CONSTANT,
    OP_RETURN, // return from current function
} Opcode;

typedef struct {
    int count; //how many of those allocated entries are in use
    int capacity; //the number of elements in the array we have allocated
    uint8_t* code;
    int* lines;
    ValueArray constants;
} Chunk;


void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);
#endif