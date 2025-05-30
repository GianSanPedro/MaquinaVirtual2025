#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "MVTipos.h"
#include "Operaciones.h"
#include "Funciones.h"


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
            int valor = mv->registros[(int)op.registro];
            switch (op.segmentoReg) {
                case 0: {
                    //printf("LV reg: Leer REGISTRO: %d\n", valor);
                    return valor;                                   // EAX completo
                    break;
                }
                case 1: {                                           // AL
                    valor = valor & 0xFF;
                    valor = valor << 24;
                    valor = valor >> 24;
                    return valor;
                    break;
                }
                case 2: {                                           // AH
                    valor = (valor >> 8) & 0xFF;
                    valor = valor << 24;
                    valor = valor >> 24;
                    return valor;
                    break;
                }
                case 3: {                                           // AX
                    valor = valor & 0xFFFF;
                    valor = valor << 16;
                    valor = valor >> 16;
                    return valor;
                    break;
                }
                default: return 0;
            }
            break;
        }

        case 2: { // Inmediato de 16 bits
            //printf("LV: Leer INMEDIATO: %d\n", op.valor);
            int valor = op.valor & 0xFFFF;
            valor = valor << 16;
            valor = valor >> 16;
            return valor;
            break;
        }
        case 3: { // Memoria dinamica (acceso logico)
            unsigned int selector;
            int offset_registro;

            if (op.registro == 0) {
                // Caso: no se especifico registro base -> usar DS dinamicamente!
                unsigned int ds = mv->registros[DS];
                selector = ds >> 16;
                offset_registro = ds & 0xFFFF;
            } else {
                unsigned int contenido = mv->registros[(int)op.registro];
                selector = contenido >> 16;
                offset_registro = contenido & 0xFFFF;
            }
            //printf("DEBUG: Acceso a memoria: reg base 0x%08X (selector: %d, offset: %d)\n", mv->registros[(int)op.registro], selector, offset_registro);

            int offset_instruc = op.desplazamiento;             // obtenemos el offset (dentro del segmento) del operando de la instruccion
            int base = mv->TDS[selector] >> 16;                 // obtenemos la base fisica del segmento
            int direccion = base + offset_registro + offset_instruc;

            //printf("LV: Leer de direccion fisica: %d (0x%04X)  selector: %d  base: %d  offset_reg: %d  offset_instr: %d\n", direccion, direccion, selector, base, offset_registro, offset_instruc);

            // Determino cuantos bytes leer
            int bytes;
            if (op.tamCelda == 2) {
                bytes = 2;
            } else {
                if (op.tamCelda == 3) {
                    bytes = 1;
                } else {
                    bytes = 4;
                }
            }

            if (esDireccionValida(mv, selector, direccion, bytes)) {
               int val = 0;
                for (int i = 0; i < bytes; i++)
                    val = (val << 8) | (unsigned char)mv->memoria[direccion + i];

                if (bytes < 4)  // extensión de signo
                    val = (val << (32 - 8*bytes)) >> (32 - 8*bytes);
                return val;
            }
            else
                return 0;
            break;
        }
    }
    return 0;
}

