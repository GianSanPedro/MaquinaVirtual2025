void actualizarNZ(TMV *mv, int resultado);

int leerValor(TMV *mv, TOperando op);

void escribirValor(TMV *mv, TOperando op, int valor);

void leerDesdeTeclado(TMV *mv);

void escribirEnPantalla(TMV *mv);

int esDireccionValida(TMV *mv, int selector, int direccion, int tam);

void MOV(TMV *mv, TOperando op1, TOperando op2);

void ADD(TMV *mv, TOperando op1, TOperando op2);

void SUB(TMV *mv, TOperando op1, TOperando op2);

void SWAP(TMV *mv, TOperando op1, TOperando op2);

void MUL(TMV *mv, TOperando op1, TOperando op2);

void DIV(TMV *mv, TOperando op1, TOperando op2);

void CMP(TMV *mv, TOperando op1, TOperando op2);

void SHL(TMV *mv, TOperando op1, TOperando op2);

void SHR(TMV *mv, TOperando op1, TOperando op2);

void AND(TMV *mv, TOperando op1, TOperando op2);

void OR(TMV *mv, TOperando op1, TOperando op2);

void XOR(TMV *mv, TOperando op1, TOperando op2);

void LDL(TMV *mv, TOperando op1, TOperando op2);

void LDH(TMV *mv, TOperando op1, TOperando op2);

void RND(TMV *mv, TOperando op1, TOperando op2);

void SYS(TMV *mv, TOperando op1);

void JMP(TMV *mv, TOperando op1);

void JZ(TMV *mv, TOperando op1);

void JP(TMV *mv, TOperando op1);

void JN(TMV *mv, TOperando op1);

void JNZ(TMV *mv, TOperando op1);

void JNP(TMV *mv, TOperando op1);

void JNN(TMV *mv, TOperando op1);

void NOT(TMV *mv, TOperando op1);

void imprimirRegistrosGenerales(TMV *mv);
