#include "preforth.h"
#include <stdarg.h>

VM *vm_create(void) {
    VM *vm = (VM *)calloc(1, sizeof(VM));
    vm->sp = 0;
    vm->rp = 0;
    vm->word_count = 0;
    vm->word_capacity = 64;
    vm->words = (Word **)calloc(vm->word_capacity, sizeof(Word *));
    vm->out_stream = stdout;
    vm->has_error = false;
    vm->max_call_depth = 0;
    vm->current_call_depth = 0;

    vm->import_count = 0;
    vm->import_capacity = 8;
    vm->imports = (ImportEntry *)calloc(vm->import_capacity, sizeof(ImportEntry));

    vm->export_count = 0;
    vm->export_capacity = 8;
    vm->exports = (ExportEntry *)calloc(vm->export_capacity, sizeof(ExportEntry));

    vm->link_lib_count = 0;
    vm->link_lib_capacity = 8;
    vm->link_libs = (char **)calloc(vm->link_lib_capacity, sizeof(char *));

    ffi_init(vm);

    return vm;
}

void vm_free(VM *vm) {
    if (!vm) return;

    ffi_cleanup(vm);

    for (size_t i = 0; i < vm->import_count; i++) {
        free(vm->imports[i].name);
        free(vm->imports[i].arg_types);
    }
    free(vm->imports);

    for (size_t i = 0; i < vm->export_count; i++) {
        free(vm->exports[i].word_name);
        free(vm->exports[i].c_symbol_name);
        free(vm->exports[i].arg_types);
    }
    free(vm->exports);

    for (size_t i = 0; i < vm->link_lib_count; i++) {
        free(vm->link_libs[i]);
    }
    free(vm->link_libs);

    for (size_t i = 0; i < vm->word_count; i++) {
        Word *w = vm->words[i];
        if (w) {
            free(w->name);
            if (w->kind == WORD_BYTECODE && w->bytecode) {
                bytecode_block_free(w->bytecode);
            } else if (w->kind == WORD_VAR && w->var_slot) {
                free(w->var_slot);
            } else if (w->kind == WORD_EXTERN && w->extern_fn) {
                free(w->extern_fn->name);
                free(w->extern_fn->arg_types);
                free(w->extern_fn);
            }
            free(w);
        }
    }
    free(vm->words);
    free(vm);
}

void vm_error(VM *vm, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(vm->error_msg, sizeof(vm->error_msg), fmt, args);
    va_end(args);
    vm->has_error = true;
    fprintf(stderr, "PreForth Error: %s\n", vm->error_msg);
}

void vm_push(VM *vm, Value v) {
    if (vm->sp >= PF_STACK_CAPACITY) {
        vm_error(vm, "Data stack overflow");
        return;
    }
    vm->data_stack[vm->sp++] = v;
}

Value vm_pop(VM *vm) {
    if (vm->sp <= 0) {
        vm_error(vm, "Data stack underflow");
        Value v;
        memset(&v, 0, sizeof(v));
        return v;
    }
    return vm->data_stack[--vm->sp];
}

Value vm_peek(VM *vm) {
    if (vm->sp <= 0) {
        vm_error(vm, "Data stack underflow");
        Value v;
        memset(&v, 0, sizeof(v));
        return v;
    }
    return vm->data_stack[vm->sp - 1];
}

void vm_push_int(VM *vm, int64_t i) {
    vm_push(vm, val_int(i));
}

void vm_push_float(VM *vm, double f) {
    vm_push(vm, val_float(f));
}

void vm_push_ptr(VM *vm, void *p) {
    vm_push(vm, val_ptr(p));
}

void vm_push_str(VM *vm, const char *s) {
    vm_push(vm, val_str(s));
}

