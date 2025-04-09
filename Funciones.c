#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "MVTipos.h"

void actualizarNZ(TMV *mv, int resultado) {
    // Limpiar bits 30 (Z) y 31 (N)
    // 0011...1111 (limpia los 2 bits superiores)
    mv->registros[CC] &= 0x3FFFFFFF;

    // Si es cero, activar bit Z (bit 30)
    if (resultado == 0)
        mv->registros[CC] |= (1 << 30);

    // Si es negativo, activar bit N (bit 31)
    if (resultado < 0)
        mv->registros[CC] |= (1 << 31);
}

int leerValor(TMV *mv, TOperando op) {
    switch (op.tipo) {
        case 1: // Registro
            return mv->registros[op.registro];

        case 2: // Inmediato
            return op.valor;

        case 3: { // Memoria
            int base = mv->TDS[op.registro] >> 16;
            int direccion = base + op.desplazamiento;
            int val = 0;
            val |= mv->memoria[direccion] << 24;
            val |= mv->memoria[direccion + 1] << 16;
            val |= mv->memoria[direccion + 2] << 8;
            val |= mv->memoria[direccion + 3];
            return val;
        }
        default:
            return 0;
    }
}

void escribirValor(TMV *mv, TOperando op, int valor) {
    // No se escribe en inmediatos ni operandos vacios
    switch (op.tipo) {
        case 1: // Registro
            mv->registros[op.registro] = valor;
            break;

        case 3: { // Memoria
            int base = mv->TDS[op.registro] >> 16;
            int direccion = base + op.desplazamiento;
            mv->memoria[direccion]     = (valor >> 24) & 0xFF;
            mv->memoria[direccion + 1] = (valor >> 16) & 0xFF;
            mv->memoria[direccion + 2] = (valor >> 8)  & 0xFF;
            mv->memoria[direccion + 3] = valor & 0xFF;
            break;
        }
    }
}

void leerDesdeTeclado(TMV *mv) {
    int base   = mv->registros[EDX] >> 16;
    int offset = mv->registros[EDX] & 0xFFFF;

    int ecx  = mv->registros[ECX];
    int cant = ecx & 0xFF;           // CL
    int tam  = (ecx >> 8) & 0xFF;    // CH

    int eax  = mv->registros[EAX];
    int modo = eax & 0xFF;           // AL

    for (int i = 0; i < cant; i++) {
        int direccion = base + offset + i * tam;
        int valor = 0;

        printf("[%.4X]: ", direccion);

        if (modo & 0x10 || modo & 0x01) {
            scanf("%d", &valor);
        } else if (modo & 0x08) {
            scanf("%x", &valor);
        } else if (modo & 0x04) {
            scanf("%o", &valor);
        } else if (modo & 0x02) {
            char c;
            scanf(" %c", &c);
            valor = c;
        }

        // Escribir segun tamaño
        if (tam == 1) {
            mv->memoria[direccion] = valor & 0xFF;
        } else if (tam == 2) {
            mv->memoria[direccion]     = (valor >> 8) & 0xFF;
            mv->memoria[direccion + 1] = valor & 0xFF;
        } else if (tam == 4) {
            mv->memoria[direccion]     = (valor >> 24) & 0xFF;
            mv->memoria[direccion + 1] = (valor >> 16) & 0xFF;
            mv->memoria[direccion + 2] = (valor >> 8) & 0xFF;
            mv->memoria[direccion + 3] = valor & 0xFF;
        }
    }
}

