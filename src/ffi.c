#include "preforth.h"

typedef struct {
    const char *name;
    void *fn_ptr;
} BuiltinSymbol;

static const BuiltinSymbol builtin_symbols[] = {
    {"malloc",   (void *)malloc},
    {"free",     (void *)free},
    {"realloc",  (void *)realloc},
    {"calloc",   (void *)calloc},
    {"snprintf", (void *)snprintf},
    {"sprintf",  (void *)sprintf},
    {"printf",   (void *)printf},
    {"puts",     (void *)puts},
    {"putchar",  (void *)putchar},
    {"qsort",    (void *)qsort},
    {"bsearch",  (void *)bsearch},
    {"sin",      (void *)sin},
    {"cos",      (void *)cos},
    {"tan",      (void *)tan},
    {"sqrt",     (void *)sqrt},
    {"pow",      (void *)pow},
    {"exp",      (void *)exp},
    {"log",      (void *)log},
    {"floor",    (void *)floor},
    {"ceil",     (void *)ceil},
    {"fabs",     (void *)fabs},
    {"strlen",   (void *)strlen},
    {"strcmp",   (void *)strcmp},
    {"strncmp",  (void *)strncmp},
    {"strcpy",   (void *)strcpy},
    {"strncpy",  (void *)strncpy},
    {"memcpy",   (void *)memcpy},
    {"memset",   (void *)memset},
    {"memmove",  (void *)memmove},
    {"atoi",     (void *)atoi},
    {"atol",     (void *)atol},
    {"exit",     (void *)exit},
    {NULL,       NULL}
};

static CallbackEntry g_callbacks[PF_MAX_CALLBACKS];

uint64_t pf_callback_dispatch(int idx, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    if (idx < 0 || idx >= PF_MAX_CALLBACKS || !g_callbacks[idx].in_use) {
        return 0;
    }
    CallbackEntry *entry = &g_callbacks[idx];
    VM *vm = entry->vm;
    uint64_t raw_args[6] = {a0, a1, a2, a3, a4, a5};

    // Push incoming C arguments onto PreForth stack
    for (int i = 0; i < entry->arg_count; i++) {
        FFIType t = entry->arg_types[i];
        Value v;
        memset(&v, 0, sizeof(v));
        switch (t) {
            case FFI_PTR:
                v = val_ptr((void *)raw_args[i]);
                break;
            case FFI_STR:
                v = val_str((const char *)raw_args[i]);
                break;
            case FFI_I32:
            case FFI_U32:
            case FFI_I64:
            case FFI_U64:
                v = val_int((int64_t)raw_args[i]);
                break;
            case FFI_F64: {
                double f;
                memcpy(&f, &raw_args[i], sizeof(double));
                v = val_float(f);
                break;
            }
            default:
                v = val_int((int64_t)raw_args[i]);
                break;
        }
        vm_push(vm, v);
    }

    // Execute the PreForth word
    Word *w = vm->words[entry->word_idx];
    if (w->kind == WORD_BYTECODE) {
        vm_execute_block(vm, w->bytecode);
    } else if (w->kind == WORD_PRIMITIVE) {
        w->primitive_fn(vm);
    }

    // Pops and returns the result to C if ret_type is non-void
    if (entry->ret_type != FFI_VOID) {
        Value ret_val = vm_pop(vm);
        if (entry->ret_type == FFI_F64) {
            double f = (ret_val.type == VAL_FLOAT) ? ret_val.as.f : (double)ret_val.as.i;
            uint64_t raw;
            memcpy(&raw, &f, sizeof(uint64_t));
            return raw;
        } else if (entry->ret_type == FFI_PTR) {
            return (uint64_t)ret_val.as.ptr;
        } else if (entry->ret_type == FFI_STR) {
            return (uint64_t)ret_val.as.str;
        } else {
            return (uint64_t)ret_val.as.i;
        }
    }

    return 0;
}

#define CB_DISP(n) \
    static uint64_t cb_trampoline_##n(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) { \
        return pf_callback_dispatch(n, a0, a1, a2, a3, a4, a5); \
    }

