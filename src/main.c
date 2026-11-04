#include "preforth.h"

static bool run_test_1(void) {
    printf("[*] Running Test 1 (Non-Commutative Prefix Arithmetic)...\n");
    VM *vm = vm_create();

    vm_interpret(vm, ". - 10 * 2 3");
    if (vm->has_error) {
        printf("[-] Test 1.1 failed with error: %s\n", vm->error_msg);
        vm_free(vm);
        return false;
    }

    vm_interpret(vm, ". / 20 + 2 3");
    if (vm->has_error) {
        printf("[-] Test 1.2 failed with error: %s\n", vm->error_msg);
        vm_free(vm);
        return false;
    }

    // Verify raw values on stack
    vm_interpret(vm, "- 10 * 2 3");
    Value v1 = vm_pop(vm);
    vm_interpret(vm, "/ 20 + 2 3");
    Value v2 = vm_pop(vm);

    vm_free(vm);

    if (v1.as.i == 4 && v2.as.i == 4) {
        printf("[+] Test 1 PASSED: 10 - (2 * 3) = 4, 20 / (2 + 3) = 4\n\n");
        return true;
    } else {
        printf("[-] Test 1 FAILED: expected 4 and 4, got %lld and %lld\n\n", (long long)v1.as.i, (long long)v2.as.i);
        return false;
    }
}

static bool run_test_2(void) {
    printf("[*] Running Test 2 (Recursive Word Definition)...\n");
    VM *vm = vm_create();

    const char *code =
        "def fact if == dup 0 then + 1 drop else * fact dec dup end ;\n"
        "fact 5\n";

    vm_interpret(vm, code);
    if (vm->has_error) {
        printf("[-] Test 2 failed with error: %s\n", vm->error_msg);
        vm_free(vm);
        return false;
    }

    Value res = vm_pop(vm);
    vm_free(vm);

    if (res.as.i == 120) {
        printf("[+] Test 2 PASSED: fact(5) = 120\n\n");
        return true;
    } else {
        printf("[-] Test 2 FAILED: expected 120, got %lld\n\n", (long long)res.as.i);
        return false;
    }
}

static bool run_test_3(void) {
    printf("[*] Running Test 3 (Forward & Backward goto State Machine)...\n");
    VM *vm = vm_create();

    const char *code =
        "var n\n"
        "var steps\n"
        "def collatz_steps\n"
        "    ! steps 0\n"
        "    ! n\n"
        "loop:\n"
        "    ?goto done == @ n 1\n"
        "    ! steps inc @ steps\n"
        "    ?goto is_even == % @ n 2 0\n"
        "    ! n + * 3 @ n 1\n"
        "    goto loop\n"
        "is_even:\n"
        "    ! n / @ n 2\n"
        "    goto loop\n"
        "done:\n"
        "    @ steps\n"
        ";\n"
        "collatz_steps 27\n";

    vm_interpret(vm, code);
    if (vm->has_error) {
        printf("[-] Test 3 failed with error: %s\n", vm->error_msg);
        vm_free(vm);
        return false;
    }

    Value res = vm_pop(vm);
    int max_depth = vm->max_call_depth;
    vm_free(vm);

    // Call depth when entering collatz_steps is 2 (top_level -> collatz_steps),
    // and must not grow during the 111 goto loop iterations!
    if (res.as.i == 111 && max_depth <= 2) {
        printf("[+] Test 3 PASSED: collatz_steps(27) = 111, max call depth = %d (constant stack)\n\n", max_depth);
        return true;
    } else {
        printf("[-] Test 3 FAILED: expected 111 with constant call stack, got %lld (max depth %d)\n\n",
               (long long)res.as.i, max_depth);
        return false;
    }
}

static bool run_test_4(void) {
    printf("[*] Running Test 4 (Variadic C ABI & Heap Memory)...\n");
    VM *vm = vm_create();

    const char *code =
        "extern libc ptr malloc ( u64 )\n"
        "extern libc void free ( ptr )\n"
        "extern libc i32 snprintf ( ptr u64 str ... )\n"
        "var buf\n"
        "! buf malloc 128\n"
        "snprintf @ buf 128 \"Val: %d\" 1 42\n"
        "drop\n" // drop snprintf return value
        "cstr @ buf\n"
        "free @ buf\n";

    vm_interpret(vm, code);
    if (vm->has_error) {
        printf("[-] Test 4 failed with error: %s\n", vm->error_msg);
        vm_free(vm);
        return false;
    }

    Value res = vm_pop(vm);
    bool pass = (res.type == VAL_STR && strcmp(res.as.str, "Val: 42") == 0);
    if (pass) {
        printf("[+] Test 4 PASSED: buffer formatted via snprintf, read back as \"%s\"\n\n", res.as.str);
    } else {
        printf("[-] Test 4 FAILED: expected \"Val: 42\", got \"%s\"\n\n", res.type == VAL_STR ? res.as.str : "<non-string>");
    }

    if (res.type == VAL_STR) free(res.as.str);
    vm_free(vm);
    return pass;
}

