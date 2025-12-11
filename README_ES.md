# Máquina Virtual 2025

Versión en inglés disponible en [README.md](README.md).

Máquina virtual segmentada escrita en C para la materia Arquitectura de Computadoras (~2025). Ejecuta imágenes binarias (.vmx), cuenta con un disassembler opcional, soporta breakpoints con volcados `.vmi` y se entrega con un entorno de pruebas que incluye especificaciones del assembler y herramientas para generar casos de test.

## Contexto y fuentes
- Trabajo práctico académico; la implementación de la VM y el intérprete está en este repo.
- Documentación oficial en `Entorno Test/Documentacion del ASSEMBLER y MV desarrollados/`: PDFs con la especificación del assembler (VM1/VM2) y las consignas de diseño.
- Toolkit en `Entorno Test/`: binarios `vmg.exe` y `vmt.exe` para traducir/ensamblar programas a `.vmx` (ver PDFs) y `MaquinaVirtual2025.exe` como ejecutable ya compilado de la VM. Incluye ejemplos `.asm`, `.vmx` y `.vmi`.

## Diseño de la máquina virtual
- CPU con 16 registros de 32 bits: CS, DS, ES, SS, KS, IP, SP, BP, CC, AC y generales EAX–EFX. `CC` usa bits 31 (N) y 30 (Z); `AC` guarda restos de divisiones.
- Memoria segmentada con Tabla de Descriptores (TDS) de hasta 8 entradas: los 16 bits altos son la base física y los bajos el tamaño del segmento.
- Binaries VMX v1: solo segmento de código; el resto de la RAM queda como datos. VMX v2: segmentos Code, Data, Extra, Stack, Const y opcional Param (si se pasan argumentos con `-p`). El entry point se toma de la cabecera y carga `IP` (selector + offset).
- Pila en el segmento `SS` con `SP` decreciendo; `PUSH/POP/CALL/RET` validan overflow/underflow.
- Decodificación: primer byte contiene opcode (5 bits) más tipos de operandos (2 bits); operandos pueden ser registros completos o parciales (AL/AH/AX), inmediatos de 16 bits con signo, o memoria segmentada con tamaño byte/word/long (por defecto usa `DS` si el registro base es 0).
- Disassembler opcional (`-d`) que vuelca código, entry point y constantes antes de ejecutar. Breakpoint por `SYS 15` genera `.vmi` y permite step/go/quit cuando se indicó un nombre de imagen.

## Instrucciones y syscalls
- Flujo: `JMP`, `JZ`, `JP`, `JN`, `JNZ`, `JNP`, `JNN`, `CALL`, `RET`, `STOP`.
- ALU y datos: `MOV`, `ADD`, `SUB`, `SWAP`, `MUL`, `DIV` (resto en `AC`), `CMP`, `SHL`, `SHR`, `AND`, `OR`, `XOR`, `LDL`, `LDH`, `RND`, `NOT`, `PUSH/POP` (solo v2).
- Syscalls: `SYS 1` leer numérico/char; `SYS 2` escribir en varios formatos; `SYS 3` leer string; `SYS 4` escribir string; `SYS 7` limpiar pantalla; `SYS 15` breakpoint y volcado `.vmi`. Convenciones: `EDX` apunta al buffer (selector:offset), `ECX` define cantidad y tamaño por celda, `AL` configura modo de E/S.

## Formatos de archivo
- `.vmx`: cabecera "VMX25" + byte de versión. v1: tamaño de código (2 bytes) seguido del código. v2: tamaños (Code/Data/Extra/Stack/Const) y entry point (2 bytes cada uno); el payload almacena Code seguido de Const y el resto se inicializa en cero en RAM.
- `.vmi`: cabecera "VMI25", versión, tamaño de memoria en KiB, registros (16×4 bytes big-endian), TDS (8×4 bytes big-endian) y volcado completo de memoria.

## Árbol del proyecto y recursos
- Código fuente: `main.c`, `Operaciones.c/.h`, `Funciones.c/.h`, `MVTipos.h`, proyecto `MaquinaVirtual2025.cbp`.
- `Entorno Test/Documentacion del ASSEMBLER y MV desarrollados/`: PDFs con detalle de assembler y consignas.
- `Entorno Test/Resultados Cuestionario 1` y `Resultados Cuestionario 2`: conjuntos de `.vmx` y una `.vmi` ya probados, con capturas de salida.
- `Entorno Test/Test extra`: ejemplos `.asm` junto a sus `.vmx`/`.vmi` (p.ej. `factorial`, `cuentabits`, `pushpop`).
- Binarios auxiliares: `Entorno Test/vmg.exe`, `vmt.exe` (ensamblador/traductor a `.vmx`, seguir instrucciones de los PDFs) y `Entorno Test/MaquinaVirtual2025.exe` (VM compilada en Windows).

## Requisitos
- Compilador C compatible con C11 (GCC/Clang/MinGW) y consola. No hay dependencias externas.

## Compilación desde el código fuente
```bash
gcc -std=c11 -Wall -O2 -o MaquinaVirtual2025 main.c Operaciones.c Funciones.c
```

## Ejecución
- Desde el binario compilado o `Entorno Test/MaquinaVirtual2025.exe`, ubícate en la raíz del repo:
  - Cargar un `.vmx` v1/v2: `./MaquinaVirtual2025 programa.vmx [-d] [m=32] [-p arg1 arg2 ...] [salida.vmi]`
    - `-d` activa disassembler, `m=` ajusta RAM (KiB), `-p` pasa argumentos al programa (v2) y el nombre `.vmi` habilita snapshots en breakpoints.
  - Reanudar desde `.vmi`: `./MaquinaVirtual2025 estado.vmi [-d]` (ignora `m=` porque la imagen contiene memoria, registros y TDS).
- Ejemplos rápidos con assets incluidos:
  - `./MaquinaVirtual2025 "Entorno Test/Test extra/factorial.vmx" -d`
  - `./MaquinaVirtual2025 "Entorno Test/Resultados Cuestionario 2/sample1.vmi" -d`

## Crear y ejecutar tests propios
1) Escribe un programa en assembler (ver ejemplos en `Entorno Test/Test extra/*.asm` y la sintaxis en los PDFs).
2) Usa las herramientas `vmg.exe` / `vmt.exe` del directorio `Entorno Test` para traducirlo a `.vmx` siguiendo las indicaciones de la documentación.
3) Ejecuta el `.vmx` resultante con la VM como en los ejemplos anteriores o genera snapshots `.vmi` con `SYS 15`.

## Trabajo futuro
- Sembrar `rand()` con una semilla configurable para ejecuciones reproducibles.
- Revisar el acceso a memoria con base `DS` marcado en el código (comentario "REVISAAAAAR") y unificar el manejo de offsets con signo.
- Añadir ejemplos `.vmx` que cubran todas las syscalls y pruebas automatizadas.
- Documentar el uso detallado de `vmg.exe`/`vmt.exe` en texto plano junto a los PDFs.
