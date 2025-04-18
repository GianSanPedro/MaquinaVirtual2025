#include <stdio.h>
#include <stdlib.h>
#include "MVTipos.h"
#include "Operaciones.h"
#include "Funciones.h"

void DecodificarInstruccion(char instruccion,char *operando1,char *operando2,char *operacion, int *ErrorFlag);
TInstruccion LeerInstruccionCompleta(TMV *MV, int ip);
void reportEstado(int estado);

int main(int argc, char *argv[]){

    FILE *archBinario;
    TMV MV;
    MV.ErrorFlag = 0;
    int modoDisassembler = 0;
    int BandStop = 0;
    char *nombreArchivo = NULL;
    TInstruccion InstruccionActual;                         // Para cargar la instruccion act
    unsigned int ipActual;                                  // Instruction Pointer
    int tamCod;                                             // Para leer el tamanio del codigo
    char *header =(char *)malloc(sizeof(char) * 6);         // 0 - 4 Identificador "VMX25"
                                                            // 5 Version "1"
                                                            // 6 - 7 Tamanio del codigo

    // MODO DISASSEMBLER
    for (int i = 1; i < argc; i++) {                        //argc (argument count)
        if (strcmp(argv[i], "-d") == 0) {                   //argv[] (argument vector)
            modoDisassembler = 1;
        } else {
            nombreArchivo = argv[i];                        // El primer argumento que no sea -d es el archivo
        }
    }
    if (nombreArchivo == NULL) {
        fprintf(stderr, ">> Error: debe especificar el archivo .vmx a ejecutar.\n");
        fprintf(stderr, "Uso: %s [-d] archivo.vmx\n", argv[0]);
        return 1;
    }

    // Cargo en memoria
    archBinario = fopen(nombreArchivo, "rb");
    if (!archBinario) {
        fprintf(stderr, ">> Error: no se pudo abrir el archivo '%s'\n", nombreArchivo);
        return 1;
    }
    fread(header, sizeof(char), 6, archBinario);                    // Obtengo el header (6 bytes)
    unsigned char high, low;
    fread(&high, sizeof(char), 1, archBinario);
    fread(&low, sizeof(char), 1, archBinario);
    tamCod = (high << 8) | low;

    MV.TDS[0] = (0 << 16) | tamCod;                                 // Segmento de código: base = 0, tamaño = tamCod
    MV.TDS[1] = ((unsigned int)tamCod << 16) | (16384 - tamCod);    // Segmento de datos: base = tamCod, tamaño restante

    MV.registros[CS] = (0 << 16);                                   // CS = 0x00000000 → segmento 0, offset 0
    MV.registros[DS] = (1 << 16);                                   // DS = 0x00010000 → segmento 1, offset 0
    MV.registros[IP] = MV.registros[CS];                            // IP apunta a inicio del segmento de código

    fread(MV.memoria, sizeof(char), tamCod, archBinario);           // Carga la totalidad del codigo
    fclose(archBinario);

    // Imprimo Disassembler
    if (modoDisassembler) {
        printf("\n>> Codigo assembler cargado en memoria:\n");
        int ipTemp = 0;
        int Band = 0;
        while (ipTemp < tamCod && !Band) {
            TInstruccion inst = LeerInstruccionCompleta(&MV, ipTemp);
            MostrarInstruccion(inst, MV.memoria);
            if (inst.codOperacion == 0x0F) {
                Band = 1;
            }
            ipTemp += inst.tamanio;
        }
        printf(" Error flag %d\n\n", MV.ErrorFlag);
    }

    // Reiniciar el Instruction Pointer despues de recorrer
    MV.registros[CS] = (0 << 16);                                   // CS = 0x00000000 → segmento 0, offset 0
    MV.registros[DS] = (1 << 16);                                   // DS = 0x00010000 → segmento 1, offset 0
    MV.registros[IP] = MV.registros[CS];                            // IP apunta a inicio del segmento de código

    //Comienza la ejecucion
    printf("\n>> Tamanio codigo: %d\n", tamCod);
    printf("\n>> Tabla de Segmentos seteada\n");
    for (int i = 0; i < 2; i++) {
        int base = MV.TDS[i] >> 16;
        int limite = MV.TDS[i] & 0xFFFF;
        printf("TDS[%d] -> base: %d (0x%04X), tamanio: %d (0x%04X)\n", i, base, base, limite, limite);
    }
    printf("\n>> Codigo assembler en ejecucion:\n");
    while (MV.registros[IP] < tamCod && !BandStop && !MV.ErrorFlag) {
        ipActual = MV.registros[IP];

        // Leo la instrucción desde memoria
        InstruccionActual = LeerInstruccionCompleta(&MV, ipActual);

        // Verifico que no se haya detectado un error en la decodificacion antes de ejecutar
        if (!MV.ErrorFlag){
            // Ejecutar o detectar STOP
            if (InstruccionActual.codOperacion == 0x0F) {
                BandStop = 1;
            } else {
                //MostrarInstruccion(InstruccionActual, MV.memoria);
                //printf("DEBUG IP antes: %d  tamanio instruccion: %d\n", MV.registros[IP], InstruccionActual.tamanio);
                procesarInstruccion(&MV, InstruccionActual);

                // Si no hubo salto, avanzar IP
                if (MV.registros[IP] == ipActual) {
                    MV.registros[IP] += InstruccionActual.tamanio;
                }
                //printf("DEBUG IP despues: %d\n", MV.registros[IP]);
            }
        }
    }

    reportEstado(MV.ErrorFlag);
    free(header);
    return 0;
}

