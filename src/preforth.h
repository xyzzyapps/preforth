#ifndef PREFORTH_H
#define PREFORTH_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#if defined(_WIN32) || defined(_WIN64)
#define PREFORTH_WINDOWS 1
#include <windows.h>
#elif defined(__wasi__) || defined(__EMSCRIPTEN__) || defined(__wasm__)
#define PREFORTH_WASM 1
#else
#define PREFORTH_POSIX 1
#include <dlfcn.h>
#endif

#define PF_STACK_CAPACITY 4096
#define PF_RSTACK_CAPACITY 4096
#define PF_MAX_CALLBACKS 64

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_PTR,
    VAL_STR
} ValType;

typedef union {
    int64_t i;
    uint64_t u;
    double f;
    void *ptr;
    char *str;
} Cell;

typedef struct {
    ValType type;
    Cell as;
} Value;

static inline Value val_int(int64_t i) {
    Value v;
    v.type = VAL_INT;
    v.as.i = i;
    return v;
}

static inline Value val_float(double f) {
    Value v;
    v.type = VAL_FLOAT;
    v.as.f = f;
    return v;
}

static inline Value val_ptr(void *ptr) {
    Value v;
    v.type = VAL_PTR;
    v.as.ptr = ptr;
    return v;
}

static inline Value val_str(const char *s) {
    Value v;
    v.type = VAL_STR;
    v.as.str = s ? strdup(s) : NULL;
    return v;
}

static inline bool val_is_truthy(Value v) {
    switch (v.type) {
        case VAL_INT:   return v.as.i != 0;
        case VAL_FLOAT: return v.as.f != 0.0;
        case VAL_PTR:   return v.as.ptr != NULL;
        case VAL_STR:   return v.as.str != NULL && v.as.str[0] != '\0';
    }
    return false;
}

typedef enum {
    TOK_INT,
    TOK_FLOAT,
    TOK_STR,
    TOK_IDENT,
    TOK_NL,
    TOK_COMMA,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COLON,
    TOK_SEMICOLON,
    TOK_EOF
} PFTokenType;

typedef struct {
    PFTokenType type;
    char *text;
    int64_t int_val;
    double float_val;
    int line;
    int col;
} Token;

typedef struct {
    Token *tokens;
    size_t count;
    size_t capacity;
} TokenArray;

typedef enum {
    OP_NOP,
    OP_LIT_INT,
    OP_LIT_FLOAT,
    OP_LIT_STR,
    OP_LIT_PTR,
    OP_CALL,
    OP_RET,
    OP_JMP,
    OP_JZ,
    OP_JNZ,
    OP_DUP,
    OP_DROP,
    OP_SWAP,
    OP_OVER,
    OP_TO_R,
    OP_FROM_R,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_INC,
    OP_DEC,
    OP_NEG,
    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE,
    OP_AND,
    OP_OR,
    OP_XOR,
    OP_NOT,
    OP_SHL,
    OP_SHR,
    OP_PRINT,
    OP_FETCH,
    OP_STORE,
    OP_C_FETCH8,
    OP_C_FETCH32,
    OP_C_FETCH64,
    OP_C_STORE8,
    OP_C_STORE32,
    OP_C_STORE64,
    OP_CSTR,
    OP_FFI_CALL
} Opcode;

typedef struct {
    Opcode op;
    int64_t arg;
    Value val;
    char *label_ref;
} Instruction;

typedef struct {
    char *name;
    Instruction *code;
    size_t count;
    size_t capacity;
} BytecodeBlock;

typedef enum {
    FFI_VOID,
    FFI_I32,
    FFI_U32,
    FFI_I64,
    FFI_U64,
    FFI_F64,
    FFI_PTR,
    FFI_STR
} FFIType;

typedef struct ExternFunc {
    char *name;
    void *fn_ptr;
    FFIType ret_type;
    FFIType *arg_types;
    int arg_count;
    bool is_variadic;
} ExternFunc;

typedef enum {
    WORD_PRIMITIVE,
    WORD_BYTECODE,
    WORD_VAR,
    WORD_EXTERN
} WordKind;

struct VM;

