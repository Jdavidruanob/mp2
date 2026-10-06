#ifndef IMAGEN_IO_H
#define IMAGEN_IO_H

#include "Imagen.h"

// Fábrica de lectura/escritura: detecta automáticamente P2 (PGM) o P3 (PPM)
// a partir del número mágico del encabezado e ignora comentarios ('#' hasta
// fin de línea) en cualquier parte del archivo.
class ImagenIO {
public:
    // Lee una imagen desde 'ruta'. Si ruta es "-" lee desde stdin.
    // Devuelve nullptr (e imprime un mensaje por stderr) si el archivo no
    // se pudo abrir, el número mágico no es P2/P3, o faltan datos de píxel.
    // El llamador es responsable de liberar la imagen devuelta (delete).
    static Imagen *leer(const char *ruta);

    // Escribe 'imagen' en 'ruta' preservando su formato original
    // (PGM -> P2, PPM -> P3). Devuelve false si no se pudo abrir el
    // archivo de salida.
    static bool escribir(const Imagen &imagen, const char *ruta);
};

#endif
