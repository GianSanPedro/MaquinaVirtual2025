
typedef char instruccion;
typedef struct{
    instruccion memoria[16384];     // Memoria principal (RAM) de 16 KiB
    int TDS[8];                     // Tabla de Descriptores de Segmentos
    unsigned int registros[16];     // 16 registros de 4 bytes (se utilizan 11 en esta primera parte)
}TMV;

typedef struct {
    char tipo;                      // 0: ninguno, 1: registro, 2: inmediato, 3: memoria
    char registro;                  // código de registro (0–15) si aplica (registro o memoria)
    char segmentoReg;               // para tipo registro: 00=EAX, 01=AL, 10=AH, 11=AX
    unsigned short desplazamiento;  // para memoria (OffSet)
    int valor;                      // para inmediato (o para lectura posterior)
} TOperando;

typedef struct {
    char codOperacion;    // Código de operación (5 bits reales)
    TOperando op1;        // Operando A
    TOperando op2;        // Operando B
    int tamanio;          // Tamaño total de la instrucción en bytes
    int ipInicial;        // Dirección en memoria donde comienza la instrucción
} TInstruccion;
