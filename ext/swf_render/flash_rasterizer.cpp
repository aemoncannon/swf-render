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

#include "flash_rasterizer.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <map>
#include <algorithm>

#include "agg_rendering_buffer.h"
#include "agg_trans_viewport.h"
#include "agg_path_storage.h"
#include "agg_conv_transform.h"
#include "agg_conv_curve.h"
#include "agg_conv_stroke.h"
#include "agg_gsv_text.h"
#include "agg_scanline_u.h"
#include "agg_scanline_bin.h"
#include "agg_renderer_scanline.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_rasterizer_compound_aa.h"
#include "agg_span_allocator.h"
#include "agg_gamma_lut.h"
#include "agg_pixfmt_rgba.h"
#include "agg_bounding_rect.h"
#include "agg_color_gray.h"
#include "lodepng.h"

#include "display_tree.h"
#include "tiny_common.h"
#include "tiny_swfparser.h"
#include "utils.h"

DisplayTree* create_display_tree(const RunConfig& c) {
  TinySWFParser parser;
  ParsedSWF* swf = parser.parse(c.input_swf.c_str());
  assert(swf);
  if (const Sprite* sprite = swf->SpriteByClassName(c.class_name.c_str())) {
    DisplayTree* tree = DisplayTree::Build(*swf, *sprite);
    if (c.spec.size()) {
      tree->ApplySpec(c.spec.c_str());
    }
    return tree;
  }
  return NULL;
}

Matrix create_view_matrix(
    const DisplayTree& tree,
    int width,
    int height,
    int pad) {

  double x1 = 0;
  double x2 = 0;
  double y1 = 0;
  double y2 = 0;
  Matrix identity;
  tree.GetBounds(identity, &x1, &x2, &y1, &y2);

  agg::trans_viewport vp;
  vp.preserve_aspect_ratio(0.5, 0.5, agg::aspect_ratio_meet);
  vp.world_viewport(x1, y1, x2, y2);
  vp.device_viewport(0, 0, width, height);
  const double p = pad / vp.scale_x();  // Find world padding that will give the desired pixel padding.
  vp.world_viewport(x1-p, y1-p, x2+p, y2+p);
  vp.device_viewport(0, 0, width, height);
  return vp.to_affine();
}

int render_to_buffer(
    const DisplayTree& tree,
    const Matrix& view_transform,
    int width,
    int height,
    unsigned char* buf) {

  agg::rendering_buffer rbuf;
  rbuf.attach(buf, width, height, width * 4);
  pixfmt pixf(rbuf);
  renderer_base ren_base(pixf);
  ren_base.clear(Color(0, 0, 0, 0));
  renderer_scanline ren(ren_base);
  tree.Render(view_transform, NULL, width, height, ren_base, ren);
  return 0;
}

void get_output_dimensions(const DisplayTree& tree, int* width_out, int* height_out) {
  if (*width_out == 0 || *height_out == 0) {
    int width = 0;
    int height = 0;
    tree.GetNaturalSizeInPixels(&width, &height);
    if (*width_out > 0) {
      const double r = (double)height / (double)width;
      *height_out = (int)((double)*width_out * r);
    } else {
      *width_out = width;
      *height_out = height;
    }
  }
}

int render_to_png_file(const RunConfig& c) {
  DisplayTree* tree = create_display_tree(c);
  int width = c.width;
  int height = c.height;
  int pad = c.padding;
  get_output_dimensions(*tree, &width, &height);
  unsigned char* buf = new unsigned char[width * height * 4];
  Matrix view_transform = create_view_matrix(*tree, width, height, pad);
  render_to_buffer(*tree, view_transform, width, height, buf);
  unsigned error = lodepng_encode32_file(c.output_png.c_str(), buf, width, height);
  if(error) {
    printf("Error %u: %s\n", error, lodepng_error_text(error));
    return 1;
  } else {
    return 0;
  }
}

int render_to_png_buffer(const RunConfig& c, Result* result) {
  DisplayTree* tree = create_display_tree(c);
  int width = c.width;
  int height = c.height;
  int pad = c.padding;
  get_output_dimensions(*tree, &width, &height);
  unsigned char* buf = new unsigned char[width * height * 4];
  Matrix view_transform = create_view_matrix(*tree, width, height, pad);
  view_transform.transform(&result->origin_x, &result->origin_y);
  render_to_buffer(*tree, view_transform, width, height, buf);
  unsigned error = lodepng_encode32(&result->data, &result->size, buf, width, height);
  if(error) {
    printf("Error %u: %s\n", error, lodepng_error_text(error));
    return 1;
  } else {
    return 0;
  }
}

int get_metadata(const RunConfig& c, Result* result) {
  DisplayTree* tree = create_display_tree(c);

  int width = c.width;
  int height = c.height;
  int pad = c.padding;
  get_output_dimensions(*tree, &width, &height);

  Matrix view_transform = create_view_matrix(*tree, width, height, pad);
  view_transform.transform(&result->origin_x, &result->origin_y);

  return 0;
}

int main(int argc, char* argv[]) {
  RunConfig config;
  int c;
  int opterr = 0;
  while ((c = getopt (argc, argv, "w:h:o:c:p:")) != -1) {
    switch (c) {
      case 'w':
        config.width = strtol(optarg, 0, 10);
        break;
      case 'h':
        config.height = strtol(optarg, 0, 10);
        break;
      case 'p':
        config.padding = strtol(optarg, 0, 10);
        break;
      case 'c':
        config.class_name = optarg;
        break;
      case 'o':
        config.output_png = optarg;
        break;
      case '?':
          fprintf (stderr,
                   "Unrecognized option `\\x%x'.\n", optopt);
        return 1;
      default:
        abort();
    }
  }

  if (optind > (argc - 1)) {
    fprintf(stderr, "Missing required input swf filename\n");
    return 1;
  }
  config.input_swf = argv[optind];

  return render_to_png_file(config);
}

