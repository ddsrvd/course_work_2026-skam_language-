#include <cstdlib>
#include <iostream>
#include <string>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

// Анонимное пространство имен скрывает всё это внутри текущего файла (как
// static в Си)
namespace {

struct Parser {
    Token current;
    Token previous;
    bool hadError = false;
    bool panicMode = false;
};

enum Precedence {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY
};

// В C++ используем указатели на обычные функции
using ParseFn = void (*)();

struct ParseRule {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};

// Глобальные переменные модуля (теперь безопасно скрыты в namespace)
Parser parser;
Chunk *compilingChunk = nullptr;

Chunk *currentChunk() { return compilingChunk; }

void errorAt(Token *token, const char *message) {
    if (parser.panicMode)
        return;
    parser.panicMode = true;

    std::cerr << "[line " << token->line << "] Error";

    if (token->type == TOKEN_EOF) {
        std::cerr << " at end";
    } else if (token->type != TOKEN_ERROR) {
        // std::string_view позволяет распечатать кусок строки без копирования
        std::string_view lexeme(token->start, token->length);
        std::cerr << " at '" << lexeme << "'";
    }

    std::cerr << ": " << message << std::endl;
    parser.hadError = true;
}

void error(const char *message) { errorAt(&parser.previous, message); }

void errorAtCurrent(const char *message) { errorAt(&parser.current, message); }

void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;

        errorAtCurrent(parser.current.start);
    }
}

void consume(TokenType type, const char *message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

void emitReturn() { emitByte(OP_RETURN); }

uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return static_cast<uint8_t>(constant);
}

void emitConstant(Value value) { emitBytes(OP_CONSTANT, makeConstant(value)); }

void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

// Предварительные объявления функций (Forward declarations)
void expression();
ParseRule *getRule(TokenType type);
void parsePrecedence(Precedence precedence);

void binary() {
    TokenType operatorType = parser.previous.type;
    ParseRule *rule = getRule(operatorType);

    // Компилируем правый операнд с приоритетом на 1 выше текущего
    parsePrecedence(static_cast<Precedence>(rule->precedence + 1));

    switch (operatorType) {
    case TOKEN_PLUS:
        emitByte(OP_ADD);
        break;
    case TOKEN_MINUS:
        emitByte(OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emitByte(OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emitByte(OP_DIVIDE);
        break;
    default:
        return; // Недостижимо
    }
}

void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

void number() {
    double value = std::strtod(parser.previous.start, nullptr);
    emitConstant(value);
}

void unary() {
    TokenType operatorType = parser.previous.type;

    // Компилируем операнд
    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
    case TOKEN_MINUS:
        emitByte(OP_NEGATE);
        break;
    default:
        return; // Недостижимо
    }
}

// Массив правил (теперь без конфликтов синтаксиса Си)
ParseRule rules[] = {
    /* [TOKEN_LEFT_PAREN]    = */ {grouping, nullptr, PREC_NONE},
    /* [TOKEN_RIGHT_PAREN]   = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_LEFT_BRACE]    = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_RIGHT_BRACE]   = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_COMMA]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_DOT]           = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_MINUS]         = */ {unary, binary, PREC_TERM},
    /* [TOKEN_PLUS]          = */ {nullptr, binary, PREC_TERM},
    /* [TOKEN_SEMICOLON]     = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_SLASH]         = */ {nullptr, binary, PREC_FACTOR},
    /* [TOKEN_STAR]          = */ {nullptr, binary, PREC_FACTOR},
    /* [TOKEN_BANG]          = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_BANG_EQUAL]    = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_EQUAL]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_EQUAL_EQUAL]   = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_GREATER]       = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_GREATER_EQUAL] = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_LESS]          = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_LESS_EQUAL]    = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_IDENTIFIER]    = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_STRING]        = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_NUMBER]        = */ {number, nullptr, PREC_NONE},
    /* [TOKEN_AND]           = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_CLASS]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_ELSE]          = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_FALSE]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_FOR]           = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_FUN]           = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_IF]            = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_NIL]           = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_OR]            = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_PRINT]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_RETURN]        = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_SUPER]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_THIS]          = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_TRUE]          = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_VAR]           = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_WHILE]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_ERROR]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_EOF]           = */ {nullptr, nullptr, PREC_NONE},
};

void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == nullptr) {
        error("Expect expression.");
        return;
    }

    prefixRule();

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }
}

ParseRule *getRule(TokenType type) { return &rules[type]; }

void expression() { parsePrecedence(PREC_ASSIGNMENT); }

} // namespace

bool compile(const char *source, Chunk *chunk) {
    initScanner(source);
    compilingChunk = chunk;
    parser.hadError = false;
    parser.panicMode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");
    endCompiler();

    return !parser.hadError;
}