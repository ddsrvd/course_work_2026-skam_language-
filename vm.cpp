#include "vm.h"
#include "compiler.h"
#include <iostream>

// Анонимное пространство имен скрывает внутреннее состояние (замена static)
namespace {

// Глобальный (но скрытый) экземпляр виртуальной машины
VM vm;

void resetStack() { vm.stackTop = vm.stack; }

// Извлекает верхнее значение из стека
Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

// Главный цикл исполнения (сердце эмулятора)
InterpretResult run() {
// Макросы для удобного чтения байтов и констант
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

// Макрос для бинарных операций (+, -, *, /)
#define BINARY_OP(op)                                                          \
    do {                                                                       \
        double b = pop();                                                      \
        double a = pop();                                                      \
        push(a op b);                                                          \
    } while (false)

    for (;;) {
        // 1. Извлечение инструкции (Fetch)
        uint8_t instruction = READ_BYTE();

        // 2. Декодирование и исполнение (Decode & Execute)
        switch (instruction) {
        case OP_CONSTANT: {
            Value constant = READ_CONSTANT();
            push(constant);
            break;
        }
        case OP_ADD:
            BINARY_OP(+);
            break;
        case OP_SUBTRACT:
            BINARY_OP(-);
            break;
        case OP_MULTIPLY:
            BINARY_OP(*);
            break;
        case OP_DIVIDE:
            BINARY_OP(/);
            break;

        case OP_NEGATE:
            push(-pop());
            break;

        case OP_RETURN: {
            // Пока что RETURN просто выводит финальный результат программы
            std::cout << pop() << std::endl;
            return INTERPRET_OK;
        }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

} // namespace

void initVM() { resetStack(); }

void freeVM() {
    // В будущем здесь будет запуск Сборщика мусора для очистки всех объектов
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

InterpretResult interpret(const char *source) {
    Chunk chunk;
    initChunk(&chunk); // Создаем пустой чанк для компилятора

    // Просим компилятор перевести текст в байт-код и положить в чанк
    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR; // Ошибка синтаксиса
    }

    // Настраиваем машину на выполнение сгенерированного чанка
    vm.chunk = &chunk;
    vm.ip = vm.chunk->code; // Ставим "курсор" на первый байт

    // Запускаем двигатель!
    InterpretResult result = run();

    // После завершения программы очищаем память чанка
    freeChunk(&chunk);
    return result;
}