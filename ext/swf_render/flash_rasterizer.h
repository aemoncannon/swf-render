#ifndef SRC_FLASH_RASTERIZER
#define SRC_FLASH_RASTERIZER

#include <stdlib.h>

struct RunConfig;

int render_to_png_file(const RunConfig& c);
int render_to_png_buffer(const RunConfig& c,
                         unsigned char** out,
                         size_t* outsize);

#endif