void escribirEnPantalla(TMV *mv) {
    int base   = mv->registros[EDX] >> 16;
    int offset = mv->registros[EDX] & 0xFFFF;

    int ecx  = mv->registros[ECX];
    int cant = ecx & 0xFF;           // CL
    int tam  = (ecx >> 8) & 0xFF;    // CH

    int eax  = mv->registros[EAX];
    int modo = eax & 0xFF;           // AL

    for (int i = 0; i < cant; i++) {
        int direccion = base + offset + i * tam;
        int valor = 0;

        if (tam == 1) {
            valor = mv->memoria[direccion];
        } else if (tam == 2) {
            valor = (mv->memoria[direccion] << 8) |
                    mv->memoria[direccion + 1];
        } else if (tam == 4) {
            valor = (mv->memoria[direccion] << 24) |
                    (mv->memoria[direccion + 1] << 16) |
                    (mv->memoria[direccion + 2] << 8) |
                    mv->memoria[direccion + 3];
        }

        printf("[%.4X]: ", direccion);

        if (modo & 0x10 || modo & 0x01) {
            printf("%d", valor);
        } else if (modo & 0x08) {
            printf("0x%X", valor);
        } else if (modo & 0x04) {
            printf("0o%o", valor);
        } else if (modo & 0x02) {
            if (valor >= 32 && valor <= 126)
                printf("%c", valor);
            else
                printf(".");
        }

        printf("\n");
    }
}

void MOV(TMV *mv, TOperando op1, TOperando op2) {
    // asumiendo que cualquier acceso a memoria es valido
    int valor = leerValor(mv, op2);
    escribirValor(mv, op1, valor);
}

void ADD(TMV *mv, TOperando op1, TOperando op2) {
    int valor1 = leerValor(mv, op1);
    int valor2 = leerValor(mv, op2);

    int resultado = valor1 + valor2;

    escribirValor(mv, op1, resultado);

    actualizarNZ(mv, resultado);
}

void SUB(TMV *mv, TOperando op1, TOperando op2) {
    int valor1 = leerValor(mv, op1);
    int valor2 = leerValor(mv, op2);

    int resultado = valor1 - valor2;

    escribirValor(mv, op1, resultado);

    actualizarNZ(mv, resultado);
}

void SWAP(TMV *mv, TOperando op1, TOperando op2) {
    int valor1 = leerValor(mv, op1);
    int valor2 = leerValor(mv, op2);

    escribirValor(mv, op1, valor2);
    escribirValor(mv, op2, valor1);
}

void MUL(TMV *mv, TOperando op1, TOperando op2) {
    int valor1 = leerValor(mv, op1);
    int valor2 = leerValor(mv, op2);

    int resultado = valor1 * valor2;

    escribirValor(mv, op1, resultado);

    actualizarNZ(mv, resultado);
}

void DIV(TMV *mv, TOperando op1, TOperando op2) {
    int dividendo = leerValor(mv, op1);
    int divisor   = leerValor(mv, op2);

    if (divisor == 0) {
        printf("Error: Division por cero\n");
        mv->ErrorFlag = 1;
    }
    else{
        int resultado = dividendo / divisor;

        escribirValor(mv, op1, resultado);
        actualizarNZ(mv, resultado);
    }
}

void CMP(TMV *mv, TOperando op1, TOperando op2) {
    int valor1 = leerValor(mv, op1);
    int valor2 = leerValor(mv, op2);

    int resultado = valor1 - valor2;

    actualizarNZ(mv, resultado);
}

void SHL(TMV *mv, TOperando op1, TOperando op2) {
    int valor = leerValor(mv, op1);
    int desplazamiento = leerValor(mv, op2);

    int resultado = valor << desplazamiento;

    escribirValor(mv, op1, resultado);

    actualizarNZ(mv, resultado);
}

void SHR(TMV *mv, TOperando op1, TOperando op2) {
    int valor = leerValor(mv, op1);
    int desplazamiento = leerValor(mv, op2);

    // Shift aritmetico (preserva el signo si valor es negativo)
    int resultado = valor >> desplazamiento;

    escribirValor(mv, op1, resultado);

    actualizarNZ(mv, resultado);
}

void AND(TMV *mv, TOperando op1, TOperando op2) {
    int valor1 = leerValor(mv, op1);
    int valor2 = leerValor(mv, op2);

    int resultado = valor1 & valor2;

    escribirValor(mv, op1, resultado);

    actualizarNZ(mv, resultado);
}

void OR(TMV *mv, TOperando op1, TOperando op2) {
    int valor1 = leerValor(mv, op1);
    int valor2 = leerValor(mv, op2);

    int resultado = valor1 | valor2;

    escribirValor(mv, op1, resultado);
    actualizarNZ(mv, resultado);
}

