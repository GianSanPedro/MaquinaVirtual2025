#include <stdio.h>
#include "MVTipos.h"

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

void procesarInstruccion(TMV *mv, TInstruccion inst) {
    switch (inst.codOperacion) {
        // Instrucciones de 2 operandos
        case 0x10: MOV(mv, inst.op1, inst.op2); break;
        case 0x11: ADD(mv, inst.op1, inst.op2); break;
        case 0x12: SUB(mv, inst.op1, inst.op2); break;
        case 0x13: SWAP(mv, inst.op1, inst.op2); break;
        case 0x14: MUL(mv, inst.op1, inst.op2); break;
        case 0x15: DIV(mv, inst.op1, inst.op2); break;
        case 0x16: CMP(mv, inst.op1, inst.op2); break;
        case 0x17: SHL(mv, inst.op1, inst.op2); break;
        case 0x18: SHR(mv, inst.op1, inst.op2); break;
        case 0x19: AND(mv, inst.op1, inst.op2); break;
        case 0x1A: OR(mv, inst.op1, inst.op2); break;
        case 0x1B: XOR(mv, inst.op1, inst.op2); break;

        // Instrucciones de 1 operando
        case 0x00: SYS(mv, inst.op1); break;
        case 0x08: NOT(mv, inst.op1); break;
        case 0x1C: LDL(mv, inst.op1); break;
        case 0x1D: LDH(mv, inst.op1); break;
        case 0x1E: RND(mv, inst.op1); break;

        // Saltos (usan op1.valor como destino)
        case 0x01: JMP(mv, inst.op1.valor); break;
        case 0x02: JZ(mv, inst.op1.valor); break;
        case 0x03: JP(mv, inst.op1.valor); break;
        case 0x04: JN(mv, inst.op1.valor); break;
        case 0x05: JNZ(mv, inst.op1.valor); break;
        case 0x06: JNP(mv, inst.op1.valor); break;
        case 0x07: JNN(mv, inst.op1.valor); break;

        // Instrucción sin operandos
        //case 0x0F: STOP(mv); break;

        default:
            printf("⚠️  Instrucción no implementada: 0x%02X\n", inst.codOperacion);
            break;
    }
}
