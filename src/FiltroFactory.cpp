#include "FiltroFactory.h"
#include "FiltroBlur.h"
#include "FiltroLaplace.h"
#include "FiltroSharpen.h"
#include "FiltroSobel.h"

#include <cstring>

Filtro *FiltroFactory::crear(const char *nombre) {
    if (std::strcmp(nombre, "blur") == 0) {
        return new FiltroBlur();
    }
    if (std::strcmp(nombre, "laplace") == 0) {
        return new FiltroLaplace();
    }
    if (std::strcmp(nombre, "sharpen") == 0) {
        return new FiltroSharpen();
    }
    if (std::strcmp(nombre, "sobel") == 0) {
        return new FiltroSobel();
    }
    return nullptr;
}
