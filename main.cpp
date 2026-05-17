#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "vm.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

static std::string readFile(const std::string &path) {

    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        std::cerr << "Could not open file \"" << path << "\"." << std::endl;
        std::exit(74);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string buffer(size, '\0');
    if (!file.read(&buffer[0], size)) {
        std::cerr << "Could not read file \"" << path << "\"." << std::endl;
        std::exit(74);
    }

    return buffer;
}

static void runFile(const std::string &path) {
    std::string source = readFile(path);

    InterpretResult result = vm.interpret(source.c_str());

    if (result == INTERPRET_COMPILE_ERROR)
        std::exit(65);
    if (result == INTERPRET_RUNTIME_ERROR)
        std::exit(70);
}

static void repl() {
    std::string line;
    std::cout << "skam language console (v1.0)" << std::endl;

    for (;;) {
        std::cout << "> ";

        if (!std::getline(std::cin, line)) {
            std::cout << std::endl;
            break;
        }

        if (line == "exit")
            break;

        vm.interpret(line.c_str());
    }
}

void saveChunk(std::ofstream &file, Chunk *chunk) {
    // 1. Сохраняем байткод
    file.write(reinterpret_cast<const char *>(&chunk->count), sizeof(int));
    if (chunk->count > 0) {
        file.write(reinterpret_cast<const char *>(chunk->code),
                   chunk->count * sizeof(uint8_t));
        // Сохраняем номера строк (для вывода ошибок)
        file.write(reinterpret_cast<const char *>(chunk->lines),
                   chunk->count * sizeof(int));
    }

    // 2. Сохраняем пул констант
    file.write(reinterpret_cast<const char *>(&chunk->constants.count),
               sizeof(int));

    for (int i = 0; i < chunk->constants.count; i++) {
        Value val = chunk->constants.values[i];

        if (IS_NUMBER(val)) {
            char type = 0; // Тег 0: Число
            double num = AS_NUMBER(val);
            file.write(&type, 1);
            file.write(reinterpret_cast<const char *>(&num), sizeof(double));
        } else if (IS_OBJ(val) && OBJ_TYPE(val) == OBJ_STRING) {
            char type = 1; // Тег 1: Строка
            file.write(&type, 1);
            ObjString *str = AS_STRING(val);
            file.write(reinterpret_cast<const char *>(&str->length),
                       sizeof(int));
            file.write(str->chars, str->length);
        } else if (IS_OBJ(val) && OBJ_TYPE(val) == OBJ_FUNCTION) {
            char type = 2; // Тег 2: Функция (Запускаем рекурсию!)
            file.write(&type, 1);
            ObjFunction *func = AS_FUNCTION(val);

            // Пишем метаданные функции
            file.write(reinterpret_cast<const char *>(&func->arity),
                       sizeof(int));
            file.write(reinterpret_cast<const char *>(&func->upvalueCount),
                       sizeof(int));

            // Пишем имя функции (если оно есть)
            if (func->name != nullptr) {
                file.write(reinterpret_cast<const char *>(&func->name->length),
                           sizeof(int));
                file.write(func->name->chars, func->name->length);
            } else {
                int noName = 0;
                file.write(reinterpret_cast<const char *>(&noName),
                           sizeof(int));
            }

            // РЕКУРСИЯ: сохраняем внутренний чанк функции
            saveChunk(file, &func->chunk);
        }
    }
}

void loadChunk(std::ifstream &file, Chunk *chunk) {
    // 1. Читаем байткод
    int codeCount;
    file.read(reinterpret_cast<char *>(&codeCount), sizeof(int));

    chunk->count = codeCount;
    chunk->capacity = codeCount;

    if (codeCount > 0) {
        chunk->code = allocate<uint8_t>(codeCount);
        file.read(reinterpret_cast<char *>(chunk->code),
                  codeCount * sizeof(uint8_t));

        chunk->lines = allocate<int>(codeCount);
        file.read(reinterpret_cast<char *>(chunk->lines),
                  codeCount * sizeof(int));
    } else {
        chunk->code = nullptr;
        chunk->lines = nullptr;
    }

    // 2. Читаем пул констант
    int constCount;
    file.read(reinterpret_cast<char *>(&constCount), sizeof(int));
    chunk->constants.capacity = constCount;

    if (constCount > 0) {
        chunk->constants.values = allocate<Value>(constCount);
        // ЗАЩИТА: Заполняем массив нулями (NIL), чтобы GC не прочитал мусор!
        for (int i = 0; i < constCount; i++) {
            chunk->constants.values[i] = NIL_VAL();
        }
    } else {
        chunk->constants.values = nullptr;
    }

    chunk->constants.count = constCount;

    for (int i = 0; i < constCount; i++) {
        char type;
        file.read(&type, 1);

        if (type == 0) {
            double num;
            file.read(reinterpret_cast<char *>(&num), sizeof(double));
            chunk->constants.values[i] = NUMBER_VAL(num);
        } else if (type == 1) {
            int len;
            file.read(reinterpret_cast<char *>(&len), sizeof(int));

            char *buffer = new char[len];
            file.read(buffer, len);
            chunk->constants.values[i] = OBJ_VAL(copyString(buffer, len));
            delete[] buffer;
        } else if (type == 2) {
            ObjFunction *func = newFunction();

            // ЗАЩИТА: Сразу кладем функцию в пул констант родителя.
            // Так как родитель уже на стеке, GC увидит и эту функцию!
            chunk->constants.values[i] = OBJ_VAL(func);

            file.read(reinterpret_cast<char *>(&func->arity), sizeof(int));
            file.read(reinterpret_cast<char *>(&func->upvalueCount),
                      sizeof(int));

            int nameLen;
            file.read(reinterpret_cast<char *>(&nameLen), sizeof(int));
            if (nameLen > 0) {
                char *nameBuffer = new char[nameLen];
                file.read(nameBuffer, nameLen);
                func->name = copyString(nameBuffer, nameLen);
                delete[] nameBuffer;
            } else {
                func->name = nullptr;
            }

            loadChunk(file, &func->chunk);
        }
    }
}

