# PreForth v0.1

Initial release of PreForth—a prefix concatenative programming language with linear bytecode compilation, goto control flow, and a bidirectional C ABI FFI bridge with Ahead-of-Time (AOT) shared library emission.

### Highlights
- **Prefix to Linear Bytecode**: Statements written in prefix notation (`+ 2 3`) compile directly to linear postfix bytecode.
- **Prefix Binary Evaluation Rule**: Binary operators pop `lhs` first and `rhs` second, preserving natural mathematical semantics.
- **Control Flow**: Zero-overhead forward and backward `goto`, `?goto`, `?!goto`, labels (`<name>:` / `label <name>`), and structured branching (`if ... then ... else ... end`).
- **Memory & Variables**: 64-bit tagged stack cells, `@`, `!`, raw memory width accessors `c@8/32/64`, `c!8/32/64`, and `cstr`.
- **Bidirectional C ABI FFI Bridge**: Dynamic library loading (`lib`), C bindings (`extern`), variadic support (`...`), and native reverse C callbacks (`cb`).
- **AOT Shared Library & Linker Integration**: Linker directives `import`, `export`, `link-lib`, and `build-shared` to compile PreForth modules to native `.so`, `.dylib`, or `.dll` shared libraries with standard C ABI prologues/epilogues.
- **PolyForm Noncommercial License 1.0.0**.

### Attached Pre-compiled Binaries
- `preforth-x86_64-windows.exe` (Windows x86_64)
- `preforth-x86_64-linux` (Linux x86_64 musl)
- `preforth-aarch64-linux` (Linux aarch64 musl)
- `preforth-x86_64-macos` (macOS x86_64)
- `preforth-aarch64-macos` (macOS Apple Silicon aarch64)
