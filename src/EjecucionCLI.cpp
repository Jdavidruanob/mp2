#include "EjecucionCLI.h"

#include "Imagen.h"
#include "ImagenIO.h"
#include "FiltroFactory.h"
#include "Temporizador.h"

#include <cstdio>
#include <cstring>

namespace {

// Inserta "_<sufijo>" antes de la extensión de 'base' (o al final si no
// tiene extensión). P. ej. ("out.ppm", "blur") -> "out_blur.ppm".
void construirNombreSalida(const char *base, const char *sufijo, char *destino, size_t tam) {
    const char *punto = std::strrchr(base, '.');
    if (punto == nullptr) {
        std::snprintf(destino, tam, "%s_%s", base, sufijo);
    } else {
        int prefijoLen = static_cast<int>(punto - base);
        std::snprintf(destino, tam, "%.*s_%s%s", prefijoLen, base, sufijo, punto);
    }
}

bool procesarUnFiltro(const Imagen &entrada, const EjecutorFiltro &ejecutor,
                       const char *nombreFiltro, const char *rutaSalida,
                       const char *rutaEntradaOriginal,
                       double lecturaReal, double lecturaCPU) {
    Filtro *filtro = FiltroFactory::crear(nombreFiltro);
    if (filtro == nullptr) {
        std::fprintf(stderr, "Error: filtro desconocido '%s' (use blur, laplace, sharpen o sobel).\n", nombreFiltro);
        return false;
    }

    Imagen *salida = entrada.crearVacia(entrada.getAncho(), entrada.getAlto(), entrada.getValorMax());

    Temporizador tFiltrado;
    tFiltrado.iniciar();
    ejecutor.ejecutar(*filtro, entrada, *salida);
    double filtradoReal = tFiltrado.segundosReales();
    double filtradoCPU = tFiltrado.segundosCPU();

    Temporizador tEscritura;
    tEscritura.iniciar();
    bool ok = ImagenIO::escribir(*salida, rutaSalida);
    double escrituraReal = tEscritura.segundosReales();
    double escrituraCPU = tEscritura.segundosCPU();

    double totalReal = lecturaReal + filtradoReal + escrituraReal;
    double totalCPU = lecturaCPU + filtradoCPU + escrituraCPU;

    std::printf("%s,%s,%s,%s,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                ejecutor.nombre(), rutaEntradaOriginal, rutaSalida, nombreFiltro,
                entrada.getAncho(), entrada.getAlto(),
                lecturaReal, lecturaCPU, filtradoReal, filtradoCPU,
                escrituraReal, escrituraCPU, totalReal, totalCPU);

    if (!ok) {
        std::fprintf(stderr, "Error: no se pudo escribir '%s'.\n", rutaSalida);
    }

    delete salida;
    delete filtro;
    return ok;
}

} // namespace

int ejecutarCLI(int argc, char *argv[], const EjecutorFiltro &ejecutor) {
    if (argc < 3) {
        std::fprintf(stderr, "Uso: %s <entrada> <salida> [--f <blur|laplace|sharpen|sobel>]\n", argv[0]);
        std::fprintf(stderr, "Sin --f se aplican los tres filtros obligatorios: blur, laplace, sharpen.\n");
        return 1;
    }

    const char *rutaEntrada = argv[1];
    const char *rutaSalida = argv[2];
    const char *filtroPedido = nullptr;

    for (int i = 3; i < argc; i++) {
        if (std::strcmp(argv[i], "--f") == 0 && i + 1 < argc) {
            filtroPedido = argv[i + 1];
            i++;
        }
    }

    Temporizador tLectura;
    tLectura.iniciar();
    Imagen *entrada = ImagenIO::leer(rutaEntrada);
    if (entrada == nullptr) {
        return 1;
    }
    double lecturaReal = tLectura.segundosReales();
    double lecturaCPU = tLectura.segundosCPU();

    std::printf("ejecutor,entrada,salida,filtro,ancho,alto,lectura_real_s,lectura_cpu_s,filtrado_real_s,filtrado_cpu_s,escritura_real_s,escritura_cpu_s,total_real_s,total_cpu_s\n");

    bool ok;
    if (filtroPedido != nullptr) {
        ok = procesarUnFiltro(*entrada, ejecutor, filtroPedido, rutaSalida, rutaEntrada, lecturaReal, lecturaCPU);
    } else {
        static const char *obligatorios[3] = {"blur", "laplace", "sharpen"};
        ok = true;
        for (int i = 0; i < 3; i++) {
            char rutaGenerada[1024];
            construirNombreSalida(rutaSalida, obligatorios[i], rutaGenerada, sizeof(rutaGenerada));
            ok = procesarUnFiltro(*entrada, ejecutor, obligatorios[i], rutaGenerada, rutaEntrada, lecturaReal, lecturaCPU) && ok;
        }
    }

    delete entrada;
    return ok ? 0 : 1;
}
