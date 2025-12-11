inicio:     MOV     EAX, 1         ; acumulador = 1
            MOV     EDX, DS
            MOV     ECX, 0x0401    ; 1 celda de 4 bytes
            MOV     AL, 1
            SYS     1             ; leer número en [EDX]

factorial:  CMP     [EDX], 1
            JNP     fin           ; si n <= 1, fin
            MUL     EAX, [EDX]    ; EAX = EAX * n
            SUB     [EDX], 1      ; n = n - 1
            JMP     factorial

fin:        ADD     EDX, 4
            MOV     [EDX], EAX
            MOV     ECX, 0x0401
            MOV     AL, 1
            SYS     2
            STOP