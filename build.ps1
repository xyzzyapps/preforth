param(
    [switch]$All = $false
)

$ErrorActionPreference = "Stop"

$SRCS = @("src/lexer.c", "src/compiler.c", "src/vm.c", "src/ffi.c", "src/aot.c", "src/main.c")
$INCS = "-Isrc"

if (!(Test-Path "dist")) {
    New-Item -ItemType Directory -Force "dist" | Out-Null
}

Write-Host "Building PreForth (native)..." -ForegroundColor Cyan
zig cc $INCS $SRCS -o preforth.exe

if ($All) {
    Write-Host "Cross-compiling for x86_64-windows..." -ForegroundColor Cyan
    zig cc -target x86_64-windows-gnu $INCS $SRCS -o dist/preforth-x86_64-windows.exe

    Write-Host "Cross-compiling for x86_64-linux (musl)..." -ForegroundColor Cyan
    zig cc -target x86_64-linux-musl $INCS $SRCS -o dist/preforth-x86_64-linux

    Write-Host "Cross-compiling for aarch64-linux (musl)..." -ForegroundColor Cyan
    zig cc -target aarch64-linux-musl $INCS $SRCS -o dist/preforth-aarch64-linux

    Write-Host "Cross-compiling for x86_64-macos..." -ForegroundColor Cyan
    zig cc -target x86_64-macos-none -Wno-deprecated-declarations $INCS $SRCS -o dist/preforth-x86_64-macos

    Write-Host "Cross-compiling for aarch64-macos..." -ForegroundColor Cyan
    zig cc -target aarch64-macos-none -Wno-deprecated-declarations $INCS $SRCS -o dist/preforth-aarch64-macos

    Write-Host "Compiling for WebAssembly (wasm32-wasi)..." -ForegroundColor Cyan
    $WASM_SRCS = @("src/wasm_bridge.c", "src/lexer.c", "src/compiler.c", "src/vm.c", "src/ffi.c", "src/aot.c")
    zig cc -target wasm32-wasi -O3 -fno-sanitize=all -DPREFORTH_WASM=1 -mexec-model=reactor `
        "-Wl,--export=preforth_wasm_init" `
        "-Wl,--export=preforth_wasm_eval" `
        "-Wl,--export=preforth_wasm_reset" `
        "-Wl,--export=preforth_wasm_stack_depth" `
        "-Wl,--export=malloc" `
        "-Wl,--export=free" `
        -o dist/preforth.wasm $WASM_SRCS
    if (Test-Path "docs") {
        Copy-Item dist/preforth.wasm docs/preforth.wasm -Force
    }

    Write-Host "`nAll platform binaries compiled successfully in dist/:" -ForegroundColor Green
    Get-ChildItem dist
} else {
    Write-Host "Native build complete: preforth.exe" -ForegroundColor Green
}
