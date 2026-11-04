#include "preforth.h"

static VM *g_wasm_vm = NULL;
static char g_wasm_output[131072];

#if defined(_WIN32)
#define WASM_EXPORT __declspec(dllexport)
#else
#define WASM_EXPORT __attribute__((visibility("default")))
#endif

WASM_EXPORT void preforth_wasm_init(void) {
    if (g_wasm_vm) {
        vm_free(g_wasm_vm);
    }
    g_wasm_vm = vm_create();
    g_wasm_vm->custom_output_buf = g_wasm_output;
    g_wasm_vm->custom_output_cap = sizeof(g_wasm_output);
    g_wasm_vm->custom_output_len = 0;
    g_wasm_output[0] = '\0';
}

WASM_EXPORT const char *preforth_wasm_eval(const char *code) {
    if (!g_wasm_vm) {
        preforth_wasm_init();
    }
    g_wasm_vm->custom_output_len = 0;
    g_wasm_output[0] = '\0';
    g_wasm_vm->has_error = false;

    int old_sp = g_wasm_vm->sp;
    vm_interpret(g_wasm_vm, code);

    // If no explicit print was done but items were pushed onto the stack, show stack state
    if (g_wasm_output[0] == '\0' && !g_wasm_vm->has_error && g_wasm_vm->sp > old_sp) {
        char stack_summary[512] = "ok [ ";
        for (int i = 0; i < g_wasm_vm->sp; i++) {
            char item[64];
            Value v = g_wasm_vm->data_stack[i];
            if (v.type == VAL_INT) {
                snprintf(item, sizeof(item), "%lld ", (long long)v.as.i);
            } else if (v.type == VAL_FLOAT) {
                snprintf(item, sizeof(item), "%g ", v.as.f);
            } else if (v.type == VAL_STR) {
                snprintf(item, sizeof(item), "\"%s\" ", v.as.str);
            } else {
                snprintf(item, sizeof(item), "%p ", v.as.ptr);
            }
            strncat(stack_summary, item, sizeof(stack_summary) - strlen(stack_summary) - 1);
        }
        strncat(stack_summary, "]\n", sizeof(stack_summary) - strlen(stack_summary) - 1);
        strncpy(g_wasm_output, stack_summary, sizeof(g_wasm_output) - 1);
    }

    return g_wasm_output;
}

WASM_EXPORT void preforth_wasm_reset(void) {
    preforth_wasm_init();
}

WASM_EXPORT int preforth_wasm_stack_depth(void) {
    return g_wasm_vm ? g_wasm_vm->sp : 0;
}
