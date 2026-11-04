---
language: preforth
contributors:
    - ["Xyzzy Apps", "https://github.com/xyzzyapps"]
filename: learnpreforth.pf
---

PreForth is a prefix concatenative systems programming language. While traditional Forth uses postfix notation (`2 3 +`), PreForth lets you write expressions naturally in prefix notation (`+ 2 3`) while compiling down into linear postfix bytecode for O(1) `goto` state-machine jumps, a dual data/return stack, and zero-overhead bidirectional C ABI interoperability.

PreForth code executes top-to-bottom, line-by-line. Within each statement, tokens are compiled in reverse (right-to-left), converting prefix operators into linear postfix execution instructions.

```forth
# -----------------------------------------------------------------------------
# 1. BASICS & COMMENTS
# -----------------------------------------------------------------------------

# Comments start with a hash '#' and continue to the end of the line.

# Newlines '<NL>' delimit statements.
# Multiple statements can also be written on a single line separated by commas ',':
+ 1 2, + 3 4

# Literals:
42          # Decimal integer
-17         # Negative integer
0xFF        # Hexadecimal integer (255)
3.14159     # Floating-point number
"Hello!\n"  # String literal with standard escape sequences (\n, \t, \", \\)

# Print values using the dot '.' word or 'print':
. "Hello, PreForth!"
. 42
print 3.14159


# -----------------------------------------------------------------------------
# 2. THE GOLDEN RULE OF PREFIX EVALUATION
# -----------------------------------------------------------------------------

# Traditional Forth: 2 3 + (push 2, push 3, add -> 5)
# PreForth:          + 2 3 (evaluates 3, evaluates 2, executes + -> 5)

# Within any single statement, PreForth processes tokens right-to-left.
# In '+ 2 3':
#   1. '3' is pushed to the data stack.
#   2. '2' is pushed to the data stack (now Top-of-Stack: 2, second: 3).
#   3. '+' pops lhs = 2, pops rhs = 3, and pushes 2 + 3 = 5.

+ 2 3       # Stack: [ 5 ]


# -----------------------------------------------------------------------------
# 3. NON-COMMUTATIVE ARITHMETIC & LOGIC
# -----------------------------------------------------------------------------

# Crucial Rule: Binary operators pop `lhs` first and `rhs` second:
#   lhs = pop()
#   rhs = pop()
#   result = lhs <op> rhs

- 10 3      # lhs = 10, rhs = 3 -> 10 - 3 = 7
/ 20 4      # lhs = 20, rhs = 4 -> 20 / 4 = 5
% 17 5      # 17 % 5 = 2

# Nested prefix expressions evaluate naturally inside-out:
- 10 * 2 3  # * 2 3 evaluates to 6, then 10 - 6 = 4
/ 20 + 2 3  # + 2 3 evaluates to 5, then 20 / 5 = 4

# Unary operations:
inc 41      # 41 + 1 = 42
dec 10      # 10 - 1 = 9
neg 5       # -5

# Comparisons (push 1 for true, 0 for false):
== 5 5      # 1 (true)
!= 5 3      # 1 (true)
<  3 5      # 1 (true)
>  7 2      # 1 (true)
<= 4 4      # 1 (true)
>= 9 10     # 0 (false)

# Bitwise & Boolean operators:
and 0xF0 0x0F   # 0x00
or  0xF0 0x0F   # 0xFF
xor 0xAA 0xFF   # 0x55
not 1           # 0
not 0           # 1
shl 1 4         # 1 << 4 = 16
shr 16 2        # 16 >> 2 = 4


# -----------------------------------------------------------------------------
# 4. DATA STACK & RETURN STACK MANIPULATION
# -----------------------------------------------------------------------------

# PreForth maintains a 64-bit LIFO data stack:
dup         # Duplicate top of stack: ( a -- a a )
drop        # Discard top of stack:   ( a -- )
swap        # Swap top two elements:  ( b a -- a b )
over        # Copy 2nd element to top:( a b -- a b a )

# Example of stack manipulation:
# If stack has: 42
dup         # Stack: 42 42
*           # 42 * 42 = 1764

# The return stack can store temporary values:
>r          # Move Top-of-Stack to Return Stack: ( a -- ) (R: -- a )
r>          # Move Top of Return Stack to Data Stack: ( -- a ) (R: a -- )


# -----------------------------------------------------------------------------
# 5. VARIABLES & DICTIONARY MEMORY
# -----------------------------------------------------------------------------

# 'var <name>' allocates a 64-bit variable initialized to 0.
# Referencing <name> pushes the memory address/slot of that variable.
var counter
var total

# 'set' stores a value into a variable: set <var> <val>
# Pops <var> first, then pops <val> second.
set counter 10
set total 100

# '@' fetches the value from a variable address: @ <var>
@ counter   # Pushes 10 onto stack
@ total     # Pushes 100 onto stack

# Incrementing a variable in prefix style:
set counter + @ counter 1    # counter is now 11


# -----------------------------------------------------------------------------
# 6. DEFINING WORDS (FUNCTIONS)
# -----------------------------------------------------------------------------

# Use 'def <name> ... ;' to define new words.
# Definitions compile statements into reusable bytecode blocks.

# Square a number (x on stack -> x * x):
# In prefix: '* dup' reverses to 'dup, *'
def square
  * dup ;

square 7    # Stack: [ 49 ]

# Cube a number (x on stack -> x * x * x):
# In prefix: '* * dup dup' reverses to 'dup, dup, *, *'
def cube
  * * dup dup ;

cube 3      # Stack: [ 27 ]

# Words can be recursive:
# In the then branch: '1 drop' reverses to 'drop' the 0, then push '1'.
# In the else branch: '* factorial dec dup' reverses to 'dup, dec, factorial, *'.
def factorial
  if == dup 0 then
    1 drop
  else
    * factorial dec dup
  end ;

factorial 5 # Stack: [ 120 ]


# -----------------------------------------------------------------------------
# 7. GOTO & PREFIX JUMP DIRECTIVES
# -----------------------------------------------------------------------------

# PreForth supports O(1) linear bytecode jumps using labels:
# Labels can be declared with '<name>:' or 'label <name>'.

# Unconditional jump:
# goto <label>

# Conditional jumps (prefix: evaluates <cond_expr> first):
# ?goto  <label> <cond_expr>   # Jump if cond != 0 (truthy)
# ?!goto <label> <cond_expr>   # Jump if cond == 0 (falsy)

# High-performance Collatz Conjecture state machine in constant stack space:
var collatz_n
var collatz_steps_count

def collatz_steps
  set collatz_steps_count 0
  set collatz_n
step_loop:
  ?goto done == @ collatz_n 1
  set collatz_steps_count inc @ collatz_steps_count

  # Jump if even
  ?goto is_even == % @ collatz_n 2 0
  set collatz_n + * 3 @ collatz_n 1
  goto step_loop

is_even:
  set collatz_n / @ collatz_n 2
  goto step_loop

done:
  @ collatz_steps_count ;

collatz_steps 27   # Stack: [ 111 ]


# -----------------------------------------------------------------------------
# 8. STRUCTURED CONTROL FLOW
# -----------------------------------------------------------------------------

# 'if ... then ... else ... end' compiles into linear jump offsets.
# Goto statements can freely jump into or out of branches.

def signum
  if < dup 0 then
    drop -1
  else
    if > dup 0 then
      drop 1
    else
      drop 0
    end
  end ;

signum -42  # -1
signum 100  # 1
signum 0    # 0


# -----------------------------------------------------------------------------
# 9. RAW MEMORY, POINTERS & C STRINGS
# -----------------------------------------------------------------------------

# Byte-level memory access (8-bit, 32-bit, 64-bit):
#   c@8  <ptr>          Fetch 8-bit unsigned integer
#   c@32 <ptr>          Fetch 32-bit unsigned integer
#   c@64 <ptr>          Fetch 64-bit unsigned integer
#   c!8  <ptr> <val>    Store 8-bit integer into address
#   c!32 <ptr> <val>    Store 32-bit integer into address
#   c!64 <ptr> <val>    Store 64-bit integer into address

# 'cstr <str_literal>' creates a null-terminated char buffer in memory:
var msg
set msg cstr "Hello C Memory"
c@8 @ msg   # Pushes ASCII value of 'H' (72)


# -----------------------------------------------------------------------------
# 10. FOREIGN FUNCTION INTERFACE (FFI) & C INTEROP
# -----------------------------------------------------------------------------

# PreForth can dynamically bind and call external C ABI shared libraries:
# extern <lib_path> <symbol_name> ( <arg_types...> ) -> <ret_type>

# Supported types: void, i32, u32, i64, u64, f64, ptr, str

# Call standard C library math functions:
extern "m" sin ( f64 ) -> f64
extern "c" puts ( str ) -> i32

# Exporting a PreForth word as a C callback:
# callback <c_abi_sig> <word_name> -> pushes native function pointer

def my_compare
  - c@32 swap c@32 ;

callback ( ptr ptr ) -> i32 my_compare
# Now you have a native function pointer compatible with C's qsort()!


# -----------------------------------------------------------------------------
# 11. LINKER DIRECTIVES & AOT SHARED LIBRARY GENERATION
# -----------------------------------------------------------------------------

# PreForth can compile directly to standalone native shared libraries (.so / .dll / .dylib):

# Declare external linker symbols:
# import <ret_type> <symbol_name> ( <arg_types...> )
import i32 puts ( str )

# Export PreForth words with C ABI calling convention:
# export <ret_type> <word_name> as "<c_symbol>" ( <arg_types...> )
def hyp_sq
  + * dup swap * dup ;

export i64 hyp_sq as "pf_hyp_sq" ( i64 i64 )

# Add linker library dependencies:
# link-lib "m"

# Emit Position-Independent Code (PIC) and build native shared library:
# build-shared "libmath.so"


# -----------------------------------------------------------------------------
# 12. RUNNING PREFORTH
# -----------------------------------------------------------------------------

# CLI REPL:
#   $ preforth

# Run a script file:
#   $ preforth script.pf

# Run verification test suite:
#   $ preforth --test

# In WebAssembly / Browser:
#   PreForth compiles to WebAssembly with WASI support!
#   Load preforth.wasm and execute scripts in any browser.
```

## Where to Go Next

* **Official Repository**: [github.com/xyzzyapps/preforth](https://github.com/xyzzyapps/preforth)
* **Interactive Live REPL**: [xyzzyapps.github.io/preforth](https://xyzzyapps.github.io/preforth)
* **Releases & Binaries**: Windows, Linux (x86_64, aarch64), macOS (Apple Silicon, Intel), WebAssembly.
