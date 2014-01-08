/*
 *  Util.cpp
 *  
 *
 *  Created by Rintarou on 2010/5/15.
 *  Copyright 2010 http://rintarou.dyndns.org. All rights reserved.
 *
 */

#include "tiny_Util.h"
#include "tiny_common.h"

#include "lodepng.h"

#include <vector>

void debugMsg( const char* fmt, ... )
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

char *Color2String(unsigned int rgba, int hasAlpha)
{
    static char strBuffer[30];
    if (hasAlpha)
        snprintf(strBuffer, 30, "0x%08x", rgba);
    else
        snprintf(strBuffer, 30, "0x%06x", rgba);
    return strBuffer;
}


#define CHUNK 4096

// It's caller's job to return the allocated memory.
//int Util::inflate2Memory (Stream *stream, unsigned char **output_ptr)
int inflate2Memory (Stream *stream, unsigned char **output_ptr)
{
    unsigned char *inflated_stream = 0;
    size_t inflated_stream_size = 0;
    int ret;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];
    std::vector<unsigned char> full_input;

    // First read the entire input stream into memory.
    for(;;) {
      int numBytes = stream->read(in, CHUNK);
      if (numBytes == 0) {
        printf("EOS\n");
        break;
      }
      full_input.insert(full_input.end(), &in[0], &in[CHUNK]);
    }

   printf("Finished reading %d bytes.\n", (int)full_input.size());
   LodePNGDecompressSettings settings;
   settings.ignore_adler32 = false;
   settings.custom_zlib = NULL;
   settings.custom_inflate = NULL;
   settings.custom_context = NULL;
   unsigned status = lodepng_zlib_decompress(
       output_ptr,
       &inflated_stream_size,
       &full_input[0],
       full_input.size(),
       &settings);
   printf("Finished with status %d.\n", status);
   printf("Inflated size: %d.\n", (int)inflated_stream_size);
   return (size_t)inflated_stream_size;
}