void escribirValor(TMV *mv, TOperando op, int valor) {
    if (mv->ErrorFlag) return;
    // No se escribe en inmediatos ni operandos vacios
    switch (op.tipo) {
        case 1: { // Registro
            unsigned int *reg = &mv->registros[(int)op.registro];
            //printf("TEST ESCRIBO REG, valor: %d , segmentOp: %d, tipo: %d \n\n", valor, op.segmentoReg, op.tipo);
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
            break;
        }

        case 3: { // Memoria tamaño variable (acceso logico)
            int selector, offset_registro;

            if (op.registro == 0) {
                // Caso sin registro explicito -> usar DS
                unsigned int ds = mv->registros[DS];
                selector = ds >> 16;
                offset_registro = ds & 0xFFFF;
            } else {
                unsigned int contenido = mv->registros[(int)op.registro];
                selector = contenido >> 16;
                offset_registro = contenido & 0xFFFF;
            }

            int offset_instruc = op.desplazamiento;                     // desplazamiento dentro del segmento
            int base = mv->TDS[selector] >> 16;                         // base fisica del segmento
            int direccion = base + offset_registro + offset_instruc;

            //printf("EV: Escribir en direccion fisica: %d (0x%04X)  selector: %d  base: %d  offset_reg: %d  offset_instr: %d\n", direccion, direccion, selector, base, offset_registro, offset_instruc);
            //printf("EV: Valor a escribir = %d\n", valor);

            // Calcular cuantos bytes escribir segun tamCelda
            int bytes;
            if (op.tamCelda == 2) {
                bytes = 2;
            } else {
                if (op.tamCelda == 3) {
                    bytes = 1;
                } else {
                    bytes = 4;
                }
            }

            // Valido los limites de escritura MOV W[X] [Y]
            if (esDireccionValida(mv, selector, direccion, bytes)) {
                for (int i = 0; i < bytes; i++) {
                    int shift = 8*(bytes - 1 - i);
                    mv->memoria[direccion + i] = (valor >> shift) & 0xFF;
                }
                //printf("EV: Memoria [%d] escrita con %d, verificado: %d\n", direccion, valor, verificado);
            } else {
                printf("EV: ERROR - Direccion invalida al escribir en memoria\n");
                //printf("TEST ESCRIBO MEM, valor: %d , segmentOp: %d, tipo: %d \n\n", valor, op.segmentoReg, op.tipo);
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
        unsigned int ds = mv->registros[DS];    // usar DS por defecto
        selector = ds >> 16;
        offset = ds & 0xFFFF;
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
        unsigned int ds = mv->registros[DS];    // usar DS por defecto
        selector = ds >> 16;
        offset = ds & 0xFFFF;
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

    //printf("modo: %d \n", modo);

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
                unsigned char byte1 = (unsigned char) mv->memoria[direccion];
                unsigned char byte2 = (unsigned char) mv->memoria[direccion + 1];
                unsigned char byte3 = (unsigned char) mv->memoria[direccion + 2];
                unsigned char byte4 = (unsigned char) mv->memoria[direccion + 3];

                valor = (byte1 << 24) | (byte2 << 16) | (byte3 << 8) | byte4;

            }

            printf("[%.4X] : ", direccion);
            if (modo & 0x08) {
                printf("0x%X \t", valor);
            }
            if (modo & 0x04) {
                printf("0o%o \t", valor);
            }
            if (modo & 0x01) {
                printf("%d \t", valor);
            }
            if (modo & 0x02) {
                if (valor >= 32 && valor <= 126)
                    printf("%c \t", valor);
                else
                    printf(". \t");

                //printf("[%.4X] | %c", direccion, valor);
            }
            if (modo & 0x10) {
                printf("0b");
                for (int i = 31; i >= 0; i--) {
                    printf("%d", (valor >> i) & 1);
                    if (i % 8 == 0 && i != 0) printf(" ");  // separador cada 8 bits
                }
                printf("\t");
            }
            //printf("\n>> [%.4X] | 0x%X ", direccion, valor);
            //printf("\n[%.4X] | %d | 0x%X | 0o%o \n", direccion, valor, valor, valor);
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
        printf(">> Fallo de segmento: direccion %d fuera de segmento %d (base %d, tamanio %d)\n", direccion, selector, base, tamanio);
        return 0;
    }

    // Validacion de la memoria real
    if (direccion + tam - 1 >= mv->memSize) {
        mv->ErrorFlag = 2;
        printf(">> Acceso fuera de la RAM: direccion %d excede memoria real (%lu bytes)\n", direccion, mv->memSize);
        return 0;
    }
    //printf("DV: Direccion fisica validada: %d (0x%04X)  selector: %d  BaseSegmento: %d  TamanioSegmento: %d\n", direccion, direccion, selector, base, tamanio);

    return 1; // Direccion valida
}

void leerString(TMV *mv) {
    // Obtengo la direccion de inicio desde EDX
    uint32_t edxVal = mv->registros[EDX];
    uint16_t selector = edxVal >> 16;
    int32_t  offset = edxVal & 0xFFFF;
    uint32_t base = mv->TDS[selector] >> 16;
    uint32_t addr = base + offset;

    // Limite segun CX, si CX == 0xFFFF (-1) no limitamos la lectura
    int32_t ecxVal = mv->registros[ECX];
    int16_t limite = (int16_t)(ecxVal & 0xFFFF);

    //printf("STRING READ \n");

    char Texto[300];
    scanf("%s", Texto);
    int L = strlen(Texto);
    int i = 0;
    uint16_t byte;
    if (limite == -1){
        while( i < L){
            byte = Texto[i];
            mv->memoria[addr + i] = byte;
            i++;
        }
    }
    else{
        while(i < limite && i < L){
            byte = Texto[i];
            mv->memoria[addr + i] = byte;
            i++;
        }
    }

    // Escribimos el terminador nulo
    if (L > limite){
        mv->memoria[addr + i] = '\0';
    }
}

void escribirString(TMV *mv) {
    // Obtengo la direccion inicial desde EDX
    uint32_t edxVal   = mv->registros[EDX];
    uint16_t selector = edxVal >> 16;
    uint16_t offset   = edxVal & 0xFFFF;
    uint32_t base     = mv->TDS[selector] >> 16;
    uint32_t addr     = base + offset;

    //printf("STRING WRITE desde selector %d, offset %d (dir fisica %d)\n", selector, offset, addr);

    // Recorro la memoria hasta el terminador '\0'
    unsigned char c;
    while ((c = mv->memoria[addr++]) != '\0') {
        putchar(c);
    }
}

void limpiarPantalla() {
    // Codigo ANSI:
    //  \x1B[2J  → borra toda la pantalla
    //  \x1B[H   → mueve el cursor a la posición (1,1)
    printf("\x1B[2J\x1B[H"); //chequear!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    fflush(stdout);
}

void breakPoint(TMV *mv) {
    if (!mv->args.nombreArchVmi) {
        return;  // ignoramos el breakpoint
    }

    // Generamos la imagen VMI
    int err = generarImagenVMI(mv, mv->args.nombreArchVmi);
    if (err) {
        printf("ERROR: no se pudo generar imagen VMI '%s'\n", mv->args.nombreArchVmi);
        mv->ErrorFlag = 10;
        return;
    }
    printf(">>Breakpoint: imagen VMI generada en '%s'\n", mv->args.nombreArchVmi);
    char ch;
    do{
        printf("Breakpoint> (g=go, q=quit, Enter=step): ");

        scanf("%c", &ch);
        if (ch == '\n') {
            // step: ejecutar una instrucción mas y volver aqui
            mv->stepMode = 1;
        }
        else if (ch == 'g') {
            // go: continuar hasta proximo breakpoint
            mv->stepMode = 0;
        }
        else if (ch == 'q') {
            // quit: abortar la ejecucion
            mv->Aborted = 1;
        }
    } while (ch != '\n' || ch != 'g' || ch == 'q');
}

int generarImagenVMI(TMV *mv, const char *nombreImagen) {
    FILE *archImagen = fopen(nombreImagen, "wb");
    if (!archImagen) {
        printf("ERROR: no se pudo abrir imagen VMI '%s' para escritura\n", nombreImagen);
        return 1;
    }

    // Header "VMI25" + versión
    fwrite("VMI25", 1, 5, archImagen);
    unsigned char version = 2;
    fwrite(&version, 1, 1, archImagen);

    // Memoria en KiB
    unsigned short memKiB = (unsigned short)(mv->memSize / 1024);
    unsigned char high = (memKiB >> 8) & 0xFF;
    unsigned char low  = (memKiB >> 0) & 0xFF;
    fwrite(&high, 1, 1, archImagen);
    fwrite(&low,  1, 1, archImagen);

    // Registros (16 × 4 bytes, big-endian)
    for (int i = 0; i < 16; i++) {
        uint32_t r = mv->registros[i];
        unsigned char b0 = (r >> 24) & 0xFF;
        unsigned char b1 = (r >> 16) & 0xFF;
        unsigned char b2 = (r >>  8) & 0xFF;
        unsigned char b3 = (r >>  0) & 0xFF;
        fwrite(&b0, 1, 1, archImagen);
        fwrite(&b1, 1, 1, archImagen);
        fwrite(&b2, 1, 1, archImagen);
        fwrite(&b3, 1, 1, archImagen);
    }

    // TDS (8 × 4 bytes, big-endian)
    for (int j = 0; j < 8; j++) {
        uint32_t desc = mv->TDS[j];
        unsigned char b0 = (desc >> 24) & 0xFF;
        unsigned char b1 = (desc >> 16) & 0xFF;
        unsigned char b2 = (desc >>  8) & 0xFF;
        unsigned char b3 = (desc >>  0) & 0xFF;
        fwrite(&b0, 1, 1, archImagen);
        fwrite(&b1, 1, 1, archImagen);
        fwrite(&b2, 1, 1, archImagen);
        fwrite(&b3, 1, 1, archImagen);
    }

    // Memoria principal
    fwrite(mv->memoria, 1, mv->memSize, archImagen);

    fclose(archImagen);
    return 0;
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
        mv->registros[AC]= dividendo % divisor;
    }
}

