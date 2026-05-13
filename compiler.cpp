#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <cstring>

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
    PREC_PRIMARY,
};

using ParseFn = void (*)(bool canAssign);

struct ParseRule {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};

struct Local {
    Token name;
    int depth;
};

enum FunctionType {
    TYPE_FUNCTION,
    TYPE_SCRIPT
};

struct Compiler {
  struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
};

Parser parser;
Compiler* current = nullptr;
static Chunk* currentChunk() {
    return &current->function->chunk;
}

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
static int emitJump(uint8_t instruction) {
    emitByte(instruction);

    // placeholder bytes
    emitByte(0xff);
    emitByte(0xff);

    return currentChunk()->count - 2;
}

void emitReturn() { 
    emitByte(OP_NIL);
    emitByte(OP_RETURN);   
}

uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return static_cast<uint8_t>(constant);
}

void emitConstant(Value value) { emitBytes(OP_CONSTANT, makeConstant(value)); }

static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;
    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    
    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

static void patchJump(int offset) {
    // -2 because jump operand itself takes 2 bytes
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] =
        (jump >> 8) & 0xff;

    currentChunk()->code[offset + 1] =
        jump & 0xff;
}
static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset =
        currentChunk()->count - loopStart + 2;

    if (offset > UINT16_MAX) {
        error("Loop body too large.");
    }

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}
// Предварительные объявления всех необходимых функций
void expression();
void statement();
static void ifStatement();
static void whileStatement();
static void forStatement();
void declaration();
static void and_(bool canAssign);
static void or_(bool canAssign);
ParseRule *getRule(TokenType type);
void parsePrecedence(Precedence precedence);

uint8_t identifierConstant(Token *name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return std::string_view(a->start, a->length) == //instead of memcmp we use strign_view here
           std::string_view(b->start, b->length);
}

int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i>= 0; i--){
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)){
            if (local->depth == -1) {
                error("Cant read local variable in its own initializer");
            }
            return i;
        }
    }
    return -1;
}
void addLocal(Token name){
    if (current->localCount == UINT8_COUNT){
        error("Too many local variables in function");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
}

void declareVariable() {
    if (current->scopeDepth == 0) return;

    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--){
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }
        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope");
        }
    }

    addLocal(*name);
}
uint8_t parseVariable(const char *errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0) return 0;
    return identifierConstant(&parser.previous);
}

void markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

void defineVariable(uint8_t global) { 
    if (current->scopeDepth > 0){
        markInitialized();
        return;
    }
    emitBytes(OP_DEFINE_GLOBAL, global); 
}

static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if(arg != -1){
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }
    
    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else {
        emitBytes(getOp, (uint8_t)arg);
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

static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
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
    /* [TOKEN_LEFT_PAREN]    = */ {grouping, call,   PREC_CALL},
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
    /* [TOKEN_AND]           = */ {nullptr,  and_, PREC_AND},
    /* [TOKEN_CLASS]         = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_ELSE]          = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_FALSE]         = */ {literal, nullptr, PREC_NONE},
    /* [TOKEN_FOR]           = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_FUN]           = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_IF]            = */ {nullptr, nullptr, PREC_NONE},
    /* [TOKEN_NIL]           = */ {literal, nullptr, PREC_NONE},
    /* [TOKEN_OR]            = */ {nullptr, or_, PREC_OR},
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

void block(){
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope(); 

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
  }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = endCompiler();
    emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
}

static void funDeclaration() {
  uint8_t global = parseVariable("Expect function name.");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}

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

static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)) {
        emitReturn();
    } else {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
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
//объявил раньше, чтобы избежать ошибок
void beginScope();
void endScope();

void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();

    } else if (match(TOKEN_IF)) {
        ifStatement();

    } else if (match(TOKEN_RETURN)) {
        returnStatement();

    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();

    } else if (match(TOKEN_WHILE)) {
    whileStatement();

    } else if (match(TOKEN_FOR)) {
    forStatement();

    }else {
        expressionStatement();
    }
}

static void ifStatement() {
    consume(TOKEN_LEFT_PAREN,
        "Expect '(' after 'if'.");

    expression();

    consume(TOKEN_RIGHT_PAREN,
        "Expect ')' after condition.");

    int thenJump =
        emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);

    statement();

    int elseJump =
        emitJump(OP_JUMP);

    patchJump(thenJump);

    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) {
        statement();
    }

    patchJump(elseJump);
}

static void whileStatement() {

    int loopStart =
        currentChunk()->count;

    consume(TOKEN_LEFT_PAREN,
        "Expect '(' after 'while'.");

    expression();

    consume(TOKEN_RIGHT_PAREN,
        "Expect ')' after condition.");

    int exitJump =
        emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);

    statement();

    emitLoop(loopStart);

    patchJump(exitJump);

    emitByte(OP_POP);
}

static void forStatement() {

    beginScope();

    consume(TOKEN_LEFT_PAREN,
        "Expect '(' after 'for'.");

    if (match(TOKEN_SEMICOLON)) {
        // no initializer

    } else if (match(TOKEN_VAR)) {
        varDeclaration();

    } else {
        expressionStatement();
    }

    int loopStart =
        currentChunk()->count;

    int exitJump = -1;

    if (!match(TOKEN_SEMICOLON)) {

    expression();

    consume(TOKEN_SEMICOLON,
        "Expect ';' after loop condition.");

    exitJump =
        emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
}

    if (!match(TOKEN_RIGHT_PAREN)) {

    int bodyJump =
        emitJump(OP_JUMP);

    int incrementStart =
        currentChunk()->count;

    expression();

    emitByte(OP_POP);

    consume(TOKEN_RIGHT_PAREN,
        "Expect ')' after for clauses.");

    emitLoop(loopStart);

    loopStart = incrementStart;

    patchJump(bodyJump);
    }

    statement();

    emitLoop(loopStart);

    if (exitJump != -1) {
        patchJump(exitJump);

        emitByte(OP_POP);
    }

    endScope();
}

void declaration() {
    if (match(TOKEN_FUN)) {
        funDeclaration();
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode)
        synchronize();
}


static void and_(bool canAssign) {
    int endJump =
        emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);

    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

static void or_(bool canAssign) {
    int elseJump =
        emitJump(OP_JUMP_IF_FALSE);

    int endJump =
        emitJump(OP_JUMP);

    patchJump(elseJump);

    emitByte(OP_POP);

    parsePrecedence(PREC_OR);

    patchJump(endJump);
}

static ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
}

void beginScope(){
    current->scopeDepth++;
}

void endScope(){
    current->scopeDepth--;

    while (current->localCount > 0 &&
            current->locals[current->localCount - 1].depth > current->scopeDepth) {
                emitByte(OP_POP);
                current->localCount--;
            }
}

} // namespace

// Единственная функция, доступная для вызова из других файлов
ObjFunction* compile(const char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}