# How to make a (toy) programming language (Part 0.0: meta basic)

## How a compiler work? 

<center>
<pre style="font-size:0.7em;font-family: monospace; width: 100%;">
+--------------------------------+                  +--------------------------------------------------------------------------------------------------------------------------+
|                                |                  |                                                                                                                          |
|                                |                  |                                                                                                                          |
|      Human readable code       |----------------->|  +--------------------------+ +--------------------------+  +--------------------------+  +--------------------------+   |
|                                |                  |  |                          | |                          |  |                          |  |                          |   |
|                                |                  |  |                          | |                          |  |                          |  |                          |   |
+--------------------------------+                  |  |         Lexing           | |         Parsing          |  |       Optimization       |  |  Emitting instructions   |   |
                                                    |  |                          | |                          |  |                          |  |                          |   |
                                                    |  |                          | |                          |  |                          |  |                          |   |
                                                    |  +--------------------------+ +--------------------------+  +--------------------------+  +--------------------------+   |
                                                    |                                                                                                                          |
+--------------------------------+                  |      Break text into token        Tokens --> parse tree         Run different                Turn everything into        |
|                                |                  |                                   representing language         transformations to           (virtual) machine code      |
|                                |                  |                                   constructs                    optimize the code                                        |
|           Profit???            |<-----------------|                                                                                                                          |
|                                |                  |                                                                                                                          |
|                                |                  |                                                                                                                          |
+--------------------------------+                  +--------------------------------------------------------------------------------------------------------------------------+
</pre>
</center>



## Write a lexer & parser?

There are many excellent tutorials on the Internet about those things. However, if you want to write one, an easy option is looking into recursive descent parsers. In this tutorial, we’ll make it even easier: just use Antlr to generate our parser. The flow now becomes: `define our grammar -> (Antlr generates the parser & the abstract syntax tree[AST]) -> traverse AST -> make optimization -> emit code.`



## Write an optimizer and assembler?

This seems hard. Thankfully, there is LLVM which will do those hard works for us. So, we only need to do:

`define our grammar -> (Antlr generates the parser & the abstract syntax tree[AST]) -> traverse AST, emmit LLVM IR -> (LLVM does the magic) -> profit.`



## LLVM IR for lazy people

How to use LLVM IR to construct X, where X = functions, for loops, variable assignments, if else statements…

Just write X in C and compile the program into LLVM IR (`clang -S -emit-llvm somefile.c`). For example:

```C
// somefile.c
int func(int a) {
  return a;
}

int main() {
  int x = func(2);
  if (x > 3) {
    return 1;
  } else {
    return 2;
  }
}
```

```assembly
; result LLVM IR
define i32 @func(i32) #0 {
  %2 = alloca i32, align 4
  store i32 %0, i32* %2, align 4
  %3 = load i32, i32* %2, align 4
  ret i32 %3
}

; Function Attrs: noinline nounwind optnone
define i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %3 = call i32 @func(i32 2)
  store i32 %3, i32* %2, align 4
  %4 = load i32, i32* %2, align 4
  %5 = icmp sgt i32 %4, 3
  br i1 %5, label %6, label %7

; <label>:6:                                      ; preds = %0
  store i32 1, i32* %1, align 4
  br label %8

; <label>:7:                                      ; preds = %0
  store i32 2, i32* %1, align 4
  br label %8

; <label>:8:                                      ; preds = %7, %6
  %9 = load i32, i32* %1, align 4
  ret i32 %9
}
```

Having trouble wrap your head around this? This guide is useful (and short) https://mapping-high-level-constructs-to-llvm-ir.readthedocs.io/en/latest/

## Sound easy enough right? Let’s do it

Very small code for reference: https://github.com/quangio/Lucix
