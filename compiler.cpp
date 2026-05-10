#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "object.h"
#include "scanner.h"
#include "value.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

// Анонимное пространство имен надежно скрывает всё внутреннее состояние
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

using ParseFn = void (*)(bool canAssign);

struct ParseRule {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};

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

bool check(TokenType type) { return parser.current.type == type; }

bool match(TokenType type) {
    if (!check(type))
        return false;
    advance();
    return true;
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

// Предварительные объявления всех необходимых функций
void expression();
void statement();
void declaration();
ParseRule *getRule(TokenType type);
void parsePrecedence(Precedence precedence);

uint8_t identifierConstant(Token *name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

uint8_t parseVariable(const char *errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);
    return identifierConstant(&parser.previous);
}

void defineVariable(uint8_t global) { emitBytes(OP_DEFINE_GLOBAL, global); }

void namedVariable(Token name, bool canAssign) {
    uint8_t arg = identifierConstant(&name);
    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(OP_SET_GLOBAL, arg);
    } else {
        emitBytes(OP_GET_GLOBAL, arg);
    }
}

void variable(bool canAssign) { namedVariable(parser.previous, canAssign); }

void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule *rule = getRule(operatorType);

    parsePrecedence(static_cast<Precedence>(rule->precedence + 1));

    switch (operatorType) {
    case TOKEN_BANG_EQUAL:
        emitBytes(OP_EQUAL, OP_NOT);
        break;
    case TOKEN_EQUAL_EQUAL:
        emitByte(OP_EQUAL);
        break;
    case TOKEN_GREATER:
        emitByte(OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emitBytes(OP_LESS, OP_NOT);
        break;
    case TOKEN_LESS:
        emitByte(OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emitBytes(OP_GREATER, OP_NOT);
        break;
    case TOKEN_MINUS:
        emitByte(OP_SUBTRACT);
        break;
    case TOKEN_PLUS:
        emitByte(OP_ADD);
        break;
    case TOKEN_STAR:
        emitByte(OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emitByte(OP_DIVIDE);
        break;
    default:
        return;
    }
}

void literal(bool canAssign) {
    switch (parser.previous.type) {
    case TOKEN_FALSE:
        emitByte(OP_FALSE);
        break;
    case TOKEN_NIL:
        emitByte(OP_NIL);
        break;
    case TOKEN_TRUE:
        emitByte(OP_TRUE);
        break;
    default:
        return;
    }
}

void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

void number(bool canAssign) {
    double value = std::strtod(parser.previous.start, nullptr);
    emitConstant(NUMBER_VAL(value));
}

// Объединил две функции парсинга строк в одну правильную
void stringLiteral(bool canAssign) {
    emitConstant(OBJ_VAL(
        copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
    case TOKEN_BANG:
        emitByte(OP_NOT);
        break;
    case TOKEN_MINUS:
        emitByte(OP_NEGATE);
        break;
    default:
        return;
    }
}

ParseRule rules[TOKEN_EOF + 1] = {
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
    /* [TOKEN_BANG]          = */ {unary, nullptr, PREC_NONE},
    /* [TOKEN_BANG_EQUAL]    = */ {nullptr, binary, PREC_EQUALITY},
    /* [TOKEN_EQUAL]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_EQUAL_EQUAL]   = */ {nullptr, binary, PREC_EQUALITY},
    /* [TOKEN_GREATER]       = */ {nullptr, binary, PREC_COMPARISON},
    /* [TOKEN_GREATER_EQUAL] = */ {nullptr, binary, PREC_COMPARISON},
    /* [TOKEN_LESS]          = */ {nullptr, binary, PREC_COMPARISON},
    /* [TOKEN_LESS_EQUAL]    = */ {nullptr, binary, PREC_COMPARISON},
    /* [TOKEN_IDENTIFIER]    = */ {variable, nullptr, PREC_NONE},
    /* [TOKEN_STRING]        = */ {stringLiteral, nullptr, PREC_NONE},
    /* [TOKEN_NUMBER]        = */ {number, nullptr, PREC_NONE},
    /* [TOKEN_AND]           = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_CLASS]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_ELSE]          = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_FALSE]         = */ {literal, nullptr, PREC_NONE},
    /* [TOKEN_FOR]           = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_FUN]           = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_IF]            = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_NIL]           = */ {literal, nullptr, PREC_NONE},
    /* [TOKEN_OR]            = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_PRINT]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_RETURN]        = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_SUPER]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_THIS]          = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_TRUE]          = */ {literal, nullptr, PREC_NONE},
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

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

ParseRule *getRule(TokenType type) { return &rules[type]; }

void expression() { parsePrecedence(PREC_ASSIGNMENT); }

void varDeclaration() {
    uint8_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
}

void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;

        switch (parser.current.type) {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return;
        default:; // Do nothing.
        }
        advance();
    }
}

void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else {
        expressionStatement();
    }
}

void declaration() {
    if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode)
        synchronize();
}

void endCompiler() {
    emitReturn();

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

} // namespace

// Единственная функция, доступная для вызова из других файлов
bool compile(const char *source, Chunk *chunk) {
    initScanner(source);
    compilingChunk = chunk;
    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }
    endCompiler();

    return !parser.hadError;
}