#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

static std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file \"" << path << "\"." << std::endl;
        std::exit(74);
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);  // rewind

    std::string buffer(fileSize, '\0');
    if (buffer.empty() && fileSize > 0) {
        std::cerr << "Not enough memory to read \"" << path << "\"." << std::endl;
        std::exit(74);
    }

    file.read(&buffer[0], fileSize);
    size_t bytesRead = file.gcount();

    if (bytesRead < fileSize) {
        std::cerr << "Could not read file \"" << path << "\"." << std::endl;
        std::exit(74);
    }

    file.close();
    return buffer;
}

static void runFile(const std::string& path) {
    std::string source = readFile(path);
    InterpretResult result = interpret(source.c_str());

    if (result == INTERPRET_COMPILE_ERROR) std::exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) std::exit(70);
}

static void repl() {
    std::string line;
    for (;;) {
        std::cout << "> ";

        if (!std::getline(std::cin, line)) {
            std::cout << std::endl;
            break;
        }

        interpret(line.c_str());
    }
}

int main(int argc, const char* argv[]){
    initVM();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        std::cerr << "Usage: clox [path]" << std::endl;
        std::exit(64);
    }

    freeVM();
    return 0;
}