typedef struct Word {
    char *name;
    WordKind kind;
    void (*primitive_fn)(struct VM *vm);
    BytecodeBlock *bytecode;
    void *var_slot;
    ExternFunc *extern_fn;
} Word;

typedef enum {
    FRAME_CALL,
    FRAME_VALUE
} ReturnFrameType;

typedef struct {
    ReturnFrameType type;
    BytecodeBlock *block;
    size_t ip;
    Value val;
} ReturnFrame;

typedef struct CallbackEntry {
    struct VM *vm;
    int word_idx;
    FFIType ret_type;
    FFIType *arg_types;
    int arg_count;
    bool in_use;
} CallbackEntry;

typedef struct {
    char *name;
    FFIType ret_type;
    FFIType *arg_types;
    int arg_count;
} ImportEntry;

typedef struct {
    char *word_name;
    char *c_symbol_name;
    FFIType ret_type;
    FFIType *arg_types;
    int arg_count;
} ExportEntry;

typedef struct VM {
    Value data_stack[PF_STACK_CAPACITY];
    int sp;

    ReturnFrame return_stack[PF_RSTACK_CAPACITY];
    int rp;

    Word **words;
    size_t word_count;
    size_t word_capacity;

    void **libs;
    char **lib_names;
    size_t lib_count;
    size_t lib_capacity;

    CallbackEntry callbacks[PF_MAX_CALLBACKS];

    ImportEntry *imports;
    size_t import_count;
    size_t import_capacity;

    ExportEntry *exports;
    size_t export_count;
    size_t export_capacity;

    char **link_libs;
    size_t link_lib_count;
    size_t link_lib_capacity;

    char error_msg[1024];
    bool has_error;

    int max_call_depth;
    int current_call_depth;
    FILE *out_stream;

    char *custom_output_buf;
    size_t custom_output_cap;
    size_t custom_output_len;
} VM;

/* Lexer functions */
TokenArray *lexer_tokenize(const char *source);
void token_array_free(TokenArray *ta);
void token_free(Token *tok);

/* Compiler functions */
BytecodeBlock *compiler_compile(VM *vm, TokenArray *ta, const char *block_name);
void bytecode_block_free(BytecodeBlock *bb);

/* VM functions */
VM *vm_create(void);
void vm_free(VM *vm);
void vm_error(VM *vm, const char *fmt, ...);
void vm_push(VM *vm, Value v);
Value vm_pop(VM *vm);
Value vm_peek(VM *vm);
void vm_push_int(VM *vm, int64_t i);
void vm_push_float(VM *vm, double f);
void vm_push_ptr(VM *vm, void *p);
void vm_push_str(VM *vm, const char *s);

int vm_find_word(VM *vm, const char *name);
Word *vm_register_primitive(VM *vm, const char *name, void (*fn)(VM *vm));
Word *vm_register_bytecode(VM *vm, const char *name, BytecodeBlock *bb);
Word *vm_register_var(VM *vm, const char *name, void *slot);
Word *vm_register_extern(VM *vm, const char *name, ExternFunc *ef);

void vm_execute_block(VM *vm, BytecodeBlock *block);
void vm_interpret(VM *vm, const char *source);

/* FFI functions */
void ffi_init(VM *vm);
void ffi_cleanup(VM *vm);
bool ffi_load_library(VM *vm, const char *handle, const char *lib_name);
void *ffi_find_symbol(VM *vm, const char *lib_handle, const char *symbol_name);
void ffi_call_extern(VM *vm, ExternFunc *ef);
void *ffi_create_callback(VM *vm, int word_idx, FFIType ret_type, FFIType *arg_types, int arg_count);

/* AOT / Linker functions */
void vm_add_import(VM *vm, const char *name, FFIType ret_t, FFIType *args, int argc);
void vm_add_export(VM *vm, const char *word, const char *sym, FFIType ret_t, FFIType *args, int argc);
void vm_add_link_lib(VM *vm, const char *libname);

bool aot_build_shared(VM *vm, const char *out_filename);
bool aot_build_object(VM *vm, const char *out_filename);
bool aot_emit_c_file(VM *vm, const char *out_c_filename);
const char *aot_default_shared_ext(void);

#endif /* PREFORTH_H */