void CMP(TMV *mv, TOperando op1, TOperando op2) {
    int valor1 = leerValor(mv, op1);
    int valor2 = leerValor(mv, op2);

    int resultado = valor1 - valor2;
    //printf("CMP OP1: %d \n", valor1); EJERCICIO 10 FIBONACCI
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
        case 1: leerDesdeTeclado(mv); break;        //READ
        case 2: escribirEnPantalla(mv); break;      //WRITE
        case 3: leerString(mv); break;              //STRING READ
        case 4: escribirString(mv); break;          //STRING WRITE
        case 7: limpiarPantalla(); break;           //CLEAR SCREEN
        case 15: breakPoint(mv); break;             //BREAKPOINT
        default:
            printf("Error: Llamada al sistema no valida: SYS %d   valor: %d\n", syscall_id, op1.valor);
            mv->ErrorFlag = 4;
    }
}

void JMP(TMV *mv, TOperando op1) {
    int destino = leerValor(mv, op1);
    mv->registros[IP] = (mv->registros[IP] & 0xFFFF0000) | (destino & 0xFFFF);
}

void JZ(TMV *mv, TOperando op1) {
    int cc = mv->registros[CC];

    // Verifica si el bit Z (bit 30) esta encendido
    if (cc & (1 << 30)) {
        int destino = leerValor(mv, op1);
        mv->registros[IP] = (mv->registros[IP] & 0xFFFF0000) | (destino & 0xFFFF);
    }
}