static bool run_test_5(void) {
    printf("[*] Running Test 5 (C Callback Interop with qsort)...\n");
    VM *vm = vm_create();

    const char *code =
        "extern libc ptr malloc ( u64 )\n"
        "extern libc void free ( ptr )\n"
        "extern libc void qsort ( ptr u64 u64 ptr )\n"
        "def cmp_i32 - c@32 swap c@32 ;\n"
        "var arr\n"
        "! arr malloc 16\n"
        "c!32 + @ arr 0 40\n"
        "c!32 + @ arr 4 10\n"
        "c!32 + @ arr 8 30\n"
        "c!32 + @ arr 12 20\n"
        "var cmp_cb\n"
        "! cmp_cb cb i32 cmp_i32 ( ptr ptr )\n"
        "qsort @ arr 4 4 @ cmp_cb\n"
        "c@32 + @ arr 0\n"
        "c@32 + @ arr 4\n"
        "c@32 + @ arr 8\n"
        "c@32 + @ arr 12\n"
        "free @ arr\n";

    vm_interpret(vm, code);
    if (vm->has_error) {
        printf("[-] Test 5 failed with error: %s\n", vm->error_msg);
        vm_free(vm);
        return false;
    }

    Value v3 = vm_pop(vm);
    Value v2 = vm_pop(vm);
    Value v1 = vm_pop(vm);
    Value v0 = vm_pop(vm);

    vm_free(vm);

    bool pass = (v0.as.i == 10 && v1.as.i == 20 && v2.as.i == 30 && v3.as.i == 40);
    if (pass) {
        printf("[+] Test 5 PASSED: [40, 10, 30, 20] sorted to [%lld, %lld, %lld, %lld]\n\n",
               (long long)v0.as.i, (long long)v1.as.i, (long long)v2.as.i, (long long)v3.as.i);
    } else {
        printf("[-] Test 5 FAILED: expected [10, 20, 30, 40], got [%lld, %lld, %lld, %lld]\n\n",
               (long long)v0.as.i, (long long)v1.as.i, (long long)v2.as.i, (long long)v3.as.i);
    }
    return pass;
}

static bool run_test_6(void) {
    printf("[*] Running Test 6 (Phase 7: Shared Library Round-Trip)...\n");

    const char *shared_lib_name = "dist/libtest"
#if defined(_WIN32) || defined(_WIN64)
        ".dll";
#elif defined(__APPLE__)
        ".dylib";
#else
        ".so";
#endif

    // Step 1: In VM 1, compile .pf code exporting pf_factorial and importing libc's abs
    VM *vm1 = vm_create();
    char script1[1024];
    snprintf(script1, sizeof(script1),
        "import i64 abs ( i64 )\n"
        "def fact\n"
        "  if == dup 0 then\n"
        "    + 1 drop\n"
        "  else\n"
        "    * fact dec dup\n"
        "  end ;\n"
        "def pf_factorial\n"
        "  fact abs ;\n"
        "export i64 pf_factorial as \"pf_factorial\" ( i64 )\n"
        "link-lib \"c\"\n"
        "build-shared \"%s\"\n",
        shared_lib_name
    );

    vm_interpret(vm1, script1);
    if (vm1->has_error) {
        printf("[-] Test 6: failed to build shared library: %s\n", vm1->error_msg);
        vm_free(vm1);
        return false;
    }
    vm_free(vm1);

    // Step 2: In a separate PreForth VM instance (VM 2), load via lib + extern and invoke pf_factorial 10
    VM *vm2 = vm_create();
    char script2[1024];
    snprintf(script2, sizeof(script2),
        "lib testlib \"%s\"\n"
        "extern testlib i64 pf_factorial ( i64 )\n"
        "pf_factorial 10\n",
        shared_lib_name
    );

    vm_interpret(vm2, script2);
    if (vm2->has_error) {
        printf("[-] Test 6: failed to invoke shared library: %s\n", vm2->error_msg);
        vm_free(vm2);
        return false;
    }

    Value res = vm_pop(vm2);
    vm_free(vm2);

    bool pass = (res.as.i == 3628800);
    if (pass) {
        printf("[+] Test 6 PASSED: pf_factorial(10) via round-trip shared library = %lld (3628800)\n\n", (long long)res.as.i);
    } else {
        printf("[-] Test 6 FAILED: expected 3628800, got %lld\n\n", (long long)res.as.i);
    }

    return pass;
}

