#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "MVTipos.h"
#include "Operaciones.h"
#include "Funciones.h"


// En C int son 4 bytes
//      short int 2 bytes
//      char 1 byte

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
    if (mv->ErrorFlag) return;
    switch (op.tipo) {
        case 1: { // Registro
            int valor = mv->registros[op.registro];
            switch (op.segmentoReg) {
                case 0: return valor;                               // EAX completo
                case 1: {                                           // AL
                    valor = valor & 0xFF;
                    valor = valor << 24;
                    valor = valor >> 24;
                    return valor;
                }
                case 2: {                                           // AH
                    valor = (valor >> 8) & 0xFF;
                    valor = valor << 24;
                    valor = valor >> 24;
                    return valor;
                }
                case 3: {                                           // AX
                    valor = op.valor & 0xFFFF;
                    valor = valor << 16;
                    valor = valor >> 16;
                    return valor;
                }
                default: return 0;
            }
        }

        case 2: { // Inmediato de 16 bits
            //printf("LV: Leer inmediato: %d\n", op.valor);
            int valor = op.valor & 0xFFFF;
            valor = valor << 16;
            valor = valor >> 16;
            return valor;
        }
        case 3: { // Memoria siempre 4 bytes (acceso logico)
            int selector, offset_registro;

            if (op.registro == 0) {
                // Caso: no se especifico registro base -> usar DS
                selector = 1;
                offset_registro = 0;
            } else {
                int contenido = mv->registros[op.registro];
                selector = contenido >> 16;
                offset_registro = contenido & 0xFFFF;
            }

            int offset_instruc = op.desplazamiento;   // obtenemos el offset (dentro del segmento) del operando de la instruccion
            int base = mv->TDS[selector] >> 16;       // obtenemos la base fisica del segmento
            int direccion = base + offset_registro + offset_instruc;

            //printf("LV: Leer de direccion fisica: %d (0x%04X)  selector: %d  base: %d  offset_reg: %d  offset_instr: %d\n", direccion, direccion, selector, base, offset_registro, offset_instruc);

            if (esDireccionValida(mv, selector, direccion, 4)) {
                int val = 0;
                val |= mv->memoria[direccion]     << 24;
                val |= mv->memoria[direccion + 1] << 16;
                val |= mv->memoria[direccion + 2] << 8;
                val |= mv->memoria[direccion + 3];
                return val;
            }
            else
                return 0;
        }
    }
}

void escribirValor(TMV *mv, TOperando op, int valor) {
    if (mv->ErrorFlag) return;
    // No se escribe en inmediatos ni operandos vacios
    switch (op.tipo) {
        case 1: { // Registro
            int *reg = &mv->registros[op.registro];

            switch (op.segmentoReg) {
                case 0:                                             // Registro completo (EAX)
                    *reg = valor;
                    break;

                case 1:                                             // Byte bajo (AL)
                    *reg = (*reg & 0xFFFFFF00) | (valor & 0xFF);
                    break;

                case 2:                                             // Byte alto (AH)
                    *reg = (*reg & 0xFFFF00FF) | ((valor & 0xFF) << 8);
                    break;

                case 3:                                             // Palabra baja (AX)
                    *reg = (*reg & 0xFFFF0000) | (valor & 0xFFFF);
                    break;
            }
        }

        case 3: { // Memoria (acceso logico)
            int selector, offset_registro;

            if (op.registro == 0) {
                // Caso sin registro explicito -> usar DS
                selector = 1;
                offset_registro = 0;
            } else {
                int contenido = mv->registros[op.registro];
                selector = contenido >> 16;
                offset_registro = contenido & 0xFFFF;
            }

            int offset_instruc = op.desplazamiento;                     // desplazamiento dentro del segmento
            int base = mv->TDS[selector] >> 16;                         // base fisica del segmento
            int direccion = base + offset_registro + offset_instruc;

            //printf("EV: Escribir en direccion fisica: %d (0x%04X)  selector: %d  base: %d  offset_reg: %d  offset_instr: %d\n", direccion, direccion, selector, base, offset_registro, offset_instruc);
            //printf("EV: Valor a escribir = %d\n", valor);

            // Valido los limites de escritura
            if (esDireccionValida(mv, selector, direccion, 4)) {
                mv->memoria[direccion]     = (valor >> 24) & 0xFF;
                mv->memoria[direccion + 1] = (valor >> 16) & 0xFF;
                mv->memoria[direccion + 2] = (valor >> 8) & 0xFF;
                mv->memoria[direccion + 3] = valor & 0xFF;

                // Verificar post-escritura
                //int verificado = leerValor(mv, op);
                //printf("EV: Memoria [%d] escrita con %d, verificado: %d\n", direccion, valor, verificado);
            } else {
                printf("EV: ERROR - Dirección invalida al escribir en memoria\n");
            }
            break;
        }
    }
}

