#ifndef IMAGEN_H
#define IMAGEN_H

// Clase base abstracta para una imagen tipo netpbm (PGM o PPM).
// Almacena los píxeles en un único arreglo dinámico de enteros,
// intercalado por canal dentro de cada fila:
//   indice = (y * ancho + x) * canales + canal
// Esto permite que cada canal se filtre por separado (requisito de
// los diseños 2/3/4) sin cambiar la representación de almacenamiento.
class Imagen {
protected:
    int ancho;
    int alto;
    int valorMax;
    int canales;    // 1 para PGM, 3 para PPM
    int *pixeles;   // arreglo dinámico de tamaño ancho*alto*canales

public:
    Imagen(int ancho, int alto, int valorMax, int canales);
    virtual ~Imagen();

    // Las imágenes no se copian implícitamente: el arreglo de píxeles
    // es de propiedad única de cada instancia.
    Imagen(const Imagen &) = delete;
    Imagen &operator=(const Imagen &) = delete;

    int getAncho() const;
    int getAlto() const;
    int getValorMax() const;
    int getCanales() const;
    int getCantidadValores() const; // ancho * alto * canales

    int *getPixeles();
    const int *getPixeles() const;

    // Acceso seguro por posición y canal (0 <= x < ancho, 0 <= y < alto,
    // 0 <= canal < canales).
    int obtenerValor(int x, int y, int canal) const;
    void asignarValor(int x, int y, int canal, int valor);

    // Identifican el formato concreto; las usa ImagenIO al escribir.
    virtual const char *numeroMagico() const = 0;
    virtual const char *nombreFormato() const = 0;

    // Patrón "prototipo": crea una imagen nueva del mismo tipo concreto
    // (PGM o PPM) con las dimensiones y valor máximo dados, píxeles sin
    // inicializar. Permite que código genérico (p. ej. los filtros) cree
    // la imagen de salida correcta sin preguntar getCanales() ni conocer
    // las subclases concretas.
    virtual Imagen *crearVacia(int ancho, int alto, int valorMax) const = 0;
};

#endif
