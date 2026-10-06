#include <iostream>

#define MAX_FILENAME 256
#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {

  // Kernel de blur (promedio)
  const float blur_kernel[3][3] = {
      {1.0/9, 1.0/9, 1.0/9},
      {1.0/9, 1.0/9, 1.0/9},
      {1.0/9, 1.0/9, 1.0/9}
  };

  char buffer[BUFFER_SIZE];
  char magic[3];
  int width;
  int height;
  int max_color;
  int *pixels;
  int* blurred_pixels;

  if(argc<2){
    std::cout << "missing input and output paths\n";
    std::cout << "usage:" << argv[0] << " input_image.pgm output_image.pgm" << std::endl;
    std::cout << "or "<< argv[0] << "input_image.ppm output_image.ppm" << std::endl;
    return 1;
  }
  // abrir archivo
  FILE *file = fopen(argv[1], "r");
  if (file == NULL) {
    std::cout << "Error, incorrect path or incorrect file."<< std::endl;
    return 1;
  }
  // Leer y mostrar línea por línea
  fscanf(file, "%2s", magic);  // magic number
  fscanf(file, "%d %d", &width, &height);
  fscanf(file, "%d", &max_color);

  int pixel_count = width * height;
  if (strcmp(magic, "P3") != 0){
    int pixel_count = width * height * 3;
  }

  pixels = (int *) malloc(pixel_count);
  int value;

  for (int i = 0; i < pixel_count; i++) {
      if (fscanf(file, "%d", &value) != 1) {
        std::cout << "Error reading pixels."<< std::endl;
        free(pixels);
        fclose(file);
        return 0;
      }
      pixels[i] = value;
  }
  fclose(file);

  blurred_pixels = (int *) malloc(pixel_count);
  // Aplicar filtro blur
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      float sum = 0.0;
      float weight_sum = 0.0;

      // Aplicar kernel 3x3
      for (int ky = -1; ky <= 1; ky++) {
        for (int kx = -1; kx <= 1; kx++) {
          int nx = x + kx;
          int ny = y + ky;
          
          if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
            int idx = (ny * width + nx);
            sum += pixels[idx] * blur_kernel[ky+1][kx+1];
            weight_sum += blur_kernel[ky+1][kx+1];
          }
        }
      }

      // Normalizar y asegurar valores dentro del rango
      int result = static_cast<int>(sum / weight_sum);
      result = std::max(0, std::min(max_color, result));

      int idx = (y * width + x);
      blurred_pixels[idx] = result;
    }
  }

  FILE * output = fopen(argv[2], "w");
  (void) fprintf(output, "%s\n%d %d\n%d\n", magic, width, height, max_color);
  for (int i = 0; i < pixel_count; i++) {
    fprintf(output, "%d\n", blurred_pixels[i]);
  }
  (void) fclose(output);
  return 0;


  
  

}