void leerDesdeTeclado(TMV *mv) {
    if (mv->ErrorFlag) return;
    int edxVal = mv->registros[EDX];

    int selector, offset;
    if (edxVal == 0) {
        selector = 1;                           // usar DS por defecto
        offset = 0;
    } else {
        selector = edxVal >> 16;                // indice de la TDS
        offset   = edxVal & 0xFFFF;             // offset dentro del segmento
    }

    int base = mv->TDS[selector] >> 16;         // Base fisica real del segmento

    int ecx  = mv->registros[ECX];
    int cant = ecx & 0xFF;                      // CL: cantidad
    int tam  = (ecx >> 8) & 0xFF;               // CH: tamaño por celda

    int eax  = mv->registros[EAX];
    int modo = eax & 0xFF;                      // AL: modo de impresion

    for (int i = 0; i < cant; i++) {
        int direccion = base + offset + i * tam;
        int valor = 0;

        // Valido los limites de escritura
        if (esDireccionValida(mv, selector, direccion, tam)) {
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
                valor = (int)c;
            }

            // Escribir en memoria segun tamaño
            // Extiendo el valor si tam < 4
            if (tam == 1) {
                mv->memoria[direccion] = valor & 0xFF;
            } else if (tam == 2) {
                mv->memoria[direccion]     = (valor >> 8) & 0xFF;
                mv->memoria[direccion + 1] = valor & 0xFF;
            } else if (tam == 4) {
                mv->memoria[direccion]     = (valor >> 24) & 0xFF;
                mv->memoria[direccion + 1] = (valor >> 16) & 0xFF;
                mv->memoria[direccion + 2] = (valor >> 8)  & 0xFF;
                mv->memoria[direccion + 3] = valor & 0xFF;
            }
        }
    }
}

