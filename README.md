# PreForth

PreForth is a prefix concatenative programming language featuring linear bytecode compilation, $O(1)$ goto control flow, and a zero-overhead bidirectional C ABI FFI bridge.

In PreForth, expressions within a statement are written in **prefix notation** (`+ 2 3`) rather than postfix (`2 3 +`), statements execute top-to-bottom, and prefix expressions compile down to linear postfix bytecode for efficient execution and seamless C interoperability.

---

## Quick Links

- 🌐 **Live WebAssembly Playground**: [xyzzyapps.github.io/preforth](https://xyzzyapps.github.io/preforth/)
- 📖 **Tutorial (Learn X in Y Minutes)**: [LEARN.md](LEARN.md)
- 📦 **Download Pre-Built Binaries (v0.1)**: [GitHub Releases](https://github.com/xyzzyapps/preforth/releases/tag/v0.1)
- 📜 **License**: [PolyForm Noncommercial License 1.0.0](LICENSE)

---

## Key Architectural Principles

1. **Prefix Notation to Linear Bytecode**: Statements are parsed top-to-bottom, and within each statement, tokens are reversed right-to-left so prefix expressions compile directly into postfix bytecode.
2. **Crucial Binary Evaluation Rule**: Because prefix expressions `op a b` evaluate right-to-left into bytecode `b a op`, `a` is on top of the stack (TOS) when `op` executes. Therefore, all binary operators pop `lhs = pop()` first and `rhs = pop()` second, ensuring non-commutative operations like `-`, `/`, and comparisons preserve mathematical order.
3. **Control Flow & $O(1)$ Jumps**: Both structured control flow (`if ... then ... else ... end`) and jump directives (`goto`, `?goto`, `?!goto`) compile into linear bytecode with anonymous offsets and label backpatching.
4. **Bidirectional C ABI Bridge**: Zero-overhead foreign function interface with support for dynamic library loading (`lib`), C function bindings (`extern`), variadics (`...`), and native C callbacks (`cb`).

---

## Specification & Feature Matrix

### Phase 1: Lexer & Statement Chunker
- **Tokenization**: Whitespace-delimited tokens with double-quoted string literals and escape sequences (`\n`, `\t`, `\r`, `\"`, `\\`, `\0`). Line comments start with `#`.
- **Statement Boundary Detection**: Emits an explicit `<NL>` token at the end of every non-empty logical line. Comma `,` acts as an inline statement separator equivalent to `<NL>`.
- **Literals**: Base-10 integers (`42`, `-7`), hex (`0xFF`), floats (`3.14159`, `-0.5`), and string literals (`"..."`).

### Phase 2: Data Stack, Return Stack & Core Primitives
- **Data Stack**: LIFO stack of 64-bit cells (integers, floats, raw pointers, strings).
- **Return Stack**: LIFO call stack storing return instruction pointers and temporary values (`>r`, `r>`).
- **Stack Ops**: `dup`, `drop`, `swap`, `over`, `>r`, `r>`.
- **Arithmetic & Logic**: `+`, `-`, `*`, `/`, `%`, `inc`, `dec`, `neg`.
- **Comparisons**: `==`, `!=`, `<`, `>`, `<=`, `>=` (push 1 for true, 0 for false).
- **Bitwise/Boolean**: `and`, `or`, `xor`, `not`, `shl`, `shr`.
- **I/O**: `.` (pops TOS and prints).

### Phase 3: Statement-Reversing Bytecode Compiler & Goto
- **Statement Reversal (Pass 1)**: Reverses token order within statements.
- **Label Extraction**: Labels defined as `<name>:` or `label <name>` recorded in current scope without emitting runtime instructions.
- **Prefix Jump Directives**:
  - `goto <label>`: emits unconditional jump.
  - `?goto <label> <cond_expr>`: evaluates `<cond_expr>`, jumps if condition != 0.
  - `?!goto <label> <cond_expr>`: evaluates `<cond_expr>`, jumps if condition == 0.
- **Structured Control Flow**: `if <cond> then <true_body> [else <false_body>] end` compiles to internal anonymous conditional and unconditional jumps.
- **Label Backpatching (Pass 2)**: Replaces label references with resolved integer instruction indices.

### Phase 4: Word Definitions & Memory Operations
- **Word Definitions**: `def <name> ... ;` compiles to flat bytecode blocks.
- **Variables**: `var <name>` allocates an 8-byte slot initialized to 0; referencing `<name>` pushes its address.
- **Fetch & Store**:
  - `@ <var>`: fetches 64-bit value at address.
  - `! <var> <val>`: stores `<val>` into `<var>` (pops `<var>` first, `<val>` second).
- **Raw Memory Words**:
  - Fetch: `c@8`, `c@32`, `c@64`
  - Store: `c!8`, `c!32`, `c!64`
  - `cstr`: reads null-terminated C `char*` into string.

### Phase 5: Bidirectional C ABI FFI Bridge
- **Library Loading**: `lib <handle> "<path>"` (libc and libm pre-loaded by default).
- **C Function Binding**: `extern <lib_handle> <ret_type> <func_name> ( <arg_types...> )`.
- **Supported Types**: `void`, `i32`, `u32`, `i64`, `u64`, `f64`, `ptr`, `str`.
- **Variadic Support**: `extern <lib_handle> <ret_type> <func_name> ( <fixed_types...> ... )`.
- **Native Callbacks**: `cb <ret_type> <word_name> ( <arg_types...> )` generates native C function pointer trampolines.

### Phase 7: Linker Integration, Symbol Resolution & Shared Library Generation
- **Global Process Symbol Resolution**: `import <ret_type> <symbol_name> ( <arg_types...> )` declares an unresolved external symbol resolved by `ld` or process-wide symbol resolution (`dlsym(RTLD_DEFAULT)` / `GetModuleHandle(NULL)`).
- **Exporting PreForth Words as C ABI Symbols**: `export <ret_type> <word_name> as "<c_symbol_name>" ( <arg_types...> )` generates a C-ABI-compliant entry stub with platform export visibility (`__declspec(dllexport)` / `__attribute__((visibility("default")))`).
  - **Prologue**: Pushes incoming C register/stack arguments onto the data stack in reverse order (`argN-1` down to `arg0`) so `arg0` is at TOS.
  - **Body**: Executes the PreForth word.
  - **Epilogue**: Pops TOS and returns it in the native C return register.
- **Linker Dependency Directives**: `link-lib "<name>"` passes `-l<name>` tags (`DT_NEEDED` in ELF) to the system linker.
- **AOT Shared Library / Object Emission**: `build-shared "<out.so>"` (or CLI flags `--emit-shared`, `--emit-obj`, `--emit-c`) transpiles linear bytecode into Position-Independent C99 code and invokes `zig cc -shared -fPIC` to output standalone `.so`, `.dylib`, or `.dll` files.

---

## Verification Test Suite (Phases 1-7)

All 6 verification suites pass completely:

```bash
$ ./preforth.exe --test
============================================================
           PreForth Verification Test Suite (Phases 1-7)    
============================================================

[*] Running Test 1 (Non-Commutative Prefix Arithmetic)...
4
4
[+] Test 1 PASSED: 10 - (2 * 3) = 4, 20 / (2 + 3) = 4

[*] Running Test 2 (Recursive Word Definition)...
[+] Test 2 PASSED: fact(5) = 120

[*] Running Test 3 (Forward & Backward goto State Machine)...
[+] Test 3 PASSED: collatz_steps(27) = 111, max call depth = 2 (constant stack)

[*] Running Test 4 (Variadic C ABI & Heap Memory)...
[+] Test 4 PASSED: buffer formatted via snprintf, read back as "Val: 42"

[*] Running Test 5 (C Callback Interop with qsort)...
[+] Test 5 PASSED: [40, 10, 30, 20] sorted to [10, 20, 30, 40]

[*] Running Test 6 (Phase 7: Shared Library Round-Trip)...
[+] Test 6 PASSED: pf_factorial(10) via round-trip shared library = 3628800 (3628800)

============================================================
Test Results: 6 / 6 verification suites passed.
ALL TESTS PASSED SUCCESSFULLY!
============================================================
```

---

## Building Binaries with `zig cc`

PreForth is written in portable C and compiled using `zig cc` for cross-platform targeting.

### Native Compilation

```bash
zig cc -Isrc src/lexer.c src/compiler.c src/vm.c src/ffi.c src/main.c -o preforth.exe
```

Or using PowerShell / Shell scripts:
```powershell
.\build.ps1
```

### Multiplatform Compilation for All Targets

```powershell
.\build.ps1 -All
```

Builds binaries into `dist/`:
- **Windows x86_64**: `dist/preforth-x86_64-windows.exe`
- **Linux x86_64 (musl)**: `dist/preforth-x86_64-linux`
- **Linux aarch64 (musl)**: `dist/preforth-aarch64-linux`
- **macOS x86_64**: `dist/preforth-x86_64-macos`
- **macOS aarch64**: `dist/preforth-aarch64-macos`
- **WebAssembly (WASI)**: `dist/preforth.wasm`

### WebAssembly Compilation

```bash
zig cc -target wasm32-wasi -O3 -fno-sanitize=all -DPREFORTH_WASM=1 -mexec-model=reactor \
  -Wl,--export=preforth_wasm_init \
  -Wl,--export=preforth_wasm_eval \
  -Wl,--export=preforth_wasm_reset \
  -Wl,--export=preforth_wasm_stack_depth \
  -Wl,--export=malloc -Wl,--export=free \
  -o dist/preforth.wasm src/wasm_bridge.c src/lexer.c src/compiler.c src/vm.c src/ffi.c src/aot.c
```

---

## CLI Usage

- **Interactive REPL**:
  ```bash
  ./preforth.exe
  ```
- **Execute file**:
  ```bash
  ./preforth.exe examples/test3_collatz.pf
  ```
- **Evaluate inline expression**:
  ```bash
  ./preforth.exe -e ". + 10 32"
  ```
- **AOT Shared Library Compilation**:
  ```bash
  ./preforth.exe --emit-shared dist/libtest.dll examples/test6_shared_roundtrip.pf
  ```
- **AOT Object File Compilation**:
  ```bash
  ./preforth.exe --emit-obj dist/libtest.o examples/test6_shared_roundtrip.pf
  ```
- **AOT C99 Transpilation**:
  ```bash
  ./preforth.exe --emit-c dist/libtest.c examples/test6_shared_roundtrip.pf
  ```
- **Run verification test suite**:
  ```bash
  ./preforth.exe --test
  ```
