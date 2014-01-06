// This program is derived from the flash demos included in the
// AGG 2.5 project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// In the header you should put also a copyright notice something like:
//
// Copyright (C) 2002-2006 Maxim Shemanarev
// Copyright Aemon Cannon 2013,2013


#ifndef SRC_FLASH_RASTERIZER
#define SRC_FLASH_RASTERIZER

#include <stdlib.h>

struct RunConfig;

int render_to_png_file(const RunConfig& c);
int render_to_png_buffer(const RunConfig& c,
                         unsigned char** out,
                         size_t* outsize);

#endif
