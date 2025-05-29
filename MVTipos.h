#define CS  0
#define DS  1
#define ES  2
#define SS  3
#define KS  4
#define IP  5
#define SP  6
#define BP  7
#define CC  8
#define AC  9
#define EAX  10
#define EBX  11
#define ECX  12
#define EDX  13
#define EEX  14
#define EFX  15

typedef char instruccion;

typedef struct {
    char   *nombreArchVmx;          // NULL si no viene .vmx
    char   *nombreArchVmi;          // NULL si no viene .vmi
    int    tamMemoriaKiB;           // por defecto 16
    int    modoDisassembler;        // 0 = off, 1 = on
    char   **parametros;            // NULL si no hay -p
    int    cantidadParametros;      // 0 si no hay -p
} TArgs;

typedef struct{
    char *memoria;                  // Memoria principal (RAM), asignada dinamicamente
    int TDS[8];                     // Tabla de Descriptores de Segmentos
    unsigned int registros[16];     // 16 registros de 4 bytes (se utilizan 11 en esta primera parte)
    int ErrorFlag;                  // Bandera para detectar errores
    char version;
    size_t memSize;                 // Tamaño en bytes de la memoria
    TArgs args;                     // parametros pasados al VM, necesito el vmi para breakPoint
    int stepMode;                   // 1 = ejecuta 1 instrucción y vuelve al breakpoint; 0 = continuar
    int Aborted;                    // 1 = el usuario pidio quit en breakpoint
}TMV;

typedef struct {
    char tipo;                      // 0: ninguno, 1: registro, 2: inmediato, 3: memoria
    char registro;                  // código de registro (0–15) si aplica (registro o memoria)
    char segmentoReg;               // para tipo registro: 00=EAX, 01=AL, 10=AH, 11=AX
    short desplazamiento;           // para memoria (OffSet)
    int tamCelda;                   // para memoria 0: long (l), 2: word (w), 3: byte (b)
    int valor;                      // para inmediato (o para lectura posterior)
} TOperando;

typedef struct {
    char codOperacion;              // Código de operación (5 bits reales)
    TOperando op1;                  // Operando A
    TOperando op2;                  // Operando B
    int tamanio;                    // Tamaño total de la instrucción en bytes
    int ipInicial;                  // Dirección en memoria donde comienza la instrucción
} TInstruccion;


