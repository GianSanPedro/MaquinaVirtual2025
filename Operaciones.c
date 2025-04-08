#include <stdio.h>
#include "MVTipos.h"

const char *mnemonicos[] = {
    "SYS", "JMP", "JZ", "JP", "JN", "JNZ", "JNP", "JNN", "NOT", "", "", "", "", "", "", "STOP",
    "MOV", "ADD", "SUB", "SWAP", "MUL", "DIV", "CMP", "SHL", "SHR", "AND", "OR", "XOR", "LDL", "LDH", "RND"
};

const char *regNombres[] = {
    "EAX", "EBX", "ECX", "EDX", "EEX", "EFX", "DS", "CS", "IP", "AC", "CC", "AX", "BX", "CX", "DX", "??"
};

const char *segmentos[] = { "E", "L", "H", "" }; // EAX, AL, AH, AX

void MostrarOperando(TOperando op) {
    switch (op.tipo) {
        case 1: // Registro
            printf("%s%s", segmentos[op.segmentoReg], regNombres[op.registro]);
            break;
        case 2: // Inmediato
            printf("%d", op.valor);
            break;
        case 3: // Memoria
            if (op.registro == 1) // DS
                printf("[%d]", op.desplazamiento);
            else
                printf("[%s+%d]", regNombres[op.registro], op.desplazamiento);
            break;
    }
}

void MostrarInstruccion(TInstruccion inst) {
    printf("%s\t", mnemonicos[inst.codOperacion]);

    int tieneOp1 = inst.op1.tipo != 0;
    int tieneOp2 = inst.op2.tipo != 0;

    if (tieneOp2) {
        MostrarOperando(inst.op2);
        if (tieneOp1) printf(", ");
    }

    if (tieneOp1) {
        MostrarOperando(inst.op1);
    }

    printf("\n");
}
