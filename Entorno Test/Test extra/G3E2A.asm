        MOV     [4], 0           ; acumulador en memoria
        XOR     EFX, EFX         ; contador de bits ingresados

LEER:   MOV     EDX, DS
        ADD     EDX, 100         ; base de datos de bits
        ADD     EDX, EFX         ; desplazamiento por bit
        MOV     CH, 4
        MOV     CL, 1
        MOV     AL, 0x01
        SYS     1                ; leer un numero
        CMP     [EDX], 0
        JZ      CONTINUAR
        CMP     [EDX], 1
        JZ      CONTINUAR
        SUB     EFX, 4           ; retroceder si no era 0 ni 1
        JMP     SIGUIENTE

CONTINUAR: ADD     EFX, 4
        JMP     LEER

SIGUIENTE: CMP     EFX, 0
        JN      MOSTRAR
        MOV     EDX, DS
        ADD     EDX, 100
        ADD     EDX, EFX
        SHL     [4], 1
        ADD     [4], [EDX]
        SUB     EFX, 4
        JMP     SIGUIENTE

MOSTRAR: MOV     EDX, DS
        ADD     EDX, 8
        MOV     [EDX], [4]
        MOV     CH, 4
        MOV     CL, 1
        MOV     AL, 0x01
        SYS     2
        STOP

