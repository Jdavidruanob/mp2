#ifndef PARTICION_MPI_H
#define PARTICION_MPI_H

// Calcula, de forma determinista e idéntica en todos los ranks (sin
// necesidad de comunicación), qué franja de filas [y0, y0+filas) del total
// 'alto' le corresponde al rank 'rank' de 'size' ranks. El resto de la
// división se reparte entre los primeros ranks, así que ningún rank queda
// con más de 1 fila de diferencia respecto a los demás, incluso si 'alto'
// no es múltiplo de 'size' (dimensiones "impares" respecto al número de
// procesos).
void calcularParticion(int alto, int size, int rank, int &y0, int &filas);

#endif
