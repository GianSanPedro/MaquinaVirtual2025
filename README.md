# Virtual Machine 2025

Spanish version available in [README_ES.md](README_ES.md).

Segmented virtual machine written in C for a Computer Architecture course (~2025). It loads binary images (`.vmx`), offers an optional disassembler, supports breakpoints that dump `.vmi` snapshots, and ships with a test toolkit containing assembler specs and helper tools.

## Context and sources
- Academic assignment; this repo contains the VM implementation and instruction interpreter.
- Official PDFs in `Entorno Test/Documentacion del ASSEMBLER y MV desarrollados/`: assembler specs (VM1/VM2) and the project statements.
- Toolkit under `Entorno Test/`: binaries `vmg.exe` and `vmt.exe` (assembler/translator to build `.vmx`, see the PDFs) plus the prebuilt VM executable `MaquinaVirtual2025.exe`. Sample `.asm`, `.vmx`, and `.vmi` programs are included.

## VM design
- CPU with sixteen 32-bit registers: CS, DS, ES, SS, KS, IP, SP, BP, CC, AC, and general-purpose EAX–EFX. `CC` uses bits 31 (N) and 30 (Z); `AC` stores division remainders.
- Segmented memory backed by a Segment Descriptor Table (TDS) with up to 8 entries (upper 16 bits = physical base, lower 16 bits = segment size).
- VMX v1 binaries: only a Code segment; the remaining RAM is used as data. VMX v2 binaries: Code, Data, Extra, Stack, Const, and an optional Param segment when using `-p`. The entry point comes from the header and seeds `IP` (selector + offset).
- Stack lives in `SS` with downward-growing `SP`; `PUSH/POP/CALL/RET` guard overflow/underflow.
- Instruction decoding: first byte holds opcode (5 bits) plus operand type fields (2 bits). Operands may be registers (full or AL/AH/AX slices), 16-bit sign-extended immediates, or segmented memory with byte/word/long cell sizes (defaults to `DS` when base register is 0).
- Optional disassembler (`-d`) dumps code, entry point, and constants before execution. `SYS 15` implements a breakpoint that writes a `.vmi` and exposes step/go/quit when an image name was provided.

## Instructions and syscalls
- Control flow: `JMP`, `JZ`, `JP`, `JN`, `JNZ`, `JNP`, `JNN`, `CALL`, `RET`, `STOP`.
- Data/ALU: `MOV`, `ADD`, `SUB`, `SWAP`, `MUL`, `DIV` (remainder in `AC`), `CMP`, `SHL`, `SHR`, `AND`, `OR`, `XOR`, `LDL`, `LDH`, `RND`, `NOT`, `PUSH/POP` (v2 only).
- Syscalls: `SYS 1` numeric/char input; `SYS 2` formatted output; `SYS 3` read string; `SYS 4` print string; `SYS 7` clear screen; `SYS 15` breakpoint and `.vmi` dump. Conventions: `EDX` points to the buffer (selector:offset), `ECX` sets count and cell size, `AL` configures the I/O mode.

## File formats
- `.vmx`: "VMX25" header + version byte. v1: 2-byte code size followed by code. v2: sizes for Code/Data/Extra/Stack/Const and entry point (2 bytes each); payload stores Code then Const, the rest is zeroed in RAM.
- `.vmi`: "VMI25" header, version, memory size in KiB, registers (16×4-byte big-endian), TDS (8×4-byte big-endian), and full memory dump.

## Project layout and assets
- Source: `main.c`, `Operaciones.c/.h`, `Funciones.c/.h`, `MVTipos.h`, `MaquinaVirtual2025.cbp`.
- Docs: PDFs under `Entorno Test/Documentacion del ASSEMBLER y MV desarrollados/`.
- Test sets: `Entorno Test/Resultados Cuestionario 1` and `Resultados Cuestionario 2` with proven `.vmx`/`.vmi` plus screenshots. Some cases assert correct outputs, others check proper error detection (and absence of false positives).
- Extra examples: `Entorno Test/Test extra` holds `.asm` samples with their `.vmx`/`.vmi` counterparts (`factorial`, `cuentabits`, `pushpop`, etc.).
- Helper binaries: `Entorno Test/vmg.exe`, `vmt.exe` (assembler/translator to `.vmx`, follow the PDFs) and `Entorno Test/MaquinaVirtual2025.exe` (Windows build of the VM).

## Requirements
- C11-capable compiler (GCC/Clang/MinGW) and a terminal; no external libraries.

## Build from source
```bash
gcc -std=c11 -Wall -O2 -o MaquinaVirtual2025 main.c Operaciones.c Funciones.c
```

## Run
- Using the compiled binary or `Entorno Test/MaquinaVirtual2025.exe`, from the repo root:
  - Load a `.vmx` v1/v2: `./MaquinaVirtual2025 program.vmx [-d] [m=32] [-p arg1 arg2 ...] [state.vmi]`
    - `-d` enables the disassembler, `m=` sets RAM in KiB, `-p` forwards guest args (v2), and a `.vmi` name turns on breakpoint snapshots.
  - Resume from `.vmi`: `./MaquinaVirtual2025 state.vmi [-d]`; `m=` is ignored because the image carries memory/registers/TDS.
- Quick examples with bundled assets:
  - `./MaquinaVirtual2025 "Entorno Test/Test extra/factorial.vmx" -d`
  - `./MaquinaVirtual2025 "Entorno Test/Resultados Cuestionario 2/sample1.vmi" -d`

## Create and run your own tests
1) Write an assembler program (see `Entorno Test/Test extra/*.asm` and the syntax in the PDFs).
2) Use `vmg.exe` / `vmt.exe` from `Entorno Test` to translate it into a `.vmx`, following the documentation for exact usage.
3) Execute the resulting `.vmx` with the VM as shown above, or produce `.vmi` snapshots via `SYS 15`.

## Future work
- Seed `rand()` with a configurable value for reproducible runs.
- Revisit the `DS`-based memory access marked in code ("REVISAAAAAR") and clarify signed-offset handling.
- Add sample `.vmx` programs covering all syscalls and automated tests.
- Add a plain-text quickstart for `vmg.exe`/`vmt.exe` alongside the PDFs.