CB_DISP(0)  CB_DISP(1)  CB_DISP(2)  CB_DISP(3)  CB_DISP(4)  CB_DISP(5)  CB_DISP(6)  CB_DISP(7)
CB_DISP(8)  CB_DISP(9)  CB_DISP(10) CB_DISP(11) CB_DISP(12) CB_DISP(13) CB_DISP(14) CB_DISP(15)
CB_DISP(16) CB_DISP(17) CB_DISP(18) CB_DISP(19) CB_DISP(20) CB_DISP(21) CB_DISP(22) CB_DISP(23)
CB_DISP(24) CB_DISP(25) CB_DISP(26) CB_DISP(27) CB_DISP(28) CB_DISP(29) CB_DISP(30) CB_DISP(31)
CB_DISP(32) CB_DISP(33) CB_DISP(34) CB_DISP(35) CB_DISP(36) CB_DISP(37) CB_DISP(38) CB_DISP(39)
CB_DISP(40) CB_DISP(41) CB_DISP(42) CB_DISP(43) CB_DISP(44) CB_DISP(45) CB_DISP(46) CB_DISP(47)
CB_DISP(48) CB_DISP(49) CB_DISP(50) CB_DISP(51) CB_DISP(52) CB_DISP(53) CB_DISP(54) CB_DISP(55)
CB_DISP(56) CB_DISP(57) CB_DISP(58) CB_DISP(59) CB_DISP(60) CB_DISP(61) CB_DISP(62) CB_DISP(63)

