#include "ParticionMPI.h"
#include <algorithm>

void calcularParticion(int alto, int size, int rank, int &y0, int &filas) {
    int base = alto / size;
    int resto = alto % size;
    filas = base + (rank < resto ? 1 : 0);
    y0 = rank * base + std::min(rank, resto);
}
