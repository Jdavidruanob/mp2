#include "Temporizador.h"

void Temporizador::iniciar() {
    inicioReal = std::chrono::steady_clock::now();
    inicioCPU = std::clock();
}

double Temporizador::segundosReales() const {
    std::chrono::duration<double> transcurrido = std::chrono::steady_clock::now() - inicioReal;
    return transcurrido.count();
}

double Temporizador::segundosCPU() const {
    return static_cast<double>(std::clock() - inicioCPU) / CLOCKS_PER_SEC;
}