int vm_find_word(VM *vm, const char *name) {
    for (int i = (int)vm->word_count - 1; i >= 0; i--) {
        if (strcmp(vm->words[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void vm_add_word(VM *vm, Word *w) {
    if (vm->word_count >= vm->word_capacity) {
        vm->word_capacity *= 2;
        vm->words = (Word **)realloc(vm->words, vm->word_capacity * sizeof(Word *));
    }
    vm->words[vm->word_count++] = w;
}

Word *vm_register_primitive(VM *vm, const char *name, void (*fn)(VM *vm)) {
    Word *w = (Word *)calloc(1, sizeof(Word));
    w->name = strdup(name);
    w->kind = WORD_PRIMITIVE;
    w->primitive_fn = fn;
    vm_add_word(vm, w);
    return w;
}

Word *vm_register_bytecode(VM *vm, const char *name, BytecodeBlock *bb) {
    Word *w = (Word *)calloc(1, sizeof(Word));
    w->name = strdup(name);
    w->kind = WORD_BYTECODE;
    w->bytecode = bb;
    vm_add_word(vm, w);
    return w;
}

Word *vm_register_var(VM *vm, const char *name, void *slot) {
    Word *w = (Word *)calloc(1, sizeof(Word));
    w->name = strdup(name);
    w->kind = WORD_VAR;
    w->var_slot = slot;
    vm_add_word(vm, w);
    return w;
}

Word *vm_register_extern(VM *vm, const char *name, ExternFunc *ef) {
    Word *w = (Word *)calloc(1, sizeof(Word));
    w->name = strdup(name);
    w->kind = WORD_EXTERN;
    w->extern_fn = ef;
    vm_add_word(vm, w);
    return w;
}

void vm_add_import(VM *vm, const char *name, FFIType ret_t, FFIType *args, int argc) {
    for (size_t i = 0; i < vm->import_count; i++) {
        if (strcmp(vm->imports[i].name, name) == 0) return;
    }
    if (vm->import_count >= vm->import_capacity) {
        vm->import_capacity *= 2;
        vm->imports = (ImportEntry *)realloc(vm->imports, vm->import_capacity * sizeof(ImportEntry));
    }
    ImportEntry *e = &vm->imports[vm->import_count++];
    e->name = strdup(name);
    e->ret_type = ret_t;
    e->arg_count = argc;
    e->arg_types = (FFIType *)malloc(argc * sizeof(FFIType));
    memcpy(e->arg_types, args, argc * sizeof(FFIType));
}

void vm_add_export(VM *vm, const char *word, const char *sym, FFIType ret_t, FFIType *args, int argc) {
    for (size_t i = 0; i < vm->export_count; i++) {
        if (strcmp(vm->exports[i].c_symbol_name, sym) == 0) return;
    }
    if (vm->export_count >= vm->export_capacity) {
        vm->export_capacity *= 2;
        vm->exports = (ExportEntry *)realloc(vm->exports, vm->export_capacity * sizeof(ExportEntry));
    }
    ExportEntry *e = &vm->exports[vm->export_count++];
    e->word_name = strdup(word);
    e->c_symbol_name = strdup(sym);
    e->ret_type = ret_t;
    e->arg_count = argc;
    e->arg_types = (FFIType *)malloc(argc * sizeof(FFIType));
    memcpy(e->arg_types, args, argc * sizeof(FFIType));
}

void vm_add_link_lib(VM *vm, const char *libname) {
    for (size_t i = 0; i < vm->link_lib_count; i++) {
        if (strcmp(vm->link_libs[i], libname) == 0) return;
    }
    if (vm->link_lib_count >= vm->link_lib_capacity) {
        vm->link_lib_capacity *= 2;
        vm->link_libs = (char **)realloc(vm->link_libs, vm->link_lib_capacity * sizeof(char *));
    }
    vm->link_libs[vm->link_lib_count++] = strdup(libname);
}

void bytecode_block_free(BytecodeBlock *bb) {
    if (!bb) return;
    for (size_t i = 0; i < bb->count; i++) {
        if (bb->code[i].label_ref) {
            free(bb->code[i].label_ref);
        }
        if (bb->code[i].op == OP_LIT_STR && bb->code[i].val.as.str) {
            free(bb->code[i].val.as.str);
        }
    }
    free(bb->code);
    free(bb->name);
    free(bb);
}

void vm_execute_block(VM *vm, BytecodeBlock *block) {
    if (!block || vm->has_error) return;

    BytecodeBlock *cur_block = block;
    size_t ip = 0;

    vm->current_call_depth++;
    if (vm->current_call_depth > vm->max_call_depth) {
        vm->max_call_depth = vm->current_call_depth;
    }

    while (ip < cur_block->count && !vm->has_error) {
        Instruction inst = cur_block->code[ip];
        switch (inst.op) {
            case OP_NOP:
                ip++;
                break;

            case OP_LIT_INT:
                vm_push(vm, inst.val);
                ip++;
                break;

            case OP_LIT_FLOAT:
                vm_push(vm, inst.val);
                ip++;
                break;

            case OP_LIT_STR:
                vm_push_str(vm, inst.val.as.str);
                ip++;
                break;

            case OP_LIT_PTR:
                vm_push(vm, inst.val);
                ip++;
                break;

            case OP_CALL: {
                int word_idx = (int)inst.arg;
                if (word_idx < 0 || (size_t)word_idx >= vm->word_count) {
                    vm_error(vm, "Invalid word index in call: %d", word_idx);
                    return;
                }
                Word *w = vm->words[word_idx];
                switch (w->kind) {
                    case WORD_PRIMITIVE:
                        w->primitive_fn(vm);
                        ip++;
                        break;
                    case WORD_VAR:
                        // Pushes address of variable slot
                        vm_push_ptr(vm, w->var_slot);
                        ip++;
                        break;
                    case WORD_EXTERN:
                        ffi_call_extern(vm, w->extern_fn);
                        ip++;
                        break;
                    case WORD_BYTECODE: {
                        if (vm->rp >= PF_RSTACK_CAPACITY) {
                            vm_error(vm, "Return stack overflow");
                            return;
                        }
                        ReturnFrame frame;
                        frame.type = FRAME_CALL;
                        frame.block = cur_block;
                        frame.ip = ip + 1;
                        vm->return_stack[vm->rp++] = frame;

                        vm->current_call_depth++;
                        if (vm->current_call_depth > vm->max_call_depth) {
                            vm->max_call_depth = vm->current_call_depth;
                        }

                        cur_block = w->bytecode;
                        ip = 0;
                        break;
                    }
                }
                break;
            }

            case OP_RET: {
                // Find nearest call frame on return stack
                bool found = false;
                while (vm->rp > 0) {
                    ReturnFrame frame = vm->return_stack[--vm->rp];
                    if (frame.type == FRAME_CALL) {
                        cur_block = frame.block;
                        ip = frame.ip;
                        vm->current_call_depth--;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    // Reached top of execution
                    vm->current_call_depth--;
                    return;
                }
                break;
            }

            case OP_JMP:
                ip = (size_t)inst.arg;
                break;

            case OP_JZ: {
                Value cond = vm_pop(vm);
                if (!val_is_truthy(cond)) {
                    ip = (size_t)inst.arg;
                } else {
                    ip++;
                }
                break;
            }

            case OP_JNZ: {
                Value cond = vm_pop(vm);
                if (val_is_truthy(cond)) {
                    ip = (size_t)inst.arg;
                } else {
                    ip++;
                }
                break;
            }

            case OP_DUP: {
                Value v = vm_peek(vm);
                vm_push(vm, v);
                ip++;
                break;
            }

            case OP_DROP:
                vm_pop(vm);
                ip++;
                break;

            case OP_SWAP: {
                Value a = vm_pop(vm);
                Value b = vm_pop(vm);
                vm_push(vm, a);
                vm_push(vm, b);
                ip++;
                break;
            }

            case OP_OVER: {
                if (vm->sp < 2) {
                    vm_error(vm, "Stack underflow on over");
                    return;
                }
                Value second = vm->data_stack[vm->sp - 2];
                vm_push(vm, second);
                ip++;
                break;
            }

            case OP_TO_R: {
                Value v = vm_pop(vm);
                if (vm->rp >= PF_RSTACK_CAPACITY) {
                    vm_error(vm, "Return stack overflow on >r");
                    return;
                }
                ReturnFrame frame;
                frame.type = FRAME_VALUE;
                frame.val = v;
                frame.block = NULL;
                frame.ip = 0;
                vm->return_stack[vm->rp++] = frame;
                ip++;
                break;
            }

            case OP_FROM_R: {
                if (vm->rp <= 0) {
                    vm_error(vm, "Return stack underflow on r>");
                    return;
                }
                ReturnFrame frame = vm->return_stack[--vm->rp];
                if (frame.type != FRAME_VALUE) {
                    vm_error(vm, "Corrupted return stack on r>");
                    return;
                }
                vm_push(vm, frame.val);
                ip++;
                break;
            }

            /* Binary operators: lhs = pop() first, rhs = pop() second */
            case OP_ADD: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                if (lhs.type == VAL_FLOAT || rhs.type == VAL_FLOAT) {
                    double l = (lhs.type == VAL_FLOAT) ? lhs.as.f : (double)lhs.as.i;
                    double r = (rhs.type == VAL_FLOAT) ? rhs.as.f : (double)rhs.as.i;
                    vm_push_float(vm, l + r);
                } else if (lhs.type == VAL_PTR && rhs.type == VAL_INT) {
                    vm_push_ptr(vm, (char *)lhs.as.ptr + rhs.as.i);
                } else if (rhs.type == VAL_PTR && lhs.type == VAL_INT) {
                    vm_push_ptr(vm, (char *)rhs.as.ptr + lhs.as.i);
                } else {
                    vm_push_int(vm, lhs.as.i + rhs.as.i);
                }
                ip++;
                break;
            }

            case OP_SUB: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                if (lhs.type == VAL_FLOAT || rhs.type == VAL_FLOAT) {
                    double l = (lhs.type == VAL_FLOAT) ? lhs.as.f : (double)lhs.as.i;
                    double r = (rhs.type == VAL_FLOAT) ? rhs.as.f : (double)rhs.as.i;
                    vm_push_float(vm, l - r);
                } else if (lhs.type == VAL_PTR && rhs.type == VAL_INT) {
                    vm_push_ptr(vm, (char *)lhs.as.ptr - rhs.as.i);
                } else {
                    vm_push_int(vm, lhs.as.i - rhs.as.i);
                }
                ip++;
                break;
            }

            case OP_MUL: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                if (lhs.type == VAL_FLOAT || rhs.type == VAL_FLOAT) {
                    double l = (lhs.type == VAL_FLOAT) ? lhs.as.f : (double)lhs.as.i;
                    double r = (rhs.type == VAL_FLOAT) ? rhs.as.f : (double)rhs.as.i;
                    vm_push_float(vm, l * r);
                } else {
                    vm_push_int(vm, lhs.as.i * rhs.as.i);
                }
                ip++;
                break;
            }

            case OP_DIV: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                if (lhs.type == VAL_FLOAT || rhs.type == VAL_FLOAT) {
                    double l = (lhs.type == VAL_FLOAT) ? lhs.as.f : (double)lhs.as.i;
                    double r = (rhs.type == VAL_FLOAT) ? rhs.as.f : (double)rhs.as.i;
                    if (r == 0.0) {
                        vm_error(vm, "Division by zero");
                        return;
                    }
                    vm_push_float(vm, l / r);
                } else {
                    if (rhs.as.i == 0) {
                        vm_error(vm, "Division by zero");
                        return;
                    }
                    vm_push_int(vm, lhs.as.i / rhs.as.i);
                }
                ip++;
                break;
            }

            case OP_MOD: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                if (rhs.as.i == 0) {
                    vm_error(vm, "Division by zero in modulo");
                    return;
                }
                vm_push_int(vm, lhs.as.i % rhs.as.i);
                ip++;
                break;
            }

            case OP_INC: {
                Value v = vm_pop(vm);
                if (v.type == VAL_FLOAT) {
                    vm_push_float(vm, v.as.f + 1.0);
                } else {
                    vm_push_int(vm, v.as.i + 1);
                }
                ip++;
                break;
            }

            case OP_DEC: {
                Value v = vm_pop(vm);
                if (v.type == VAL_FLOAT) {
                    vm_push_float(vm, v.as.f - 1.0);
                } else {
                    vm_push_int(vm, v.as.i - 1);
                }
                ip++;
                break;
            }

            case OP_NEG: {
                Value v = vm_pop(vm);
                if (v.type == VAL_FLOAT) {
                    vm_push_float(vm, -v.as.f);
                } else {
                    vm_push_int(vm, -v.as.i);
                }
                ip++;
                break;
            }

            case OP_EQ: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                bool eq = false;
                if (lhs.type == VAL_FLOAT || rhs.type == VAL_FLOAT) {
                    double l = (lhs.type == VAL_FLOAT) ? lhs.as.f : (double)lhs.as.i;
                    double r = (rhs.type == VAL_FLOAT) ? rhs.as.f : (double)rhs.as.i;
                    eq = (l == r);
                } else {
                    eq = (lhs.as.i == rhs.as.i);
                }
                vm_push_int(vm, eq ? 1 : 0);
                ip++;
                break;
            }

            case OP_NEQ: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                bool neq = false;
                if (lhs.type == VAL_FLOAT || rhs.type == VAL_FLOAT) {
                    double l = (lhs.type == VAL_FLOAT) ? lhs.as.f : (double)lhs.as.i;
                    double r = (rhs.type == VAL_FLOAT) ? rhs.as.f : (double)rhs.as.i;
                    neq = (l != r);
                } else {
                    neq = (lhs.as.i != rhs.as.i);
                }
                vm_push_int(vm, neq ? 1 : 0);
                ip++;
                break;
            }

            case OP_LT: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                bool lt = false;
                if (lhs.type == VAL_FLOAT || rhs.type == VAL_FLOAT) {
                    double l = (lhs.type == VAL_FLOAT) ? lhs.as.f : (double)lhs.as.i;
                    double r = (rhs.type == VAL_FLOAT) ? rhs.as.f : (double)rhs.as.i;
                    lt = (l < r);
                } else {
                    lt = (lhs.as.i < rhs.as.i);
                }
                vm_push_int(vm, lt ? 1 : 0);
                ip++;
                break;
            }

            case OP_GT: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                bool gt = false;
                if (lhs.type == VAL_FLOAT || rhs.type == VAL_FLOAT) {
                    double l = (lhs.type == VAL_FLOAT) ? lhs.as.f : (double)lhs.as.i;
                    double r = (rhs.type == VAL_FLOAT) ? rhs.as.f : (double)rhs.as.i;
                    gt = (l > r);
                } else {
                    gt = (lhs.as.i > rhs.as.i);
                }
                vm_push_int(vm, gt ? 1 : 0);
                ip++;
                break;
            }

            case OP_LE: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                bool le = false;
                if (lhs.type == VAL_FLOAT || rhs.type == VAL_FLOAT) {
                    double l = (lhs.type == VAL_FLOAT) ? lhs.as.f : (double)lhs.as.i;
                    double r = (rhs.type == VAL_FLOAT) ? rhs.as.f : (double)rhs.as.i;
                    le = (l <= r);
                } else {
                    le = (lhs.as.i <= rhs.as.i);
                }
                vm_push_int(vm, le ? 1 : 0);
                ip++;
                break;
            }

            case OP_GE: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                bool ge = false;
                if (lhs.type == VAL_FLOAT || rhs.type == VAL_FLOAT) {
                    double l = (lhs.type == VAL_FLOAT) ? lhs.as.f : (double)lhs.as.i;
                    double r = (rhs.type == VAL_FLOAT) ? rhs.as.f : (double)rhs.as.i;
                    ge = (l >= r);
                } else {
                    ge = (lhs.as.i >= rhs.as.i);
                }
                vm_push_int(vm, ge ? 1 : 0);
                ip++;
                break;
            }

            case OP_AND: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                vm_push_int(vm, lhs.as.i & rhs.as.i);
                ip++;
                break;
            }

            case OP_OR: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                vm_push_int(vm, lhs.as.i | rhs.as.i);
                ip++;
                break;
            }

            case OP_XOR: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                vm_push_int(vm, lhs.as.i ^ rhs.as.i);
                ip++;
                break;
            }

            case OP_NOT: {
                Value v = vm_pop(vm);
                vm_push_int(vm, val_is_truthy(v) ? 0 : 1);
                ip++;
                break;
            }

            case OP_SHL: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                vm_push_int(vm, lhs.as.i << rhs.as.i);
                ip++;
                break;
            }

            case OP_SHR: {
                Value lhs = vm_pop(vm);
                Value rhs = vm_pop(vm);
                vm_push_int(vm, (uint64_t)lhs.as.i >> rhs.as.i);
                ip++;
                break;
            }

            case OP_PRINT: {
                Value v = vm_pop(vm);
                switch (v.type) {
                    case VAL_INT:
                        fprintf(vm->out_stream, "%lld\n", (long long)v.as.i);
                        break;
                    case VAL_FLOAT:
                        fprintf(vm->out_stream, "%g\n", v.as.f);
                        break;
                    case VAL_STR:
                        fprintf(vm->out_stream, "%s\n", v.as.str);
                        break;
                    case VAL_PTR:
                        fprintf(vm->out_stream, "%p\n", v.as.ptr);
                        break;
                }
                fflush(vm->out_stream);
                ip++;
                break;
            }

            case OP_FETCH: {
                Value var_addr = vm_pop(vm);
                if (!var_addr.as.ptr) {
                    vm_error(vm, "Null pointer dereference on @");
                    return;
                }
                uint64_t raw = *(uint64_t *)(var_addr.as.ptr);
                vm_push_int(vm, (int64_t)raw);
                ip++;
                break;
            }

            case OP_STORE: {
                Value var_addr = vm_pop(vm); // pops <var> first
                Value val = vm_pop(vm);      // pops <val> second
                if (!var_addr.as.ptr) {
                    vm_error(vm, "Null pointer dereference on !");
                    return;
                }
                *(uint64_t *)(var_addr.as.ptr) = (uint64_t)val.as.u;
                ip++;
                break;
            }

            case OP_C_FETCH8: {
                Value addr = vm_pop(vm);
                if (!addr.as.ptr) {
                    vm_error(vm, "Null pointer dereference on c@8");
                    return;
                }
                uint8_t val = *(uint8_t *)(addr.as.ptr);
                vm_push_int(vm, val);
                ip++;
                break;
            }

            case OP_C_FETCH32: {
                Value addr = vm_pop(vm);
                if (!addr.as.ptr) {
                    vm_error(vm, "Null pointer dereference on c@32");
                    return;
                }
                int32_t val = *(int32_t *)(addr.as.ptr);
                vm_push_int(vm, val);
                ip++;
                break;
            }

            case OP_C_FETCH64: {
                Value addr = vm_pop(vm);
                if (!addr.as.ptr) {
                    vm_error(vm, "Null pointer dereference on c@64");
                    return;
                }
                int64_t val = *(int64_t *)(addr.as.ptr);
                vm_push_int(vm, val);
                ip++;
                break;
            }

            case OP_C_STORE8: {
                Value addr = vm_pop(vm); // addr first
                Value val = vm_pop(vm);  // val second
                if (!addr.as.ptr) {
                    vm_error(vm, "Null pointer dereference on c!8");
                    return;
                }
                *(uint8_t *)(addr.as.ptr) = (uint8_t)val.as.i;
                ip++;
                break;
            }

            case OP_C_STORE32: {
                Value addr = vm_pop(vm); // addr first
                Value val = vm_pop(vm);  // val second
                if (!addr.as.ptr) {
                    vm_error(vm, "Null pointer dereference on c!32");
                    return;
                }
                *(int32_t *)(addr.as.ptr) = (int32_t)val.as.i;
                ip++;
                break;
            }

            case OP_C_STORE64: {
                Value addr = vm_pop(vm); // addr first
                Value val = vm_pop(vm);  // val second
                if (!addr.as.ptr) {
                    vm_error(vm, "Null pointer dereference on c!64");
                    return;
                }
                *(int64_t *)(addr.as.ptr) = (int64_t)val.as.i;
                ip++;
                break;
            }

            case OP_CSTR: {
                Value addr = vm_pop(vm);
                const char *s = (const char *)addr.as.ptr;
                vm_push_str(vm, s ? s : "");
                ip++;
                break;
            }

            case OP_FFI_CALL: {
                int word_idx = (int)inst.arg;
                if (word_idx < 0 || (size_t)word_idx >= vm->word_count) {
                    vm_error(vm, "Invalid extern word index in FFI call: %d", word_idx);
                    return;
                }
                Word *w = vm->words[word_idx];
                ffi_call_extern(vm, w->extern_fn);
                ip++;
                break;
            }
        }
    }
}
