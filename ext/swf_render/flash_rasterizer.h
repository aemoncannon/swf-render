#ifndef SRC_FLASH_RASTERIZER
#define SRC_FLASH_RASTERIZER

#include <stdlib.h>

int render_to_png_file(const char* input_swf, const char* output_png);
int render_to_png_buffer(const char* input_swf,
                         int width,
                         int height,
                         unsigned char** out,
                         size_t* outsize);

#endif
