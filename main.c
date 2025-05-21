#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include "MVTipos.h"
#include "Operaciones.h"
#include "Funciones.h"

void DecodificarInstruccion(char instruccion,char *operando1,char *operando2,char *operacion, int *ErrorFlag);
TInstruccion LeerInstruccionCompleta(TMV *MV, int ip);
int esIPValida(TMV *mv);
void reportEstado(int estado);
TArgs parsearArgumentos(int argc, char *argv[]);
int cargarArchivoVMX(const char *nombreArch, TMV *MV, int *tamCod);
void configurarSegmentos(TMV *mv, unsigned short tamConst, unsigned short tamCode, unsigned short tamData, unsigned short tamExtra, unsigned short tamStack, unsigned short entryOffset);
void initSubRutinaPrincipal(TMV *mv);
int cargarImagenVMI(const char *nombreImagen, TMV *MV, int *tamCod);

int main(int argc, char *argv[]){

    TMV MV;
    MV.ErrorFlag = 0;
    MV.memoria = NULL;
    MV.memSize = 0;
    size_t memBytes;
    int BandStop = 0;
    TInstruccion InstruccionActual;                         // Para cargar la instruccion act
    unsigned int ipActual;                                  // Instruction Pointer
    int tamCod;                                             // Para leer el tamanio del codigo

    // Lectura argumentos de la Consola, necesitare el vmi para el breakPoint
    MV.args = parsearArgumentos(argc, argv);
    MV.stepMode = 0;
    MV.Aborted  = 0;

    if (!MV.args.nombreArchVmx && !MV.args.nombreArchVmi) {
        printf(">> Error: se requiere al menos .vmx o .vmi\n");
        return 1;
    }
    if (!MV.args.nombreArchVmx && MV.args.cantidadParametros > 0) {
        printf("Aviso: sin .vmx, se ignoran %d parámetro(s)\n",MV.args.cantidadParametros);
        //free(MV.args.parametros); libero al final
        MV.args.parametros = NULL;
        MV.args.cantidadParametros = 0;
    }
    printf("VMX: %s\n", MV.args.nombreArchVmx ? MV.args.nombreArchVmx : "(ninguno)");
    printf("VMI: %s\n", MV.args.nombreArchVmi ? MV.args.nombreArchVmi : "(ninguno)");
    //printf("Memoria: %d KiB\n", args.tamMemoriaKiB);
    if (MV.args.cantidadParametros) {
        printf("Parametros (%d): ", MV.args.cantidadParametros);
        for (int i = 0; i < MV.args.cantidadParametros; i++)
            printf("%s ", MV.args.parametros[i]);
        printf("\n");
    }

    // Caso A: Solo Imagen .vmi
    if (!MV.args.nombreArchVmx && MV.args.nombreArchVmi) {
        int err = cargarImagenVMI(MV.args.nombreArchVmi, &MV, &tamCod);
        if (err)
            return err;
    }
    else{
        // Caso B: Tenemos .vmx y opcionalmente .vmi para depuracion mas adelante
        memBytes = MV.args.tamMemoriaKiB * 1024;
        MV.memoria = malloc(memBytes);
        MV.memSize = memBytes;
        if (!MV.memoria) {
            printf(">> Error: no se pudo reservar %lu bytes de memoria\n",(unsigned long)memBytes);
            return 1;
        }

        int err = cargarArchivoVMX(MV.args.nombreArchVmx, &MV, &tamCod);
        if (err) //despues planteo mensaje de posibles errores
            return err;
    }

    // CODIGO VIEJO DESPUES ACTUALIZO!

    // Imprimo Disassembler
    if (MV.args.modoDisassembler) {
        Disassembler(&MV, tamCod);
    }

    //Comienza la ejecucion
    printf("\n>> Tamanio codigo: %d\n", tamCod);

    printf("\n>> Tabla de Segmentos seteada\n");
    for (int i = 0; i < 7; i++) {
        int base = MV.TDS[i] >> 16;
        int limite = MV.TDS[i] & 0xFFFF;
        printf("TDS[%d] -> base: %d (0x%04X), tamanio: %d (0x%04X)\n", i, base, base, limite, limite);
    }

    printf("\n>> Codigo assembler en ejecucion:\n");
    if (MV.version == 2){
        while (MV.registros[IP] < tamCod && !BandStop && !MV.ErrorFlag && !MV.Aborted) {
            ipActual = MV.registros[IP];
            if (!esIPValida(&MV)) {
                printf("ERROR: IP fuera del Segmento de Codigo: [%.4X] (%d)\n", MV.registros[IP], MV.registros[IP]);
                MV.ErrorFlag = 2;
            }

            InstruccionActual = LeerInstruccionCompleta(&MV, ipActual);

            MV.registros[IP] += InstruccionActual.tamanio;

            // Verifico que no se haya detectado un error en la decodificacion antes de ejecutar
            if (!MV.ErrorFlag){

                // Ejecutar o detectar STOP
                if (InstruccionActual.codOperacion == 0x0F) {
                    BandStop = 1;
                } else {
                    procesarInstruccion(&MV, InstruccionActual);
                }
            }

            if (MV.stepMode && !MV.Aborted && !BandStop && !MV.ErrorFlag) {
                breakPoint(&MV);
            }
        }
        if (MV.Aborted) {
            printf("Ejecución abortada por usuario.\n");
        } else if (MV.ErrorFlag) {
            printf("Ejecución detenida por error.\n");
        } else if (BandStop) {
            printf("Ejecución detenida por STOP (opcode 0x0F).\n");
        }
    }
    else{
        while (MV.registros[IP] < tamCod && !BandStop && !MV.ErrorFlag) {
            ipActual = MV.registros[IP];
            if (!esIPValida(&MV)) {
                printf("ERROR: IP fuera del Segmento de Codigo: [%.4X] (%d)\n", MV.registros[IP], MV.registros[IP]);
                MV.ErrorFlag = 2;
            }
            // Leo la instruccion desde memoria
            InstruccionActual = LeerInstruccionCompleta(&MV, ipActual);

            MV.registros[IP] += InstruccionActual.tamanio;

            // Verifico que no se haya detectado un error en la decodificacion antes de ejecutar
            if (!MV.ErrorFlag){

                // Ejecutar o detectar STOP
                if (InstruccionActual.codOperacion == 0x0F) {
                    BandStop = 1;
                } else {
                    //printf("\n");
                    //printf("\nIP ACTUAL: %d: \n", ipActual);
                    //MostrarInstruccion(InstruccionActual, MV.memoria);
                    procesarInstruccion(&MV, InstruccionActual);
                    //imprimirEstado(&MV);
                    //imprimirRegistrosGenerales(&MV);
                    //printf("\n");
                }
            }
        }
    }

    reportEstado(MV.ErrorFlag);
    free(MV.args.parametros);
    free(MV.memoria);
    return 0;
}

