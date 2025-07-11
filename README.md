Windows:
Compile with cl main.c and run with main.exe input_name.txt output_name.txt 
developer prompt has cl

Linux:
Compile with clang main.c -o main and run with ./main input.txt output.txt

Supported instructions are
L.D Fa, Offset(addr)
S.D Fa, Offset(addr)
LI $d, IMM64 -Integer Immediate Load
LW $d, Offset(addr)
SW $s, Offset(addr)

ADD $d, $s, $t - Integer add
ADDI $d, $s, immediate – Integer Add with Immediate
ADD.D Fd, Fs, Ft – Floating Point Add
SUB.D Fd, Fs, Ft – Floating Point Subtract
SUB $d, $s, $t -Integer Subtract
*MUL.D Fd, Fs, Ft -Floating Point Multiply
DIV.D Fd, Fs, Ft – Floating Point Divide

BEQ $S, $T, OFF18 - Branch to offset if equal
BNE $S, $T, OFF18 - Branch to offset if not equal
J ADDR28 - Unconditional jump to addr

Cache is simulated to be 8 set 2 way, miss will cost 3 cycles, hit will be 1. Branch prediction is one bit, always start as taken.
Will run example_text.txt if no input/output file provided.





