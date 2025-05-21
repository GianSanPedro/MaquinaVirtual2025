
void Disassembler(const TMV *mv, int tamCod);
void MostrarConstantes(TMV *mv);
void MostrarInstruccion(TInstruccion inst, char *memoria);
void MostrarOperando(TOperando op);
void procesarInstruccion(TMV *mv, TInstruccion inst);

TInstruccion LeerInstruccionCompleta(TMV *MV, int ip);
int esIPValida(TMV *mv);