void JP(TMV *mv, TOperando op1) {
    int cc = mv->registros[CC];

    // Verificar que bit N (31) y Z (30) estén en 0
    if ((cc & (1 << 31)) == 0 && (cc & (1 << 30)) == 0) {
        int destino = leerValor(mv, op1);
        mv->registros[IP] = (mv->registros[IP] & 0xFFFF0000) | (destino & 0xFFFF);
    }
}

void JN(TMV *mv, TOperando op1) {
    int cc = mv->registros[CC];

    // Verifica si el bit N (bit 31) esta activado
    if (cc & (1 << 31)) {
        int destino = leerValor(mv, op1);
        mv->registros[IP] = (mv->registros[IP] & 0xFFFF0000) | (destino & 0xFFFF);
    }
}

void JNZ(TMV *mv, TOperando op1) {
    int cc = mv->registros[CC];

    // Verifica si el bit Z (bit 30) esta en 0
    if ((cc & (1 << 30)) == 0) {
        int destino = leerValor(mv, op1);
        mv->registros[IP] = (mv->registros[IP] & 0xFFFF0000) | (destino & 0xFFFF);
    }
}

void JNP(TMV *mv, TOperando op1) {
    int cc = mv->registros[CC];

    // Si N = 1 o Z = 1 → salta
    if ((cc & (1 << 31)) || (cc & (1 << 30))) {
        int destino = leerValor(mv, op1);
        mv->registros[IP] = (mv->registros[IP] & 0xFFFF0000) | (destino & 0xFFFF);
    }
}