typedef uint64_t (*trampoline_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

static const trampoline_fn cb_trampolines[PF_MAX_CALLBACKS] = {
    cb_trampoline_0,  cb_trampoline_1,  cb_trampoline_2,  cb_trampoline_3,
    cb_trampoline_4,  cb_trampoline_5,  cb_trampoline_6,  cb_trampoline_7,
    cb_trampoline_8,  cb_trampoline_9,  cb_trampoline_10, cb_trampoline_11,
    cb_trampoline_12, cb_trampoline_13, cb_trampoline_14, cb_trampoline_15,
    cb_trampoline_16, cb_trampoline_17, cb_trampoline_18, cb_trampoline_19,
    cb_trampoline_20, cb_trampoline_21, cb_trampoline_22, cb_trampoline_23,
    cb_trampoline_24, cb_trampoline_25, cb_trampoline_26, cb_trampoline_27,
    cb_trampoline_28, cb_trampoline_29, cb_trampoline_30, cb_trampoline_31,
    cb_trampoline_32, cb_trampoline_33, cb_trampoline_34, cb_trampoline_35,
    cb_trampoline_36, cb_trampoline_37, cb_trampoline_38, cb_trampoline_39,
    cb_trampoline_40, cb_trampoline_41, cb_trampoline_42, cb_trampoline_43,
    cb_trampoline_44, cb_trampoline_45, cb_trampoline_46, cb_trampoline_47,
    cb_trampoline_48, cb_trampoline_49, cb_trampoline_50, cb_trampoline_51,
    cb_trampoline_52, cb_trampoline_53, cb_trampoline_54, cb_trampoline_55,
    cb_trampoline_56, cb_trampoline_57, cb_trampoline_58, cb_trampoline_59,
    cb_trampoline_60, cb_trampoline_61, cb_trampoline_62, cb_trampoline_63
};

void ffi_init(VM *vm) {
    vm->lib_count = 0;
    vm->lib_capacity = 8;
    vm->libs = (void **)calloc(vm->lib_capacity, sizeof(void *));
    vm->lib_names = (char **)calloc(vm->lib_capacity, sizeof(char *));

    // Phase 5.1: Pre-load libc and libm by default.
    ffi_load_library(vm, "libc", "libc");
    ffi_load_library(vm, "libm", "libm");
}

void ffi_cleanup(VM *vm) {
    for (size_t i = 0; i < vm->lib_count; i++) {
        if (vm->libs[i]) {
#if defined(PREFORTH_WINDOWS)
            FreeLibrary((HMODULE)vm->libs[i]);
#elif defined(PREFORTH_WASM)
            /* no-op on wasm */
#else
            dlclose(vm->libs[i]);
#endif
        }
        free(vm->lib_names[i]);
    }
    free(vm->libs);
    free(vm->lib_names);

    for (int i = 0; i < PF_MAX_CALLBACKS; i++) {
        if (g_callbacks[i].in_use && g_callbacks[i].vm == vm) {
            free(g_callbacks[i].arg_types);
            g_callbacks[i].in_use = false;
        }
    }
}

bool ffi_load_library(VM *vm, const char *handle, const char *lib_name) {
    void *lib_ptr = NULL;

    // Check if already loaded
    for (size_t i = 0; i < vm->lib_count; i++) {
        if (strcmp(vm->lib_names[i], handle) == 0) {
            return true;
        }
    }

#if defined(PREFORTH_WINDOWS)
    if (strcmp(lib_name, "libc") == 0 || strcmp(lib_name, "libm") == 0) {
        lib_ptr = (void *)GetModuleHandleA("msvcrt.dll");
        if (!lib_ptr) {
            lib_ptr = (void *)LoadLibraryA("msvcrt.dll");
        }
        if (!lib_ptr) {
            lib_ptr = (void *)GetModuleHandleA(NULL);
        }
    } else {
        lib_ptr = (void *)LoadLibraryA(lib_name);
    }
#elif defined(PREFORTH_WASM)
    lib_ptr = (void *)1;
#else
    if (strcmp(lib_name, "libc") == 0 || strcmp(lib_name, "libm") == 0) {
        lib_ptr = dlopen(NULL, RTLD_LAZY | RTLD_GLOBAL);
    } else {
        lib_ptr = dlopen(lib_name, RTLD_LAZY | RTLD_GLOBAL);
    }
#endif

    if (vm->lib_count >= vm->lib_capacity) {
        vm->lib_capacity *= 2;
        vm->libs = (void **)realloc(vm->libs, vm->lib_capacity * sizeof(void *));
        vm->lib_names = (char **)realloc(vm->lib_names, vm->lib_capacity * sizeof(char *));
    }

    vm->libs[vm->lib_count] = lib_ptr;
    vm->lib_names[vm->lib_count] = strdup(handle);
    vm->lib_count++;

    return true;
}

void *ffi_find_symbol(VM *vm, const char *lib_handle, const char *symbol_name) {
    // First, check builtin symbols table
    for (size_t i = 0; builtin_symbols[i].name != NULL; i++) {
        if (strcmp(builtin_symbols[i].name, symbol_name) == 0) {
            return builtin_symbols[i].fn_ptr;
        }
    }

    // Lookup library by handle
    void *lib_ptr = NULL;
    for (size_t i = 0; i < vm->lib_count; i++) {
        if (strcmp(vm->lib_names[i], lib_handle) == 0) {
            lib_ptr = vm->libs[i];
            break;
        }
    }

    void *sym = NULL;
    if (lib_ptr) {
#if defined(PREFORTH_WINDOWS)
        sym = (void *)GetProcAddress((HMODULE)lib_ptr, symbol_name);
#elif defined(PREFORTH_WASM)
        sym = NULL;
#else
        sym = dlsym(lib_ptr, symbol_name);
#endif
    }

#if defined(PREFORTH_WINDOWS)
    if (!sym) {
        HMODULE h_main = GetModuleHandleA(NULL);
        if (h_main) sym = (void *)GetProcAddress(h_main, symbol_name);
    }
#elif defined(PREFORTH_WASM)
    /* on wasm, symbols come from builtin_symbols */
#else
    if (!sym) {
        sym = dlsym(RTLD_DEFAULT, symbol_name);
    }
#endif

    return sym;
}

static uint64_t call_raw(void *fn, int count, const uint64_t *a) {
    switch (count) {
        case 0: return ((uint64_t (*)(void))fn)();
        case 1: return ((uint64_t (*)(uint64_t))fn)(a[0]);
        case 2: return ((uint64_t (*)(uint64_t, uint64_t))fn)(a[0], a[1]);
        case 3: return ((uint64_t (*)(uint64_t, uint64_t, uint64_t))fn)(a[0], a[1], a[2]);
        case 4: return ((uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t))fn)(a[0], a[1], a[2], a[3]);
        case 5: return ((uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))fn)(a[0], a[1], a[2], a[3], a[4]);
        case 6: return ((uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))fn)(a[0], a[1], a[2], a[3], a[4], a[5]);
        case 7: return ((uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
        case 8: return ((uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
        default: return 0;
    }
}

static double call_raw_f64(void *fn, int count, const uint64_t *a, const bool *is_float) {
    if (count == 0) {
        return ((double (*)(void))fn)();
    } else if (count == 1) {
        if (is_float[0]) {
            double d; memcpy(&d, &a[0], sizeof(double));
            return ((double (*)(double))fn)(d);
        } else {
            return ((double (*)(uint64_t))fn)(a[0]);
        }
    } else if (count == 2) {
        if (is_float[0] && is_float[1]) {
            double d0, d1;
            memcpy(&d0, &a[0], sizeof(double));
            memcpy(&d1, &a[1], sizeof(double));
            return ((double (*)(double, double))fn)(d0, d1);
        } else if (is_float[0]) {
            double d0; memcpy(&d0, &a[0], sizeof(double));
            return ((double (*)(double, uint64_t))fn)(d0, a[1]);
        } else if (is_float[1]) {
            double d1; memcpy(&d1, &a[1], sizeof(double));
            return ((double (*)(uint64_t, double))fn)(a[0], d1);
        } else {
            return ((double (*)(uint64_t, uint64_t))fn)(a[0], a[1]);
        }
    }
    return 0.0;
}

void ffi_call_extern(VM *vm, ExternFunc *ef) {
    if (!ef || !ef->fn_ptr) {
        vm_error(vm, "Cannot call NULL extern function pointer");
        return;
    }

    uint64_t raw_args[16];
    bool is_float[16];
    memset(raw_args, 0, sizeof(raw_args));
    memset(is_float, 0, sizeof(is_float));

    int total_args = 0;

    // Phase 5.2: 1-to-1 Stack Popping Alignment
    // Simply call pop() N times to populate (arg0, arg1, ..., argN-1) in exact left-to-right C parameter order
    for (int i = 0; i < ef->arg_count && total_args < 16; i++) {
        Value val = vm_pop(vm);
        FFIType t = ef->arg_types[i];
        switch (t) {
            case FFI_PTR:
                raw_args[total_args] = (uint64_t)val.as.ptr;
                break;
            case FFI_STR:
                raw_args[total_args] = (val.type == VAL_STR) ? (uint64_t)val.as.str : (uint64_t)val.as.ptr;
                break;
            case FFI_F64: {
                double f = (val.type == VAL_FLOAT) ? val.as.f : (double)val.as.i;
                memcpy(&raw_args[total_args], &f, sizeof(double));
                is_float[total_args] = true;
                break;
            }
            case FFI_I32:
            case FFI_U32:
            case FFI_I64:
            case FFI_U64:
            default:
                raw_args[total_args] = (uint64_t)val.as.i;
                break;
        }
        total_args++;
    }

    // Phase 5.3: Variadic C Function Support
    if (ef->is_variadic) {
        Value vcount = vm_pop(vm);
        int var_count = (int)vcount.as.i;
        for (int i = 0; i < var_count && total_args < 16; i++) {
            Value val = vm_pop(vm);
            switch (val.type) {
                case VAL_FLOAT: {
                    double f = val.as.f;
                    memcpy(&raw_args[total_args], &f, sizeof(double));
                    is_float[total_args] = true;
                    break;
                }
                case VAL_STR:
                    raw_args[total_args] = (uint64_t)val.as.str;
                    break;
                case VAL_PTR:
                    raw_args[total_args] = (uint64_t)val.as.ptr;
                    break;
                case VAL_INT:
                default:
                    raw_args[total_args] = (uint64_t)val.as.i;
                    break;
            }
            total_args++;
        }
    }

    // Execute call and marshal return value
    if (ef->ret_type == FFI_F64) {
        double fres = call_raw_f64(ef->fn_ptr, total_args, raw_args, is_float);
        vm_push_float(vm, fres);
    } else {
        uint64_t ures = call_raw(ef->fn_ptr, total_args, raw_args);
        switch (ef->ret_type) {
            case FFI_VOID:
                break;
            case FFI_I32:
                vm_push_int(vm, (int32_t)ures);
                break;
            case FFI_U32:
                vm_push_int(vm, (uint32_t)ures);
                break;
            case FFI_I64:
            case FFI_U64:
                vm_push_int(vm, (int64_t)ures);
                break;
            case FFI_PTR:
                vm_push_ptr(vm, (void *)ures);
                break;
            case FFI_STR:
                vm_push_str(vm, (const char *)ures);
                break;
            default:
                vm_push_int(vm, (int64_t)ures);
                break;
        }
    }
}

void *ffi_create_callback(VM *vm, int word_idx, FFIType ret_type, FFIType *arg_types, int arg_count) {
    int slot = -1;
    for (int i = 0; i < PF_MAX_CALLBACKS; i++) {
        if (!g_callbacks[i].in_use) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        vm_error(vm, "Exceeded maximum number of FFI callbacks (%d)", PF_MAX_CALLBACKS);
        return NULL;
    }

    CallbackEntry *entry = &g_callbacks[slot];
    entry->vm = vm;
    entry->word_idx = word_idx;
    entry->ret_type = ret_type;
    entry->arg_count = arg_count;
    entry->arg_types = (FFIType *)malloc(arg_count * sizeof(FFIType));
    memcpy(entry->arg_types, arg_types, arg_count * sizeof(FFIType));
    entry->in_use = true;

    return (void *)cb_trampolines[slot];
}
