#!/usr/bin/env bash
set -e

SRCS="src/lexer.c src/compiler.c src/vm.c src/ffi.c src/aot.c src/main.c"
INCS="-Isrc"

mkdir -p dist

echo "Building PreForth (native)..."
zig cc $INCS $SRCS -o preforth

if [ "$1" == "--all" ]; then
    echo "Cross-compiling for x86_64-windows..."
    zig cc -target x86_64-windows-gnu $INCS $SRCS -o dist/preforth-x86_64-windows.exe

    echo "Cross-compiling for x86_64-linux (musl)..."
    zig cc -target x86_64-linux-musl $INCS $SRCS -o dist/preforth-x86_64-linux

    echo "Cross-compiling for aarch64-linux (musl)..."
    zig cc -target aarch64-linux-musl $INCS $SRCS -o dist/preforth-aarch64-linux

    echo "Cross-compiling for x86_64-macos..."
    zig cc -target x86_64-macos-none -Wno-deprecated-declarations $INCS $SRCS -o dist/preforth-x86_64-macos

    echo "Cross-compiling for aarch64-macos..."
    zig cc -target aarch64-macos-none -Wno-deprecated-declarations $INCS $SRCS -o dist/preforth-aarch64-macos

    echo "All platform binaries compiled in dist/:"
    ls -lh dist/
fi