// Режим 2: Компиляция в бинарный файл
static void compileFile(const char *sourcePath) {
    std::string rawSource = readFile(sourcePath);
    std::string source(rawSource.c_str());

    ObjFunction *function = compile(source.c_str());

    if (function == nullptr) {
        std::cerr << "Compilation failed.\n";
        std::exit(65);
    }

    std::string pathStr(sourcePath);
    std::string binaryPath = pathStr;

    size_t dotPos = binaryPath.find_last_of('.');
    if (dotPos != std::string::npos) {
        binaryPath = binaryPath.substr(0, dotPos) + ".skamb";
    } else {
        binaryPath += ".skamb";
    }

    std::ofstream file(binaryPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file " << binaryPath << " for writing.\n";
        std::exit(74);
    }

    char magic[4] = {'S', 'K', 'A', 'M'};
    file.write(magic, 4);
    saveChunk(file, &function->chunk);

    std::cout << "[Compiler] SUCCESS: Source compiled to binary format.\n";
    std::cout << "[Compiler] Root instructions saved: " << function->chunk.count
              << "\n";
    std::cout << "[Compiler] Root constants saved: "
              << function->chunk.constants.count << "\n";
    std::cout << "[Compiler] Binary saved to: '" << binaryPath << "'\n";
}

static void runBinaryFile(const char *binaryPath) {
    std::ifstream file(binaryPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file " << binaryPath << ".\n";
        std::exit(74);
    }

    char magic[4];
    file.read(magic, 4);
    if (std::strncmp(magic, "SKAM", 4) != 0) {
        std::cerr << "ERROR: Invalid binary format or corrupted file.\n";
        std::exit(65);
    }

    ObjFunction *function = newFunction();

    // Защита от Сборщика Мусора (GC)
    vm.push(OBJ_VAL(function));
    loadChunk(file, &function->chunk);
    vm.pop();

    std::cout << "[VM] Successfully loaded binary file: " << binaryPath << "\n";
    std::cout << "[VM] Instructions loaded: " << function->chunk.count << "\n";
    std::cout << "[VM] Constants loaded: " << function->chunk.constants.count
              << "\n";
    std::cout << "--------------------------------------------------\n";

    vm.interpretBinary(function);
}

int main(int argc, char *argv[]) {
    // 1. Интерактивный режим (REPL)
    if (argc == 1) {
        repl();
    }
    // 2. Режим компиляции (флаг -c)
    else if (argc == 3 && std::strcmp(argv[1], "-c") == 0) {
        compileFile(argv[2]);
    }
    // 3. Режим запуска файла
    else if (argc == 2) {
        std::string path = argv[1];

        // Проверяем расширение: если .skamb — запускаем бинарник, иначе как
        // обычно
        if (path.length() >= 6 && path.substr(path.length() - 6) == ".skamb") {
            runBinaryFile(path.c_str());
        } else {
            runFile(path.c_str());
        }
    }
    // Ошибка аргументов
    else {
        std::fprintf(stderr, "Usage:\n");
        std::fprintf(stderr, "  skam                 (REPL mode)\n");
        std::fprintf(stderr, "  skam [file.skam]     (Run script)\n");
        std::fprintf(stderr, "  skam [file.skamb]    (Run binary)\n");
        std::fprintf(stderr, "  skam -c [file.skam]  (Compile to binary)\n");
        return 64;
    }

    return 0;
}