void DecodificarInstruccion(char instruccion,char *tipoOp1,char *tipoOp2,char *CodOperacion, int *ErrorFlag) {
    //Como char instruccion lee solo 1 byte
    *CodOperacion = instruccion & 0x1F;  // 5 bits menos significativos

    if (*CodOperacion == 0x0F) {
        // Sin operandos (STOP)
        *tipoOp1 = 0;
        *tipoOp2 = 0;
    }
    else if (*CodOperacion >= 0x10 && *CodOperacion <= 0x1E) {
        // Dos operandos
        *tipoOp2 = (instruccion >> 6) & 0x03;  // bits 7-6 = tipo operando B
        *tipoOp1 = (instruccion >> 4) & 0x03;  // bits 5-4 = tipo operando A

        // Validacion adicional: solo se permiten tipos A 01 o 11
        if (*tipoOp1 != 0x01 && *tipoOp1 != 0x03) {
            *ErrorFlag = 1; // Tipo de operando A invalido
            printf("\n>> Tipo de operando A invalido\n");
        }
    }
    else if (*CodOperacion <= 0x08) {
        // Un operando
        *tipoOp1 = (instruccion >> 6) & 0x03;  // bits 7-6
        *tipoOp2 = 0;

        // Validacion: bit 5 debe ser 0 (relleno)
        if (((instruccion >> 5) & 0x01) != 0) {
            *ErrorFlag = 1;
            printf("\n>> El bit 5 de la instruccion debe ser 0 (relleno)\n");
        }
    }
    else {
        // Codigo de operacion no valido
        *ErrorFlag = 1;
    }
}

TInstruccion LeerInstruccionCompleta(TMV *MV, int ip) {
    TInstruccion inst;
    inst.ipInicial = ip;

    char tipo1, tipo2;

    // 1. Decodificar cabecera
    DecodificarInstruccion(MV->memoria[ip], &tipo1, &tipo2, &inst.codOperacion, &MV->ErrorFlag);

    // Si hubo un error en la decodificacion, devuelvo la instruccion vacia
    if (MV->ErrorFlag) {
        inst.op1.tipo = 0;
        inst.op2.tipo = 0;
        return inst;
    }
    inst.op1.tipo = tipo1;
    inst.op2.tipo = tipo2;

    // 2. Leer operandos segun tipo (en orden: primero B, luego A)
    int cursor = ip + 1;

    // --- OPERANDO B ---
    switch (inst.op2.tipo) {
        case 1: { // registro
            unsigned char byte = MV->memoria[cursor++];
            inst.op2.registro = (byte >> 4) & 0x0F;
            inst.op2.segmentoReg = (byte >> 2) & 0x03;
            break;
        }
        case 2: {// inmediato
            //inst.op2.valor = MV->memoria[cursor] | (MV->memoria[cursor + 1] << 8);            //little-endian
            inst.op2.valor = (MV->memoria[cursor] << 8) | MV->memoria[cursor + 1];              //big-endian
            cursor += 2;
            break;
        }
        case 3: {// memoria
            //inst.op2.desplazamiento = MV->memoria[cursor] | (MV->memoria[cursor + 1] << 8);   //little-endian
            inst.op2.desplazamiento = (MV->memoria[cursor] << 8) | MV->memoria[cursor + 1];     //big-endian
            unsigned char byte = MV->memoria[cursor + 2];
            inst.op2.registro = (byte >> 4) & 0x0F;
            cursor += 3;
            break;
        }
    }

    // --- OPERANDO A ---
    switch (inst.op1.tipo) {
        case 1: {
            unsigned char byte = MV->memoria[cursor++];
            inst.op1.registro = (byte >> 4) & 0x0F;
            inst.op1.segmentoReg = (byte >> 2) & 0x03;
            break;
        }
        case 2: {
            //inst.op1.valor = MV->memoria[cursor] | (MV->memoria[cursor + 1] << 8);            //little-endian
            inst.op1.valor = (MV->memoria[cursor] << 8) | MV->memoria[cursor + 1];              //big-endian
            cursor += 2;
            if (inst.codOperacion == 0x00) {
                //printf("DEBUG LeerInstruccionCompleta SYS: tipo1 = %d, valor = %d (0x%04X)\n", inst.op1.tipo, inst.op1.valor, inst.op1.valor);
            }
            break;
        }
        case 3: {
            //inst.op1.desplazamiento = MV->memoria[cursor] | (MV->memoria[cursor + 1] << 8);   //little-endian
            inst.op1.desplazamiento = (MV->memoria[cursor] << 8) | MV->memoria[cursor + 1];     //big-endian
            unsigned char byte = MV->memoria[cursor + 2];
            inst.op1.registro = (byte >> 4) & 0x0F;
            cursor += 3;
            break;
        }
    }

    inst.tamanio = cursor - ip;
    return inst;
}

void reportEstado(int estado)
{
    switch(estado)
    {
        case 0: printf("\n\n>> EJECUCION EXITOSA");
        break;
        case 1: printf("\n\n>> INSTRUCCION INVALIDA");
        break;
        case 2: printf("\n\n>> FALLO DE SEGMENTO");
        break;
        case 3: printf("\n\n>> DIVISION POR CERO");
        break;
        default: printf("\n\n>> ERROR DESCONOCIDO");
        break;
   }
}
