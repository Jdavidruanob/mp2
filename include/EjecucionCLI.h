#ifndef EJECUCION_CLI_H
#define EJECUCION_CLI_H

#include "EjecutorFiltro.h"

// Lógica común a filterer/th_filterer/omp_filterer: parsea
// "entrada salida [--f <filtro>]", lee la imagen, aplica cada filtro
// pedido (uno con --f, o los tres obligatorios sin --f) usando la
// 'ejecutor' dada, escribe la(s) salida(s) y mide/reporta los tiempos en
// una línea CSV por filtro. Los argumentos específicos de cada ejecutable
// (p. ej. --hilos de omp_filterer) deben parsearse ANTES de llamar a esta
// función; se ignoran aquí porque solo se reconoce "--f".
int ejecutarCLI(int argc, char *argv[], const EjecutorFiltro &ejecutor);

#endif
