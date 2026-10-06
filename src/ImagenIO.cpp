#include "ImagenIO.h"
#include "PGMImagen.h"
#include "PPMImagen.h"

#include <cstdio>
#include <cstring>
#include <cctype>

namespace {

// Avanza el cursor del archivo saltando espacios en blanco y comentarios
// ('#' hasta el fin de línea). Se usa antes de leer cada token, así que los
// comentarios quedan ignorados sin importar si aparecen en el encabezado o
// entre los valores de píxel.
void saltarEspaciosYComentarios(FILE *f) {
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '#') {
            while ((c = fgetc(f)) != EOF && c != '\n') {
                // descartar hasta fin de línea
            }
        } else if (!std::isspace(c)) {
            std::ungetc(c, f);
            break;
        }
    }
}

// Lee un entero saltando primero cualquier espacio/comentario previo.
// Devuelve false si no se pudo leer (fin de archivo o token no numérico).
bool leerEntero(FILE *f, int &valor) {
    saltarEspaciosYComentarios(f);
    return std::fscanf(f, "%d", &valor) == 1;
}

// Lee el número mágico (2 caracteres, p.ej. "P2"/"P3") en 'destino',
// que debe tener capacidad para al menos 3 bytes (2 + '\0').
bool leerMagico(FILE *f, char *destino) {
    saltarEspaciosYComentarios(f);
    return std::fscanf(f, "%2s", destino) == 1;
}

} // namespace

Imagen *ImagenIO::leer(const char *ruta) {
    bool esStdin = std::strcmp(ruta, "-") == 0;
    FILE *archivo = esStdin ? stdin : std::fopen(ruta, "r");

    if (archivo == nullptr) {
        std::fprintf(stderr, "Error: no se pudo abrir '%s' para lectura.\n", ruta);
        return nullptr;
    }

    char magico[3] = {0, 0, 0};
    int ancho = 0;
    int alto = 0;
    int valorMax = 0;

    if (!leerMagico(archivo, magico) ||
        !leerEntero(archivo, ancho) ||
        !leerEntero(archivo, alto) ||
        !leerEntero(archivo, valorMax)) {
        std::fprintf(stderr, "Error: encabezado inválido o incompleto en '%s'.\n", ruta);
        if (!esStdin) std::fclose(archivo);
        return nullptr;
    }

    if (ancho <= 0 || alto <= 0) {
        std::fprintf(stderr, "Error: dimensiones inválidas (%d x %d) en '%s'.\n", ancho, alto, ruta);
        if (!esStdin) std::fclose(archivo);
        return nullptr;
    }

    Imagen *imagen = nullptr;
    // Nota: la condición correcta compara el número mágico leído; el
    // código original del profesor la tenía invertida (strcmp(...) != 0
    // dentro del if que asumía PPM) y además perdía el valor por redeclarar
    // "pixel_count" dentro del if (shadowing), así que en la práctica
    // siempre usaba el conteo de un solo canal.
    if (std::strcmp(magico, "P2") == 0) {
        imagen = new PGMImagen(ancho, alto, valorMax);
    } else if (std::strcmp(magico, "P3") == 0) {
        imagen = new PPMImagen(ancho, alto, valorMax);
    } else {
        std::fprintf(stderr, "Error: numero magico '%s' no soportado en '%s' (se esperaba P2 o P3).\n", magico, ruta);
        if (!esStdin) std::fclose(archivo);
        return nullptr;
    }

    int *pixeles = imagen->getPixeles();
    int cantidad = imagen->getCantidadValores();
    for (int i = 0; i < cantidad; i++) {
        if (!leerEntero(archivo, pixeles[i])) {
            std::fprintf(stderr, "Error: datos de pixel incompletos en '%s' (se esperaban %d valores, fallo en el valor %d).\n", ruta, cantidad, i);
            delete imagen;
            if (!esStdin) std::fclose(archivo);
            return nullptr;
        }
    }

    if (!esStdin) std::fclose(archivo);
    return imagen;
}

bool ImagenIO::escribir(const Imagen &imagen, const char *ruta) {
    FILE *archivo = std::fopen(ruta, "w");
    if (archivo == nullptr) {
        std::fprintf(stderr, "Error: no se pudo abrir '%s' para escritura.\n", ruta);
        return false;
    }

    std::fprintf(archivo, "%s\n%d %d\n%d\n",
                  imagen.numeroMagico(), imagen.getAncho(), imagen.getAlto(), imagen.getValorMax());

    const int *pixeles = imagen.getPixeles();
    int alto = imagen.getAlto();
    int valoresPorFila = imagen.getAncho() * imagen.getCanales();

    // Se arma el texto de cada fila en un arreglo dinámico y se escribe
    // con un único fwrite() por fila, en vez de un fprintf()+fputc() por
    // cada valor de píxel. No es solo un detalle de estilo: cada llamada
    // a una función de <cstdio> toma un lock interno del FILE* para ser
    // segura entre hilos, y en glibc, en cuanto el proceso crea al menos
    // un hilo con pthread_create() (th_filterer, omp_filterer) ese lock
    // queda activado para el resto del proceso aunque el hilo ya haya
    // terminado y se haya unido con pthread_join(). Eso hacía que esta
    // función, llamada DESPUÉS del filtrado paralelo, fuera ~25-30% más
    // lenta en th_filterer/omp_filterer que en filterer con el código
    // anterior (un fprintf/fputc por valor: cientos de miles a millones
    // de llamadas). Escribir fila por fila baja esas llamadas de
    // "cantidad de valores" a "alto", y el costo del lock se vuelve
    // despreciable para todas las versiones. Investigado y documentado
    // en docs/reportes/FASE_6.md (incluye un microbenchmark aislado que
    // confirma la causa).
    const int DIGITOS_MAX = 12; // un int cabe en 11 dígitos + signo; 12 por margen
    char *buffer = new char[static_cast<size_t>(valoresPorFila) * DIGITOS_MAX + 2];

    for (int fila = 0; fila < alto; fila++) {
        int pos = 0;
        const int *filaPixeles = pixeles + static_cast<size_t>(fila) * valoresPorFila;
        for (int col = 0; col < valoresPorFila; col++) {
            pos += std::snprintf(buffer + pos, DIGITOS_MAX, "%d", filaPixeles[col]);
            buffer[pos++] = (col + 1 == valoresPorFila) ? '\n' : ' ';
        }
        std::fwrite(buffer, 1, static_cast<size_t>(pos), archivo);
    }

    delete[] buffer;
    std::fclose(archivo);
    return true;
}
