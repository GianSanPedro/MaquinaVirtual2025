#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include "MVTipos.h"
#include "Funciones.h"
#include "Operaciones.h"

const char *mnemonicos[] = {
    "SYS", "JMP", "JZ", "JP", "JN", "JNZ", "JNP", "JNN", "NOT", "", "", "PUSH", "POP", "CALL", "RET", "STOP",
    "MOV", "ADD", "SUB", "SWAP", "MUL", "DIV", "CMP", "SHL", "SHR", "AND", "OR", "XOR", "LDL", "LDH", "RND"
};

const char *regNombres[] = {
    "CS",  // 0
    "DS",  // 1
    "ES",  // 2
    "SS",  // 3
    "KS",  // 4
    "IP",  // 5
    "SP",  // 6
    "BP",  // 7
    "CC",  // 8
    "AC",  // 9
    "EAX", // 10
    "EBX", // 11
    "ECX", // 12
    "EDX", // 13
    "EEX", // 14
    "EFX"  // 15
};

void Disassembler(const TMV *MV, int tamCod) {
    printf("\n>> Código assembler cargado en memoria:\n");

    // Calculamos la direccion fisica inicial del Code Segment
    uint32_t  csReg = MV->registros[CS];
    uint32_t  selectorCS = csReg >> 16;                  // selector
    uint32_t  offsetCS = csReg & 0xFFFF;                 // offset (normalmente 0 al iniciar)
    uint32_t  baseCS = MV->TDS[selectorCS] >> 16;        // base fisica del segmento
    uint32_t  inicio = baseCS + offsetCS;                // inicio en MV memoria
    uint32_t  finCS = baseCS + tamCod;                   // fin (no inclusive)

    uint32_t entryOff  = MV->registros[IP] & 0xFFFF;        // Entry Point
    uint32_t entryAdrr = baseCS + entryOff;                 // Direccion a marcar >

    uint32_t ipTemp = inicio;
    int hayStop = 0;

    if(MV->version == 2){
        MostrarConstantes(MV);
        printf("\n");
    }

    while (ipTemp < finCS) {
        TInstruccion inst = LeerInstruccionCompleta(MV, ipTemp);
        if (inst.codOperacion == 0x0F) {
            hayStop = 1;
        }
        // Marca entry point
        printf("%c", (ipTemp == entryAdrr) ? '>' : ' ');
        MostrarInstruccion(inst, MV->memoria);
        ipTemp += inst.tamanio;
    }

    printf("\nError flag %d\n", MV->ErrorFlag);
    if (!hayStop) {
        printf("\nAdvertencia: STOP no presente en el código Assembler\n");
    }
}

void MostrarConstantes(TMV *mv) {
    uint16_t selectorKS = mv->registros[KS] >> 16;
    uint32_t base  = mv->TDS[selectorKS] >> 16;
    uint16_t size  = mv->TDS[selectorKS] & 0xFFFF;
    uint32_t end   = base + size;

    uint32_t pos = base;
    while (pos < end) {
        if (mv->memoria[pos] == 0) {
            // no es el inicio de una cadena: avanzamos uno
            pos = pos + 1;
        }
        else {
            // Encontramos el inicio de una cadena
            uint32_t inicio = pos;
            uint32_t cursor = pos;

            // Buscamos el terminador '\0'
            while (cursor < end && mv->memoria[cursor] != 0) {
                cursor = cursor + 1;
            }
            // Incluimos el byte 0 si cabe
            if (cursor < end) {
                cursor = cursor + 1;
            }

            uint32_t longitud = cursor - inicio;

            // Imprimimos la dirección
            printf("[%04X] ", inicio);

            // Imprimimos los bytes en hex (hasta 7, o 6 + "..")
            if (longitud <= 7) {
                uint32_t i = 0;
                while (i < longitud) {
                    printf("%02X ", (unsigned char)mv->memoria[inicio + i]);
                    i++;
                }
                // alinear
                while (i < 8) {
                    printf("   ");
                    i++;
                }
            }
            else {
                // Primeros 6 bytes
                uint32_t i = 0;
                while (i < 6) {
                    printf("%02X ", (unsigned char)mv->memoria[inicio + i]);
                    i++;
                }
                printf(".. ");
                // Alinear
                while (i < 8) {
                    printf("   ");
                    i++;
                }
            }

            // imprimir ASCII completo entre comillas
            printf("| \"");
            uint32_t j = 0;
            while (j < longitud) {
                unsigned char c = mv->memoria[inicio + j];
                if (c == 0) {
                    printf("\\0");
                }
                else if (isprint(c)) {
                    printf("%c", c);
                }
                else {
                    printf(".");
                }
                j++;
            }
            printf("\"\n");

            // saltamos al final de la cadena
            pos = cursor;
        }
    }
}

