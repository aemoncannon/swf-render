#ifndef SRC_FLASH_RASTERIZER
#define SRC_FLASH_RASTERIZER

int render(char* input_swf, char* output_png);
int render_to_buffer(char* input_swf,
                     unsigned char** out,
                     unsigned int* outsize);

#endif
