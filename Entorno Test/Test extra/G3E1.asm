	MOV EFX, 0 	; cantidad
	XOR [4], [4]	; acumulador
	
OTRO:	MOV EDX, DS	; DS almacena un puntero al 1er registro del data segment
	MOV CH, 4	
	MOV CL, 1	
	MOV AL, 0x01	; es decimal
	SYS 1;

	CMP [EDX], 0
	JN SIGUE
	ADD [4], [EDX]
	ADD EFX, 1
	JMP OTRO
	
SIGUE:	CMP EFX, 0
	JZ FIN
	DIV [4], EFX
	
FIN:	MOV EDX, DS
	ADD EDX, 4
	MOV CH, 4
	MOV CL, 1
	MOV AL, 0x01
	SYS 2
	STOP