void MostrarInstruccion(TInstruccion inst, char *memoria) {
    // Direccion IP en hexadecimal (4 dígitos)
    printf("[%.4X] ", inst.ipInicial);

    // Mostrar instruccion en hexa, byte por byte
    for (int i = 0; i < inst.tamanio; i++) {
        printf("%.2X ", (unsigned char)memoria[inst.ipInicial + i]);
    }

    // Rellenar espacio si la instruccion es corta, para alinear la barra|
    for (int i = inst.tamanio; i < 8; i++) {
        printf("   ");
    }

    printf("|  %s", mnemonicos[(int)inst.codOperacion]);

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

void MostrarOperando(TOperando op) {
    if (op.tipo == 1) { // Registro

        if (op.registro < 0 || op.registro > 15) {
            printf("??");
            return;
        }

        // Si es un registro especial, lo mostramos sin segmentar
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
        char prefijo;
        if (op.tamCelda == 2)
            prefijo = 'W';
        else {
            if (op.tamCelda == 3)
                prefijo = 'B';
            else {
                if (op.tamCelda == 3)
                    prefijo = 'L';
            }
        }

        if (op.registro == DS) { //REVISAAAAAR!!!!!!!!!!!!!!!!!!
            printf("%c[%d]", prefijo, op.desplazamiento); // acceso absoluto a memoria DS
        } else if (op.registro >= 0 && op.registro < 16) {
            printf("%c[%s+%d]", prefijo, regNombres[(int)op.registro], op.desplazamiento);
        } else {
            printf("%c[??+%d]", prefijo, op.desplazamiento);
        }
    }
}

void procesarInstruccion(TMV *mv, TInstruccion inst) {
    unsigned int ip_backup = mv->registros[IP];
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
        case 0x1C: LDL(mv, inst.op1, inst.op2); break;
        case 0x1D: LDH(mv, inst.op1, inst.op2); break;
        case 0x1E: RND(mv, inst.op1, inst.op2); break;

        // Instrucciones de 1 operando
        case 0x00: SYS(mv, inst.op1); break;
        case 0x08: NOT(mv, inst.op1); break;
        case 0x0B:
            if (mv->version == 2) {
                PUSH(mv, inst.op1);
            } else {
                printf("ERROR: Trata de ejecutar PUSH en una MV1 : [%.4X] (%d)\n", mv->registros[IP], mv->registros[IP]);
                mv->ErrorFlag = 1;
            }
            break;
        case 0x0C:
            if (mv->version == 2) {
                POP(mv, inst.op1);
            } else {
                printf("ERROR: Trata de ejecutar POP en una MV1 : [%.4X] (%d)\n", mv->registros[IP], mv->registros[IP]);
                mv->ErrorFlag = 1;
            }
            break;
        case 0x0D:
            if (mv->version == 2) {
                CALL(mv, inst.op1);
            } else {
                printf("ERROR: Trata de ejecutar CALL en una MV1 : [%.4X] (%d)\n", mv->registros[IP], mv->registros[IP]);
                mv->ErrorFlag = 1;
            }
            break;

        // Instruccion SIN operandos
        case 0x0E:
            if (mv->version == 2) {
                RET(mv, inst.op1);
            } else {
                printf("ERROR: Trata de ejecutar RET en una MV1 : [%.4X] (%d)\n", mv->registros[IP], mv->registros[IP]);
                mv->ErrorFlag = 1;
            }
            break;

        // Saltos (usan op1.valor como destino)
        case 0x01: JMP(mv, inst.op1); break;
        case 0x02: JZ(mv, inst.op1); break;
        case 0x03: JP(mv, inst.op1); break;
        case 0x04: JN(mv, inst.op1); break;
        case 0x05: JNZ(mv, inst.op1); break;
        case 0x06: JNP(mv, inst.op1); break;
        case 0x07: JNN(mv, inst.op1); break;
    }

    if (mv->registros[IP] != ip_backup && !(inst.codOperacion >= 0x01 && inst.codOperacion <= 0x07)) {
        printf("Advertencia: IP fue modificado por una instruccion que no deberia: 0x%02X\n", inst.codOperacion);
    }

    // Verifico que los saltos no salgan del segmento
    if ((inst.codOperacion >= 0x01 && inst.codOperacion <= 0x07) && !esIPValida(mv)) {
        printf("ERROR: Salto IP fuera del Segmento de Codigo: [%.4X] (%d)\n", mv->registros[IP], mv->registros[IP]);
        mv->ErrorFlag = 2;
    }
}




