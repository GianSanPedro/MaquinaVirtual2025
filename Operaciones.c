#include <stdio.h>
#include "MVTipos.h"
#define CS  0
#define DS  1
#define IP  5
#define CC  8
#define AC  9
#define EAX  10
#define EBX  11
#define ECX  12
#define EDX  13
#define EEX  14
#define EFX  15

const char *mnemonicos[] = {
    "SYS", "JMP", "JZ", "JP", "JN", "JNZ", "JNP", "JNN", "NOT", "", "", "", "", "", "", "STOP",
    "MOV", "ADD", "SUB", "SWAP", "MUL", "DIV", "CMP", "SHL", "SHR", "AND", "OR", "XOR", "LDL", "LDH", "RND"
};

const char *regNombres[] = {
    "CS",  // 0
    "DS",  // 1
    "R2",  // 2 No definido
    "R3",  // 3 No definido
    "R4",  // 4 No definido
    "IP",  // 5
    "R6",  // 6 No definido
    "R7",  // 7 No definido
    "CC",  // 8
    "AC",  // 9
    "EAX", // 10
    "EBX", // 11
    "ECX", // 12
    "EDX", // 13
    "EEX", // 14
    "EFX"  // 15
};

void MostrarOperando(TOperando op) {
    if (op.tipo == 1) { // Registro

        if (op.registro < 0 || op.registro > 15) {
            printf("??");
            return;
        }

        // Si es un registro especial, mostrarlo sin segmentar
        if (op.registro <= 9) {
            printf("%s", regNombres[(int)op.registro]);
            return;
        }

        // Registros generales segmentables (EAX, EBX, ..., EFX)
        switch (op.segmentoReg) {
            case 0: // Registro completo (EAX, EBX, etc.)
                printf("%s", regNombres[(int)op.registro]);
                break;

            case 1: // Byte bajo
                switch (op.registro) {
                    case 10: printf("AL"); break;
                    case 11: printf("BL"); break;
                    case 12: printf("CL"); break;
                    case 13: printf("DL"); break;
                    case 14: printf("EL"); break;
                    case 15: printf("FL"); break;
                    default: printf("L%s", regNombres[(int)op.registro]); break;
                }
                break;

            case 2: // Byte alto
                switch (op.registro) {
                    case 10: printf("AH"); break;
                    case 11: printf("BH"); break;
                    case 12: printf("CH"); break;
                    case 13: printf("DH"); break;
                    case 14: printf("EH"); break;
                    case 15: printf("FH"); break;
                    default: printf("H%s", regNombres[(int)op.registro]); break;
                }
                break;

            case 3: // Palabra baja (AX, BX, etc.)
                switch (op.registro) {
                    case 10: printf("AX"); break;
                    case 11: printf("BX"); break;
                    case 12: printf("CX"); break;
                    case 13: printf("DX"); break;
                    case 14: printf("EX"); break;
                    case 15: printf("FX"); break;
                    default: printf("?%s", regNombres[(int)op.registro]); break;
                }
                break;

            default:
                printf("??");
        }

    } else if (op.tipo == 2) { // Inmediato
        printf("%d", op.valor);

    } else if (op.tipo == 3) { // Memoria
        if (op.registro == DS) {
            printf("[%d]", op.desplazamiento); // acceso absoluto a memoria DS
        } else if (op.registro >= 0 && op.registro < 16) {
            printf("[%s+%d]", regNombres[(int)op.registro], op.desplazamiento);
        } else {
            printf("[??+%d]", op.desplazamiento);
        }
    }
}

void MostrarInstruccion(TInstruccion inst, char *memoria) {
    // Dirección IP en hexadecimal (4 dígitos)
    printf("[%.4X] ", inst.ipInicial);

    // Mostrar instrucción en hexa, byte por byte
    for (int i = 0; i < inst.tamanio; i++) {
        printf("%.2X ", (unsigned char)memoria[inst.ipInicial + i]);
    }

    // Rellenar espacio si la instrucción es corta (para alinear el pipe |)
    for (int i = inst.tamanio; i < 6; i++) {
        printf("   ");
    }

    printf("|  %s", mnemonicos[inst.codOperacion]);

    // Mostrar operandos si existen
    int tieneOp1 = inst.op1.tipo != 0;
    int tieneOp2 = inst.op2.tipo != 0;

    if (tieneOp1) {
        printf("     ");
        MostrarOperando(inst.op1);
        if (tieneOp2) {
            printf(", ");
            MostrarOperando(inst.op2);
        }
    }

    printf("\n");
}

