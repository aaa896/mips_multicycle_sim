
<img width="2559" height="1375" alt="output" src="https://github.com/user-attachments/assets/908096cb-4090-43ba-8dc5-1f5feed2f6e3" />


### Windows
Compile with developer prompt:

```bash
cl main.c
```

Run with:

```bash
main.exe input_name.txt output_name.txt
```


---

### Linux
Compile with:

```bash
clang main.c -o main
```

Run with:

```bash
./main input.txt output.txt
```

---

### Cache and Branch Prediction Details

- **Cache**: 8-set, 2-way associative
  - Miss cost: **3 cycles**
  - Hit cost: **1 cycle**

- **Branch Prediction**: One-bit, always starts as taken

---

### Default Behavior

If no input/output file is provided, the program will run example_text.txt to example_out.txt
---
### Supported Instructions

- `L.D Fa, Offset(addr)`
- `S.D Fa, Offset(addr)`
- `LI $d, IMM64` – Integer Immediate Load
- `LW $d, Offset(addr)`
- `SW $s, Offset(addr)`
- `ADD $d, $s, $t` – Integer Add
- `ADDI $d, $s, immediate` – Integer Add with Immediate
- `ADD.D Fd, Fs, Ft` – Floating Point Add
- `SUB.D Fd, Fs, Ft` – Floating Point Subtract
- `SUB $d, $s, $t` – Integer Subtract
- `MUL.D Fd, Fs, Ft` – Floating Point Multiply
- `DIV.D Fd, Fs, Ft` – Floating Point Divide
- `BEQ $S, $T, label\Offset` – Branch to offset if equal
- `BNE $S, $T, label\offset` – Branch to offset if not equal
- `J label\relative_offset` – Unconditional jump to address

---