void DecodificarInstruccion(char instruccion,char *tipoOp1,char *tipoOp2,char *CodOperacion, int *ErrorFlag) {
    //Como char instruccion lee solo 1 byte
    *CodOperacion = instruccion & 0x1F;  // 5 bits menos significativos

    if (*CodOperacion == 0x0F || *CodOperacion == 0x0E) {
        // Sin operandos (STOP o RET)
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
    else if ((*CodOperacion <= 0x08)  || (*CodOperacion >= 0x0B && *CodOperacion <= 0x0D)) {
        // Un operando
        *tipoOp1 = (instruccion >> 6) & 0x03;  // bits 7-6
        *tipoOp2 = 0;

        // Validacion: bit 5 debe ser 0 (relleno)
        if (((instruccion >> 5) & 0x01) != 0) {
            *ErrorFlag = 1;
            printf("\n>> CodOp 1 operando: El bit 5 de la instruccion debe ser 0 (relleno)\n");
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
        case 2: { // inmediato
            unsigned char byte1 = (unsigned char) MV->memoria[cursor];
            unsigned char byte2 = (unsigned char) MV->memoria[cursor + 1];
            inst.op2.valor = (byte1 << 8) | byte2;
            cursor += 2;
            break;
        }
        case 3: { // memoria
            unsigned char byte1 = (unsigned char) MV->memoria[cursor];
            unsigned char byte2 = (unsigned char) MV->memoria[cursor + 1];
            inst.op2.desplazamiento = (byte1 << 8) | byte2;

            unsigned char byte = (unsigned char) MV->memoria[cursor + 2];
            inst.op2.registro = (byte >> 4) & 0x0F;
            inst.op2.tamCelda =  byte & 0x03;   // 00=4B, 10=2B, 11=1B

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
        case 2: { // inmediato
            unsigned char byte1 = (unsigned char) MV->memoria[cursor];
            unsigned char byte2 = (unsigned char) MV->memoria[cursor + 1];
            inst.op1.valor = (byte1 << 8) | byte2;
            cursor += 2;
            break;
        }
        case 3: { // memoria
            unsigned char byte1 = (unsigned char) MV->memoria[cursor];
            unsigned char byte2 = (unsigned char) MV->memoria[cursor + 1];
            inst.op1.desplazamiento = (byte1 << 8) | byte2;

            unsigned char byte = (unsigned char) MV->memoria[cursor + 2];
            inst.op1.registro = (byte >> 4) & 0x0F;
            inst.op1.tamCelda =  byte & 0x03;   // 00=4B, 10=2B, 11=1B

            cursor += 3;
            break;
        }
    }

    inst.tamanio = cursor - ip;
    return inst;
}

int esIPValida(TMV *MV) {
    uint32_t  csReg = MV->registros[CS];
    uint32_t  selectorCS = csReg >> 16;                  // selector
    uint32_t  baseCS = MV->TDS[selectorCS] >> 16;        // base fisica del segmento

    uint32_t tam = MV->TDS[selectorCS] & 0xFFFF;
    uint32_t ip = MV->registros[IP];
    return (ip >= baseCS && ip < baseCS + tam);
}

void reportEstado(int estado){
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
        case 4: printf("\n\n>> MEMORIA INSUFICIENTE");
        break;
        case 5: printf("\n\n>> STACK OVERFLOW");
        break;
        case 6: printf("\n\n>> STACK UNDERFLOW");
        break;
        default: printf("\n\n>> ERROR DESCONOCIDO");
        break;
   }
}

TArgs parsearArgumentos(int argc, char *argv[]) {
    TArgs a = {0};
    a.nombreArchVmx       = NULL;   // Nombre de archivo .vmx (o NULL)
    a.nombreArchVmi       = NULL;   // Nombre de archivo .vmi (o NULL)
    a.tamMemoriaKiB       = 16;     // 16 KiB por defecto
    a.modoDisassembler    = 0;      // Flag -d: deshabilitado
    a.parametros          = NULL;   // Array de parámetros
    a.cantidadParametros  = 0;      // Cantidad de parámetros

    int parsingParams = 0;          // Se activa tras "-p"

    for (int i = 1; i < argc; i++) {
        // Si ya vimos "-p", todos los argv[i] siguientes son parámetros
        if (parsingParams) {
            a.parametros = realloc(a.parametros, sizeof(char*) * (a.cantidadParametros + 1));
            a.parametros[a.cantidadParametros++] = argv[i];
            continue;
        }

        // 1) Modo desensamblador ("-d")
        if (strcmp(argv[i], "-d") == 0) {
            a.modoDisassembler = 1;
            continue;
        }

        // 2) Tamaño de memoria ("m=M")
        if (strncmp(argv[i], "m=", 2) == 0) {
            char *num = argv[i] + 2;
            int valido = 1;
            for (char *p = num; *p; p++) {
                if (!isdigit(*p)) { valido = 0; break; }
            }
            if (valido) {
                a.tamMemoriaKiB = atoi(num);
            } else {
                fprintf(stderr,"Aviso: 'm=%s' inválido, usando 16 KiB\n", num);
            }
            continue;
        }

        // 3) Inicio de parámetros ("-p")
        if (strcmp(argv[i], "-p") == 0) {
            parsingParams = 1;
            continue;
        }

        // 4) Archivo .vmx
        size_t len = strlen(argv[i]);
        if (len > 4 && strcmp(argv[i] + len - 4, ".vmx") == 0) {
            a.nombreArchVmx = argv[i];
            continue;
        }

        // 5) Archivo .vmi
        if (len > 4 && strcmp(argv[i] + len - 4, ".vmi") == 0) {
            a.nombreArchVmi = argv[i];
            continue;
        }

        // 6) Argumento desconocido
        fprintf(stderr, "Advertencia: argumento desconocido '%s'\n", argv[i]);
    }

    return a;
}

int cargarArchivoVMX(const char *nombreArch, TMV *MV, int *tamCod) {
    FILE *archVmx = fopen(nombreArch, "rb");
    if (!archVmx) {
        printf("\n>> Error: arch vmx no se pudo abrir '%s'\n", nombreArch);
        return 1;
    }

    // Leo el identificador "VMX25" (5 bytes) y version (1 byte)
    char identificador[5];
    char version;
    fread(identificador, 1, 5, archVmx);
    fread(&version, sizeof(version), 1, archVmx);
    MV->version = version;

    // Variables para tamaños y bytes temporales, tal vez despues lo guarde en un registro especial
    unsigned short tamCode = 0, tamData = 0, tamExtra = 0;
    unsigned short tamStack = 0, tamConst = 0, entryOffset = 0;
    unsigned char high, low;

    if (version == 1) {
        // V1 solo tiene CodeSize en bytes 6-7
        fread(&high, sizeof(high), 1, archVmx);
        fread(&low,  sizeof(low),  1, archVmx);
        tamCode = (high << 8) | low;
        *tamCod = tamCode;
        tamData  = (unsigned int)(MV->memSize - tamCode);

        // Validar que CodeSegment entre en la memoria
        if (tamCode > MV->memSize) {
            printf("\n>> Error: Codigo (%u bytes) excede memoria (%lu bytes)\n",tamCode, (unsigned long)MV->memSize);
            MV->ErrorFlag = 4;
            fclose(archVmx);
            return 2;
        }

        // Cargar la totalidad del CodeSegment
        fread(MV->memoria, 1, tamCode, archVmx);
    }
    else {
        // V2: Code, Data, Extra, Stack, Const, Entry
        fread(&high, sizeof(high), 1, archVmx);
        fread(&low,  sizeof(low),  1, archVmx);
        tamCode = (high << 8) | low;
        *tamCod = tamCode;

        fread(&high, sizeof(high), 1, archVmx);
        fread(&low,  sizeof(low),  1, archVmx);
        tamData = (high << 8) | low;

        fread(&high, sizeof(high), 1, archVmx);
        fread(&low,  sizeof(low),  1, archVmx);
        tamExtra = (high << 8) | low;

        fread(&high, sizeof(high), 1, archVmx);
        fread(&low,  sizeof(low),  1, archVmx);
        tamStack = (high << 8) | low;

        fread(&high, sizeof(high), 1, archVmx);
        fread(&low,  sizeof(low),  1, archVmx);
        tamConst = (high << 8) | low;

        fread(&high, sizeof(high), 1, archVmx);
        fread(&low,  sizeof(low),  1, archVmx);
        entryOffset = (high << 8) | low;

        size_t baseParam = 0;
        //Para simplificar
        TArgs args = MV->args;
        if (args.cantidadParametros > 0) {
            for (int i = 0; i < args.cantidadParametros; i++) {
                // longitud de cada string + '\0'
                baseParam += strlen(args.parametros[i]) + 1;
            }
            // punteros argv (4 bytes c/u)
            baseParam += args.cantidadParametros * 4;
        }
        size_t tamParam = baseParam;
        size_t baseConst = baseParam;              // justo tras Param
        size_t baseCode  = baseConst + tamConst;   // justo tras Const

        // Validar que los segmentos entren en la memoria total
        size_t totalSegs = tamParam + tamConst + tamCode + tamData + tamExtra + tamStack;
        if (totalSegs > MV->memSize) {
            printf("\n>> Error: Segmentos totales (%lu bytes) exceden memoria (%lu bytes)\n", (unsigned long)totalSegs,(unsigned long)MV->memSize);
            MV->ErrorFlag = 4;
            fclose(archVmx);
            return 2;
        }

        // Leo Code Segment en su posición real
        fread(MV->memoria + baseCode, 1, tamCode, archVmx);

        // Leo Const Segment (esta inmediatamente despues del code en el .vmx)
        if (tamConst > 0) {
            fread(MV->memoria + baseConst, 1, tamConst, archVmx);
        }
    }

    fclose(archVmx);

    // Configurar todos los segmentos y TDS
    configurarSegmentos(MV, tamConst, tamCode, tamData, tamExtra, tamStack, entryOffset);

    if ( MV->version == 2){
        initSubRutinaPrincipal(MV);
    }

    return 0;
}

void configurarSegmentos(TMV *mv, unsigned short tamConst, unsigned short tamCode, unsigned short tamData, unsigned short tamExtra, unsigned short tamStack, unsigned short entryOffset){
    size_t base = 0;     // dirección fisica donde empieza el proximo segmento
    int    idx  = 0;     // indice en mv->TDS para la siguiente entrada

    int idxParam = -1, idxConst = -1, idxCode = -1;
    int idxData  = -1, idxExtra = -1, idxStack = -1;

    // Inicializo registros de segmento y pila a -1 (no presentes)
    mv->registros[CS] = 0xFFFFFFFF;
    mv->registros[DS] = 0xFFFFFFFF;
    mv->registros[ES] = 0xFFFFFFFF;
    mv->registros[SS] = 0xFFFFFFFF;
    mv->registros[KS] = 0xFFFFFFFF;
    mv->registros[SP] = 0xFFFFFFFF;

    //Para simplificar
    TArgs args = mv->args;

    // Param Segment (solo si hay parametros en args)
    if (args.cantidadParametros > 0 && mv->version == 2) {
        // 1) Copiar cada string y almacenar sus offsets
        unsigned short offsets[args.cantidadParametros];
        size_t pos = 0;      // desplazamiento relativo dentro de Param Segment

        for (int i = 0; i < args.cantidadParametros; i++) {
            size_t L = strlen(args.parametros[i]) + 1;  // longitud con '\0'
            // copia el string al bloque de memoria en [base+pos ...]
            memcpy(mv->memoria + base + pos,args.parametros[i],L);
            offsets[i] = (unsigned short)pos;
            pos += L;
        }

        // 2) Construir el arreglo argv de punteros (4 bytes c/u)
        size_t startPtrs = pos;
        for (int i = 0; i < args.cantidadParametros; i++) {
            // selector=0 (Param Segment), offset = offsets[i]
            uint32_t ptrVal = (0 << 16) | offsets[i];
            size_t  off    = base + startPtrs + i * 4;
            // escribir en big-endian
            mv->memoria[off + 0] = (ptrVal >> 24) & 0xFF;
            mv->memoria[off + 1] = (ptrVal >> 16) & 0xFF;
            mv->memoria[off + 2] = (ptrVal >>  8) & 0xFF;
            mv->memoria[off + 3] = (ptrVal >>  0) & 0xFF;
        }

        // 3) Calcular tamaño total del Param Segment
        unsigned short tamParam = (unsigned short)(pos + args.cantidadParametros * 4);

        // 4) Registrar en la TDS
        idxParam = idx;
        mv->TDS[idx++] = ((unsigned int)base << 16) | tamParam;
        base += tamParam;
    }

    // Const Segment
    if (tamConst > 0) {
        idxConst = idx;
        mv->TDS[idx++] = ((unsigned int)base << 16) | tamConst;
        base += tamConst;
    }

    // Code Segment
    idxCode = idx;
    mv->TDS[idx++] = ((unsigned int)base << 16) | tamCode;
    base += tamCode;

    // Data Segment
    if (tamData > 0) {
        idxData = idx;
        mv->TDS[idx++] = ((unsigned int)base << 16) | tamData;
        base += tamData;
    }

    // Extra Segment
    if (tamExtra > 0) {
        idxExtra = idx;
        mv->TDS[idx++] = ((unsigned int)base << 16) | tamExtra;
        base += tamExtra;
    }

    // Stack Segment
    if (tamStack > 0) {
        idxStack = idx;
        mv->TDS[idx++] = ((unsigned int)base << 16) | tamStack;
        base += tamStack;
    }

    // Ajustamos los registros de segmentos
    // Cada registro de segmento (CS, DS, ES, SS, KS) apunta al selector
    // igual al indice en la TDS, offset = 0.
    if (idxCode  >= 0) mv->registros[CS] = ((unsigned int)idxCode  << 16);
    if (idxData  >= 0) mv->registros[DS] = ((unsigned int)idxData  << 16);
    if (idxExtra >= 0) mv->registros[ES] = ((unsigned int)idxExtra << 16);
    if (idxStack >= 0) mv->registros[SS] = ((unsigned int)idxStack << 16);
    if (idxConst >= 0) mv->registros[KS] = ((unsigned int)idxConst << 16);

    // SP = selector SS, offset = tamaño del Stack Segment
    if (idxStack >= 0) mv->registros[SP] = ((unsigned int)idxStack << 16) | tamStack;

    // Inicializamos IP: selector = idxCode, offset = entryOffset
    mv->registros[IP] = ((unsigned int)idxCode << 16) | entryOffset;
}

void initSubRutinaPrincipal(TMV *mv) {
    // Leo el SP inicial (apunta al final del Stack Segment)
    uint32_t spValue   = mv->registros[SP];
    uint16_t selector  = spValue >> 16;       // selector del SS
    int32_t  offset    = spValue & 0xFFFF;    // offset dentro del SS

    // Base fisica del Stack Segment
    uint32_t baseStack = mv->TDS[selector] >> 16;

    // Preparamos los tres valores a apilar
    int32_t valores[3];

    //Para simplificar
    TArgs args = mv->args;

    // a) RET (-1) : 0xFFFFFFFF
    valores[0] = -1;

    // b) argc
    valores[1] = args.cantidadParametros;

    // c) argv puntero
    if (args.cantidadParametros > 0) {
        // El Param Segment es la entrada 0 de la TDS si hay parametros
        uint16_t tamParam       = mv->TDS[0] & 0xFFFF;
        uint16_t n              = args.cantidadParametros;
        uint16_t offsetArgv     = tamParam - n * 4;
        valores[2] = (0 << 16) | offsetArgv;
    } else {
        // sin parametros: puntero = -1
        valores[2] = -1;
    }

    // Apilamos cada valor (big-endian y SP decrementa en 4)
    for (int i = 0; i < 3; i++) {
        offset -= 4;
        if (offset < 0) {
            printf("ERROR: Stack overflow al inicializar main en [%.4X]\n", mv->registros[IP]);
            mv->ErrorFlag = 5;
            return;
        }

        // actualizamos el SP con selector y nuevo offset
        mv->registros[SP] = ((uint32_t)selector << 16) | ((uint16_t)offset & 0xFFFF);

        // dirección fisica donde escribir
        uint32_t addr = baseStack + offset;

        // escribir en big-endian
        int32_t v = valores[i];
        mv->memoria[addr    ] = (v >> 24) & 0xFF;
        mv->memoria[addr + 1] = (v >> 16) & 0xFF;
        mv->memoria[addr + 2] = (v >>  8) & 0xFF;
        mv->memoria[addr + 3] = (v >>  0) & 0xFF;
    }
}

int cargarImagenVMI(const char *nombreImagen, TMV *MV, int *tamCod){
    FILE *arch = fopen(nombreImagen, "rb");
    if (!arch) {
        printf("\n>> Error: arch Vmi no se pudo abrir imagen '%s'\n", nombreImagen);
        return 1;
    }

    // Leo identificador "VMI25" (5 bytes) y version (1 byte)
    char identificador[5];
    unsigned char version;
    fread(identificador, sizeof(identificador), 1, arch);
    fread(&version, sizeof(version), 1, arch);
    MV->version = version;

    // Leo el tamaño de memoria (KiB)
    unsigned char high, low;
    fread(&high, 1, 1, arch);
    fread(&low,  1, 1, arch);
    unsigned short memKiB = (high << 8) | low;
    size_t memBytes = (size_t)memKiB * 1024;

    // Asignamos la memoria dinamica
    MV->memoria = malloc(memBytes);
    MV->memSize = memBytes;
    if (!MV->memoria) {
        printf("\n>> Error: no se pudo reservar %lu bytes para la memoria\n",(unsigned long)memBytes);
        fclose(arch);
        return 2;
    }

    // Leemos los registros (16 x 4 bytes, big-endian)
    for (int i = 0; i < 16; i++) {
        unsigned char b0, b1, b2, b3;
        fread(&b0, 1, 1, arch);
        fread(&b1, 1, 1, arch);
        fread(&b2, 1, 1, arch);
        fread(&b3, 1, 1, arch);
        MV->registros[i] = ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 <<  8) | ((uint32_t)b3 <<  0);
    }

    // Leemos la TDS (8 x 4 bytes, big-endian)
    for (int j = 0; j < 8; j++) {
        unsigned char b0, b1, b2, b3;
        fread(&b0, 1, 1, arch);
        fread(&b1, 1, 1, arch);
        fread(&b2, 1, 1, arch);
        fread(&b3, 1, 1, arch);
        MV->TDS[j] = ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 <<  8) | ((uint32_t)b3 <<  0);
    }

    // Leemos la memoria principal
    fread(MV->memoria, 1, memBytes, arch);
    fclose(arch);

    // Extraemos el tamaño de Code Segment de la TDS: selector en CS, offset 0
    unsigned int selCS = MV->registros[CS] >> 16;
    unsigned short codeSize = (unsigned short)(MV->TDS[selCS] & 0xFFFF);
    *tamCod = codeSize;

    return 0;
}