void escribirEnPantalla(TMV *mv) { //Imprimir
    if (mv->ErrorFlag) return;
    int edxVal = mv->registros[EDX];

    int selector, offset;
    if (edxVal == 0) {
        selector = 1;                           // usar DS
        offset = 0;
    } else {
        selector = edxVal >> 16;                // indice del segmento
        offset   = edxVal & 0xFFFF;             // desplazamiento dentro del segmento
    }

    int base = mv->TDS[selector] >> 16;         // base fisica del segmento

    int ecx  = mv->registros[ECX];
    int cant = ecx & 0xFF;                      // CL: cantidad
    int tam  = (ecx >> 8) & 0xFF;               // CH: tamaño por celda

    int eax  = mv->registros[EAX];
    int modo = eax & 0xFF;                      // AL: modo de impresion

    for (int i = 0; i < cant; i++) {
        int direccion = base + offset + i * tam;
        int valor = 0;

        if (esDireccionValida(mv, selector, direccion, tam)) {
            if (tam == 1) {
                valor = mv->memoria[direccion];
            }
            else if (tam == 2) {
                valor = (mv->memoria[direccion] << 8) | mv->memoria[direccion + 1];
            }
            else if (tam == 4) {
                valor = (mv->memoria[direccion]     << 24) |
                        (mv->memoria[direccion + 1] << 16) |
                        (mv->memoria[direccion + 2] << 8)  |
                        mv->memoria[direccion + 3];
            }

            //printf("[%.4X]: ", direccion);

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
}

int esDireccionValida(TMV *mv, int selector, int direccion, int tam) {
    int base     = mv->TDS[selector] >> 16;
    int tamanio  = mv->TDS[selector] & 0xFFFF;

    // Validacion del segmento
    if (direccion < base || direccion + tam - 1 >= base + tamanio) {
        mv->ErrorFlag = 2;
        printf(">> ❌ Fallo de segmento: dirección %d fuera de segmento %d (base %d, tamaño %d)\n",
               direccion, selector, base, tamanio);
        return 0;
    }

    // Validacion de la memoria real
    if (direccion + tam - 1 >= sizeof(mv->memoria)) {
        mv->ErrorFlag = 2;
        printf(">> ❌ Acceso fuera de la RAM: dirección %d excede memoria real (16384 bytes)\n", direccion);
        return 0;
    }
    //printf("DV: Direccion fisica validada: %d (0x%04X)  selector: %d  BaseSegmento: %d  TamanioSegmento: %d\n", direccion, direccion, selector, base, tamanio);

    return 1; // Direccion valida
}

void MOV(TMV *mv, TOperando op1, TOperando op2) {
    int valor = leerValor(mv, op2);
    escribirValor(mv, op1, valor);
}

void ADD(TMV *mv, TOperando op1, TOperando op2) {
    int valor1 = leerValor(mv, op1);
    int valor2 = leerValor(mv, op2);

    //printf("DEBUG ADD: valor1 = %d, valor2 = %d\n", valor1, valor2);

    int resultado = valor1 + valor2;

    escribirValor(mv, op1, resultado);
    actualizarNZ(mv, resultado);

    //printf("ADD: valor final en destino (tipo %d, desp %d) = %d\n", op1.tipo, op1.desplazamiento, leerValor(mv, op1));
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
        mv->ErrorFlag = 3;
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

    //printf("SHL: [%d] << %d = %d\n", valor, desplazamiento, resultado);

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
            printf("Error: Llamada al sistema no valida: SYS %d   valor: %d\n", syscall_id, op1.valor);
            mv->ErrorFlag = 4;
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


void imprimirRegistrosGenerales(TMV *mv) {
    const char* nombres[] = {"EAX", "EBX", "ECX", "EDX", "EEX", "EFX"};
    printf("\n>> Estado de registros generales:\n");
    for (int i = 10; i <= 15; i++) {
        unsigned int reg = mv->registros[i];
        unsigned int selector = reg >> 16;
        unsigned int offset   = reg & 0xFFFF;
        printf("%s = 0x%08X (%d)  [selector: %u | offset: %u]\n", nombres[i - 10], reg, reg, selector, offset);
    }
}
/*
void imprimirRegistrosGenerales(TMV *mv) {
    const char* nombres[] = {"EAX", "EBX", "ECX", "EDX", "EEX", "EFX"};
    printf("\n>> Estado de registros generales:\n");
    int i = 13; //EDX
    unsigned int reg = mv->registros[i];
    unsigned int selector = reg >> 16;
    unsigned int offset   = reg & 0xFFFF;
    printf("%s = 0x%08X (%d)  [selector: %u | offset: %u]\n", nombres[i - 10], reg, reg, selector, offset);

    i = 15; //EFX
    reg = mv->registros[i];
    selector = reg >> 16;
    offset   = reg & 0xFFFF;
    printf("%s = 0x%08X (%d)  [selector: %u | offset: %u]\n", nombres[i - 10], reg, reg, selector, offset);

}
*/