static void run_all_tests(void) {
    printf("============================================================\n");
    printf("           PreForth Verification Test Suite (Phases 1-7)    \n");
    printf("============================================================\n\n");

    int passed = 0;
    if (run_test_1()) passed++;
    if (run_test_2()) passed++;
    if (run_test_3()) passed++;
    if (run_test_4()) passed++;
    if (run_test_5()) passed++;
    if (run_test_6()) passed++;

    printf("============================================================\n");
    printf("Test Results: %d / 6 verification suites passed.\n", passed);
    if (passed == 6) {
        printf("ALL TESTS PASSED SUCCESSFULLY!\n");
    } else {
        printf("SOME TESTS FAILED!\n");
    }
    printf("============================================================\n");
    if (passed != 6) {
        exit(1);
    }
}

static void run_repl(void) {
    printf("PreForth v1.0.0 (Prefix Concatenative Language)\n");
    printf("Type 'exit' or Ctrl+C to quit.\n\n");

    VM *vm = vm_create();
    char line[1024];

    while (1) {
        printf("preforth> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        if (strcmp(line, "exit\n") == 0 || strcmp(line, "quit\n") == 0) break;
        vm->has_error = false;
        vm_interpret(vm, line);
    }

    vm_free(vm);
}

static void run_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: could not open file '%s'\n", filename);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    VM *vm = vm_create();
    vm_interpret(vm, buf);
    free(buf);

    if (vm->has_error) {
        vm_free(vm);
        exit(1);
    }
    vm_free(vm);
}

int main(int argc, char **argv) {
    if (argc == 1) {
        run_repl();
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0 || strcmp(argv[i], "-t") == 0) {
            run_all_tests();
            return 0;
        } else if (strcmp(argv[i], "-e") == 0) {
            if (i + 1 < argc) {
                VM *vm = vm_create();
                vm_interpret(vm, argv[i + 1]);
                bool err = vm->has_error;
                vm_free(vm);
                return err ? 1 : 0;
            } else {
                fprintf(stderr, "Error: -e requires an expression argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: preforth [options] [file.pf]\n");
            printf("Options:\n");
            printf("  --test, -t                  Run all Phase 1-7 verification test suites\n");
            printf("  -e <expr>                   Evaluate PreForth expression\n");
            printf("  --emit-shared <out> <src>   Compile source file to native shared library (.so/.dll/.dylib)\n");
            printf("  --emit-obj <out> <src>      Compile source file to native object file (.o/.obj)\n");
            printf("  --emit-c <out> <src>        Transpile source file to C99 source code\n");
            printf("  --help, -h                  Show this help message\n");
            return 0;
        } else if (strcmp(argv[i], "--emit-shared") == 0) {
            if (i + 2 < argc) {
                const char *out_file = argv[i + 1];
                const char *src_file = argv[i + 2];
                VM *vm = vm_create();
                FILE *f = fopen(src_file, "rb");
                if (!f) { fprintf(stderr, "Could not open '%s'\n", src_file); return 1; }
                fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
                char *buf = (char *)malloc(sz + 1); fread(buf, 1, sz, f); buf[sz] = '\0'; fclose(f);
                vm_interpret(vm, buf); free(buf);
                bool ok = aot_build_shared(vm, out_file);
                vm_free(vm);
                return ok ? 0 : 1;
            } else {
                fprintf(stderr, "Error: --emit-shared requires <out_file> and <src_file>\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--emit-obj") == 0) {
            if (i + 2 < argc) {
                const char *out_file = argv[i + 1];
                const char *src_file = argv[i + 2];
                VM *vm = vm_create();
                FILE *f = fopen(src_file, "rb");
                if (!f) { fprintf(stderr, "Could not open '%s'\n", src_file); return 1; }
                fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
                char *buf = (char *)malloc(sz + 1); fread(buf, 1, sz, f); buf[sz] = '\0'; fclose(f);
                vm_interpret(vm, buf); free(buf);
                bool ok = aot_build_object(vm, out_file);
                vm_free(vm);
                return ok ? 0 : 1;
            } else {
                fprintf(stderr, "Error: --emit-obj requires <out_file> and <src_file>\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--emit-c") == 0) {
            if (i + 2 < argc) {
                const char *out_file = argv[i + 1];
                const char *src_file = argv[i + 2];
                VM *vm = vm_create();
                FILE *f = fopen(src_file, "rb");
                if (!f) { fprintf(stderr, "Could not open '%s'\n", src_file); return 1; }
                fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
                char *buf = (char *)malloc(sz + 1); fread(buf, 1, sz, f); buf[sz] = '\0'; fclose(f);
                vm_interpret(vm, buf); free(buf);
                bool ok = aot_emit_c_file(vm, out_file);
                vm_free(vm);
                return ok ? 0 : 1;
            } else {
                fprintf(stderr, "Error: --emit-c requires <out_file> and <src_file>\n");
                return 1;
            }
        } else {
            run_file(argv[i]);
            return 0;
        }
    }

    return 0;
}