void JNN(TMV *mv, TOperando op1) {
    int cc = mv->registros[CC];

    // Verifica si el bit N (bit 31) esta en 0
    if ((cc & (1 << 31)) == 0) {
        int destino = leerValor(mv, op1);
        mv->registros[IP] = (mv->registros[IP] & 0xFFFF0000) | (destino & 0xFFFF);
    }
}

void NOT(TMV *mv, TOperando op1) {
    int valor = leerValor(mv, op1);

    int resultado = ~valor;             // negacion bit a bit

    escribirValor(mv, op1, resultado);

    actualizarNZ(mv, resultado);        // modifica CC
}

void PUSH(TMV *mv, TOperando op1) {
    // Obtengo el selector y offset actuales de SP
    uint32_t sp = mv->registros[SP];
    uint16_t selector = sp >> 16;
    int16_t offset = sp & 0xFFFF;

    // Decrementar SP en 4 bytes (la pila crece hacia direcciones menores)
    int32_t newOff  = (int32_t)offset - 4;
    uint32_t newSP  = ((uint32_t)selector << 16) | ((uint16_t)newOff & 0xFFFF);

    // Comprobamos el stack overflow (offset negativo)
    if (newOff < 0) {
        printf("\n>>ERROR: Stack overflow en PUSH en [%.4X]\n", mv->registros[IP]);
        mv->ErrorFlag = 5;
        return;
    }

    // Actualizar SP
    mv->registros[SP] = newSP;

    // Leer el valor del operando (inmediato, registro o memoria)
    int32_t val = leerValor(mv, op1);

    // Calcular direccion fisica: base del segmento + nuevo offset
    uint32_t base = mv->TDS[selector] >> 16;
    uint32_t addr = base + newOff;

    // Almaceno los 4 bytes en big-endian (MSB primero)
    mv->memoria[addr] = (val >> 24) & 0xFF;     // byte 3 (MSB)
    mv->memoria[addr+1] = (val >> 16) & 0xFF;   // byte 2
    mv->memoria[addr+2] = (val >>  8) & 0xFF;   // byte 1
    mv->memoria[addr+3] = (val >>  0) & 0xFF;   // byte 0 (LSB)
}

void POP(TMV *mv, TOperando op1) {
    // SP actual
    uint32_t sp = mv->registros[SP];
    uint16_t selector = sp >> 16;
    uint16_t offset = sp & 0xFFFF;

    // Comprobamos el STACK UNDERFLOW: debe haber al menos 4 bytes en la pila
    uint16_t segSize  = mv->TDS[selector] & 0xFFFF;

    if (offset >= segSize) {
        printf("\n>>ERROR: Stack underflow en POP en [%.4X]\n", mv->registros[IP]);
        mv->ErrorFlag = 6;
        return;
    }

    // Calculamos la direccion fisica del tope de pila
    uint16_t base = mv->TDS[selector] >> 16;
    uint32_t addr = base + offset;

    // Leemos 4 bytes en big-endian: MSB en addr, LSB en addr+3
    int32_t val = 0;
    for (int i = 0; i < 4; i++){
        val = (val << 8) | (unsigned char)mv->memoria[addr + i];
    }

    // Escribimos el valor extraido en el operando (trunca si < 4 bytes)
    escribirValor(mv, op1, val);

    // Incrementamos el SP en 4 bytes (pila crece hacia direcciones inferiores)
    uint16_t newOff = offset + 4;
    mv->registros[SP] = ((uint32_t)selector << 16) | newOff;
}

