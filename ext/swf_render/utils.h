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

#include <string>

#ifndef _UTILS_H
#define _UTILS_H

struct RunConfig {
  RunConfig() : output_png("out.png"),
  width(200),
  height(200),
  padding(0) {}
  std::string input_swf;
  std::string output_png;
  std::string class_name;
  std::string spec;
  int width;
  int height;
  int padding;
};

struct Result {
  Result() { Init(); }
  void Init() {
    data = NULL;
    size = 0;
    origin_x = 0;
    origin_y = 0;
    natural_width = 0;
    natural_height = 0;
  }
  unsigned char* data;
  size_t size;
  double origin_x;
  double origin_y;
  int natural_width;
  int natural_height;
};

#endif
