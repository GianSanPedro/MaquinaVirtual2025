#include <stdio.h>
#include <stdlib.h>
#include "MVTipos.h"
#include "Operaciones.h"

//#define BASE_SEG(x) ((x) >> 16)
//#define TAM_SEG(x)  ((x) & 0xFFFF)
//int limiteCodigo = BASE_SEG(mv.TDS[0]) + TAM_SEG(mv.TDS[0]);

void DecodificarInstruccion(char instruccion,char *operando1,char *operando2,char *operacion,int *ErrorFlag);
TInstruccion LeerInstruccionCompleta(char *memoria, int ip, int *ErrorFlag);

int main(int argc, char *argv[]){

    FILE *archBinario;
    TMV mv;
    int modoDisassembler = 0;
    int ErrorFlag = 0;                                      // Bandera para detectar errores
    TInstruccion InstruccionActual;                         // Para cargar la instruccion act
    unsigned int ipActual;                                  // Instruction Pointer
    unsigned short tamCod;                                  // Para leer el tamanio del codigo
    char *header =(char *)malloc(sizeof(char) * 6);         // 0 - 4 Identificador "VMX25"
                                                            // 5 Version "1"
                                                            // 6 - 7 Tamanio del codigo
    // Revisar si se paso el parametro -d
    for (int i = 1; i < argc; i++) {                        //argc (argument count)
        if (strcmp(argv[i], "-d") == 0) {                   //argv[] (argument vector)
            modoDisassembler = 1;
        }
    }

    //Cargo en memoria
    archBinario = fopen("filename.vmx","rb");
    fread(header, sizeof(char), 6, archBinario);            // Obtengo el header (6 bytes)
    fread(&tamCod, sizeof(unsigned short), 1, archBinario); // Leo el tamanio del codigo (2 bytes)
    mv.TDS[0] = (0 << 16) | tamCod;                         // Segmento de codigo: base = 0, tamanio = tamCod
    mv.TDS[1] = (tamCod << 16) | (16384 - tamCod);          // Segmento de datos: base = tamCod, tamanio restante

    fread(mv.memoria, sizeof(char), tamCod, archBinario);   // Carga la totalidad del codigo
    fclose(archBinario);
    mv.registros[5] = 0;                                    //PC == 0

    //Comienza la ejecucion
    while (mv.registros[5] < tamCod && !ErrorFlag) {
        ipActual = mv.registros[5];

        InstruccionActual = LeerInstruccionCompleta(mv.memoria, ipActual, &ErrorFlag);
        if (modoDisassembler) {
            MostrarInstruccion(InstruccionActual, mv.memoria);
        }

        if (InstruccionActual.codOperacion == 0x0F) { // STOP
            printf("\n>> Instruccion STOP encontrada. Fin del programa.\n");
            break;
        }

        mv.registros[5] += InstruccionActual.tamanio;
    }

    return 0;
}

void DecodificarInstruccion(char instruccion,char *tipoOp1,char *tipoOp2,char *CodOperacion,int *ErrorFlag) {
    //Como char instruccion lee solo 1 byte
    *CodOperacion = instruccion & 0x1F;  // 5 bits menos significativos

    if (*CodOperacion == 0x0F) {
        // Sin operandos (STOP)
        *tipoOp1 = 0;
        *tipoOp2 = 0;
    }
    else if (*CodOperacion >= 0x10 && *CodOperacion <= 0x1E) {
        // Dos operandos
        *tipoOp2 = (instruccion >> 6) & 0x03;  // bits 7�6 = tipo operando B
        *tipoOp1 = (instruccion >> 4) & 0x03;  // bits 5�4 = tipo operando A

        // Validacion adicional: solo se permiten tipos A 01 o 11
        // Creo  que es innecesario ya que (0x10 <= CodOperacion <= 0x1E)
        if (*tipoOp1 != 0x01 && *tipoOp1 != 0x03) {
            *ErrorFlag = 1; // Tipo de operando A invalido
        }
    }
    else if (*CodOperacion <= 0x08) {
        // Un operando
        *tipoOp1 = (instruccion >> 6) & 0x03;  // bits 7�6
        *tipoOp2 = 0;

        // Validacion: bit 5 debe ser 0 (relleno)
        if (((instruccion >> 5) & 0x01) != 0) {
            *ErrorFlag = 1;
        }
    }
    else {
        // Codigo de operacion no valido
        *ErrorFlag = 1;
    }
}

TInstruccion LeerInstruccionCompleta(char *memoria, int ip, int *ErrorFlag) {
    TInstruccion inst;
    inst.ipInicial = ip;

    char tipo1, tipo2;

    // 1. Decodificar cabecera
    DecodificarInstruccion(memoria[ip], &tipo1, &tipo2, &inst.codOperacion, ErrorFlag);

    inst.op1.tipo = tipo1;
    inst.op2.tipo = tipo2;

    // 2. Leer operandos segun tipo (en orden: primero B, luego A)
    int cursor = ip + 1;

    // --- OPERANDO B ---
    switch (inst.op2.tipo) {
        case 1: { // registro
            unsigned char byte = memoria[cursor++];
            inst.op2.registro = (byte >> 4) & 0x0F;
            inst.op2.segmentoReg = (byte >> 2) & 0x03;
            break;
        }
        case 2: {// inmediato
            //inst.op2.valor = memoria[cursor] | (memoria[cursor + 1] << 8);    //little-endian
            inst.op2.valor = (memoria[cursor] << 8) | memoria[cursor + 1];      //big-endian
            cursor += 2;
            break;
        }
        case 3: {// memoria
            //inst.op2.desplazamiento = memoria[cursor] | (memoria[cursor + 1] << 8); //little-endian
            inst.op2.desplazamiento = (memoria[cursor] << 8) | memoria[cursor + 1]; //big-endian
            unsigned char byte = memoria[cursor + 2];
            inst.op2.registro = (byte >> 4) & 0x0F;
            cursor += 3;
            break;
        }
    }

    // --- OPERANDO A ---
    switch (inst.op1.tipo) {
        case 1: {
            unsigned char byte = memoria[cursor++];
            inst.op1.registro = (byte >> 4) & 0x0F;
            inst.op1.segmentoReg = (byte >> 2) & 0x03;
            break;
        }
        case 2: {
            //inst.op1.valor = memoria[cursor] | (memoria[cursor + 1] << 8);            //little-endian
            inst.op1.valor = (memoria[cursor] << 8) | memoria[cursor + 1];              //big-endian
            cursor += 2;
            break;
        }
        case 3: {
            //inst.op1.desplazamiento = memoria[cursor] | (memoria[cursor + 1] << 8);   //little-endian
            inst.op1.desplazamiento = (memoria[cursor] << 8) | memoria[cursor + 1];     //big-endian
            unsigned char byte = memoria[cursor + 2];
            inst.op1.registro = (byte >> 4) & 0x0F;
            cursor += 3;
            break;
        }
    }

    inst.tamanio = cursor - ip;
    return inst;
}