void XOR(TMV *mv, TOperando op1, TOperando op2) {
    int valor1 = leerValor(mv, op1);
    int valor2 = leerValor(mv, op2);

    int resultado = valor1 ^ valor2;

    escribirValor(mv, op1, resultado);
    actualizarNZ(mv, resultado);
}

void LDL(TMV *mv, TOperando op1, TOperando op2) {
    int original = leerValor(mv, op1);
    int fuente = leerValor(mv, op2);

    // Extraer 16 bits menos significativos del segundo operando
    int low16 = fuente & 0xFFFF;

    // Conservar los 16 bits altos del original y combinar
    int resultado = (original & 0xFFFF0000) | low16;

    escribirValor(mv, op1, resultado);
}

void LDH(TMV *mv, TOperando op1, TOperando op2) {
    int original = leerValor(mv, op1);
    int fuente = leerValor(mv, op2);

    // Obtener 16 bits menos significativos de op2
    int low16 = fuente & 0xFFFF;

    // Mover esos 16 bits a la parte alta
    int high16 = low16 << 16;

    // Combinar con la parte baja del original
    int resultado = (original & 0x0000FFFF) | high16;

    escribirValor(mv, op1, resultado);
}

void RND(TMV *mv, TOperando op1, TOperando op2) {
    int limite = leerValor(mv, op2);

    if (limite <= 0) {
        printf("Error en RND: el limite debe ser mayor a cero\n");
        mv->ErrorFlag = 1;
        return;
    }

    int aleatorio = rand() % limite;

    escribirValor(mv, op1, aleatorio);
}

void SYS(TMV *mv, TOperando op1) {
    int syscall_id = leerValor(mv, op1);

    switch (syscall_id) {
        case 1: leerDesdeTeclado(mv); break;
        case 2: escribirEnPantalla(mv); break;
        default:
            printf("Error: Llamada al sistema no valida: SYS %d\n", syscall_id);
            mv->ErrorFlag = 1;
    }
}

void JMP(TMV *mv, TOperando op1) {
    int destino = leerValor(mv, op1);
    mv->registros[IP] = destino;
}

void JZ(TMV *mv, TOperando op1) {
    int cc = mv->registros[CC];

    // Verifica si el bit Z (bit 30) esta encendido
    if (cc & (1 << 30)) {
        int destino = leerValor(mv, op1);
        mv->registros[IP] = destino;
    }
}

void JP(TMV *mv, TOperando op1) {
    int cc = mv->registros[CC];

    // Verificar que bit N (31) y Z (30) estén en 0
    if ((cc & (1 << 31)) == 0 && (cc & (1 << 30)) == 0) {
        int destino = leerValor(mv, op1);
        mv->registros[IP] = destino;
    }
}

void JN(TMV *mv, TOperando op1) {
    int cc = mv->registros[CC];

    // Verifica si el bit N (bit 31) esta activado
    if (cc & (1 << 31)) {
        int destino = leerValor(mv, op1);
        mv->registros[IP] = destino;
    }
}

void JNZ(TMV *mv, TOperando op1) {
    int cc = mv->registros[CC];

    // Verifica si el bit Z (bit 30) esta en 0
    if ((cc & (1 << 30)) == 0) {
        int destino = leerValor(mv, op1);
        mv->registros[IP] = destino;
    }
}

void JNP(TMV *mv, TOperando op1) {
    int cc = mv->registros[CC];

    // Si N = 1 o Z = 1 → salta
    if ((cc & (1 << 31)) || (cc & (1 << 30))) {
        int destino = leerValor(mv, op1);
        mv->registros[IP] = destino;
    }
}

void JNN(TMV *mv, TOperando op1) {
    int cc = mv->registros[CC];

    // Verifica si el bit N (bit 31) esta en 0
    if ((cc & (1 << 31)) == 0) {
        int destino = leerValor(mv, op1);
        mv->registros[IP] = destino;
    }
}

void NOT(TMV *mv, TOperando op1) {
    int valor = leerValor(mv, op1);

    int resultado = ~valor;             // negacion bit a bit

    escribirValor(mv, op1, resultado);

    actualizarNZ(mv, resultado);        // modifica CC
}








