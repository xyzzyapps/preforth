const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "preforth",
        .target = target,
        .optimize = optimize,
    });

    exe.addIncludePath(b.path("src"));
    exe.addCSourceFiles(.{
        .files = &.{
            "src/lexer.c",
            "src/compiler.c",
            "src/vm.c",
            "src/ffi.c",
            "src/aot.c",
            "src/main.c",
        },
        .flags = &.{"-std=c99"},
    });
    exe.linkLibC();

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run PreForth");
    run_step.dependOn(&run_cmd.step);

    const test_cmd = b.addRunArtifact(exe);
    test_cmd.addArg("--test");
    const test_step = b.step("test", "Run PreForth test suite");
    test_step.dependOn(&test_cmd.step);
}