void CALL(TMV *mv, TOperando op) {
    // Preparo el operando para pushear el IP
    TOperando ipOp;
    ipOp.tipo         = 1;     // 1 = registro
    ipOp.registro     = IP;    // indice del registro IP
    ipOp.segmentoReg  = 0;     // caso 0: registro completo

    // PUSH IP
    PUSH(mv, ipOp);
    if (mv->ErrorFlag) return;  // abortar si overflow

    // Debemos modificar los 2 bytes menos significativos del IP con el valor del operando
    uint32_t destino = leerValor(mv, op) & 0xFFFF;

    // IP: conservar selector y cambiamos el offset. Seria como un JMP a la subrutina
    mv->registros[IP] = (mv->registros[IP] & 0xFFFF0000) | destino;
}

void RET(TMV *mv, TOperando op_unused) {
    // SP actual
    uint32_t spValue   = mv->registros[SP];
    uint16_t selector  = spValue >> 16;    // selector de SS
    uint16_t offset    = spValue & 0xFFFF; // offset dentro del SS

    // Calculo la direccion fisica del tope de pila
    uint32_t base = mv->TDS[selector] >> 16;
    uint32_t addr = base + offset;

    // Compruebo STACK UNDERFLOW: debe haber al menos 4 bytes
    uint16_t segSize   = mv->TDS[selector] & 0xFFFF;

    if ((uint32_t)offset >= segSize) {
        printf("\n>>ERROR: Stack underflow en RET en [%.4X]\n", mv->registros[IP]);
        mv->ErrorFlag = 6;
        return;
    }

    // Leemos 4 bytes en big-endian y reconstruir el valor de retorno
    int32_t retVal = 0;
    for (int i = 0; i < 4; i++){
        retVal = (retVal << 8) | (unsigned char)mv->memoria[addr + i];
    }


    // Cargamos IP con el valor extraido (salto de retorno)
    mv->registros[IP] = (uint32_t)retVal;

    // Incrementamos el SP en 4 bytes (la pila "sube")
    offset += 4;
    mv->registros[SP] = ((uint32_t)selector << 16) | offset;
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

void imprimirEstado(TMV *mv) {
    printf("\n>> Estado de memoria en DS:\n");

    int ds_selector = mv->registros[DS] >> 16;
    int base = mv->TDS[ds_selector] >> 16;

    for (int offset = 0; offset <= 44; offset += 4) {
        int direccion = base + offset;
        int val = 0;

        val |= ((unsigned char)mv->memoria[direccion])     << 24;
        val |= ((unsigned char)mv->memoria[direccion + 1]) << 16;
        val |= ((unsigned char)mv->memoria[direccion + 2]) << 8;
        val |= ((unsigned char)mv->memoria[direccion + 3]);

        printf("[DS + %2d] = %11d   (0x%08X)\n", offset, val, val);
    }

    printf("\n>> Estado de registros generales:\n");

    const char* nombres[] = {"EAX", "EBX", "ECX", "EDX", "EEX", "EFX"};

    for (int i = 10; i <= 15; i++) {
        int val = mv->registros[i];
        printf("%s = %11d   (0x%08X)\n", nombres[i - 10], val, val);
    }

    printf("\n>> Registro CC:\n");

    int cc = mv->registros[CC];
    int N = (cc >> 31) & 1;
    int Z = (cc >> 30) & 1;

    printf("CC  = %11d   (0x%08X)\n", cc, cc);
    printf("     Flags: N = %d, Z = %d\n", N, Z);
}






