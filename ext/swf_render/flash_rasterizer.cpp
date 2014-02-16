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

int render_to_buffer(
    const RunConfig& c,
    unsigned char* buf,
    double* sprite_origin_x, 
    double* sprite_origin_y) {
  TinySWFParser parser;
  ParsedSWF* swf = parser.parse(c.input_swf.c_str());
  //swf->Dump();
  assert(swf);

  assert(c.class_name.size() || buf);

  if (const Sprite* sprite = swf->SpriteByClassName(c.class_name.c_str())) {
    const double pad = c.padding;
    DisplayTree* tree = DisplayTree::Build(*swf, *sprite);
    if (c.spec.size()) {
      tree->ApplySpec(c.spec.c_str());
    }
    double x1 = 0;
    double x2 = 0;
    double y1 = 0;
    double y2 = 0;
    Matrix identity;
    tree->GetBounds(identity, &x1, &x2, &y1, &y2);
    agg::trans_viewport vp;
    vp.preserve_aspect_ratio(0.5, 0.5, agg::aspect_ratio_meet);
    vp.world_viewport(x1, y1, x2, y2);
    vp.device_viewport(0, 0, c.width, c.height);
    const double p = pad / vp.scale_x();  // Find world padding that will give the desired pixel padding.
    vp.world_viewport(x1-p, y1-p, x2+p, y2+p);
    vp.device_viewport(0, 0, c.width, c.height);
    const Matrix view_transform = vp.to_affine();

    // Find the sprite's origin in destination image coordinates.
    *sprite_origin_x = 0.0;
    *sprite_origin_y = 0.0;
    view_transform.transform(sprite_origin_x, sprite_origin_y);

    // Do the actual rendering of the sprite.
    if (buf != NULL) {
      agg::rendering_buffer rbuf;
      rbuf.attach(buf, c.width, c.height, c.width * 4);
      pixfmt pixf(rbuf);
      renderer_base ren_base(pixf);
      ren_base.clear(Color(0, 0, 0, 0));
      renderer_scanline ren(ren_base);
      tree->Render(view_transform, NULL, c.width, c.height, ren_base, ren);
    }

  } else {

    agg::rendering_buffer rbuf;
    rbuf.attach(buf, c.width, c.height, c.width * 4);
    pixfmt pixf(rbuf);
    renderer_base ren_base(pixf);
    ren_base.clear(Color(0, 0, 0, 0));
    renderer_scanline ren(ren_base);
    const double pad = c.padding;

    assert(swf->shapes.size());
    double x1 = 0;
    double x2 = 0;
    double y1 = 0;
    double y2 = 0;
    Matrix identity;
    DisplayTree::GetShapeBounds(swf->shapes[0], identity, &x1, &x2, &y1, &y2);
    agg::trans_viewport vp;
    vp.preserve_aspect_ratio(0.5, 0.5, agg::aspect_ratio_meet);
    vp.world_viewport(x1 - pad, y1 - pad, x2 + pad, y2 + pad);
    vp.device_viewport(0, 0, c.width, c.height);
    const Matrix view_transform = vp.to_affine();
    for (std::vector<Shape>::const_iterator it = swf->shapes.begin();
         it != swf->shapes.end(); ++it) {
      DisplayTree::RenderShape(*it, view_transform, NULL, c.width, c.height, ren_base, ren);
    }
  }
  return 0;
}

int render_to_png_file(const RunConfig& c) {
  unsigned char* buf = new unsigned char[c.width * c.height * 4];
  double origin_x = 0.0;
  double origin_y = 0.0;
  render_to_buffer(c, buf, &origin_x, &origin_y);
  unsigned error = lodepng_encode32_file(c.output_png.c_str(), buf, c.width, c.height);
  if(error) {
    printf("Error %u: %s\n", error, lodepng_error_text(error));
    return 1;
  } else {
    return 0;
  }
}

int render_to_png_buffer(const RunConfig& c, Result* result) {
  unsigned char* buf = new unsigned char[c.width * c.height * 4];
  render_to_buffer(c, buf, &result->origin_x, &result->origin_y);
  unsigned error = lodepng_encode32(&result->data, &result->size, buf, c.width, c.height);
  if(error) {
    printf("Error %u: %s\n", error, lodepng_error_text(error));
    return 1;
  } else {
    return 0;
  }
}

int get_metadata(const RunConfig& c, Result* result) {
  render_to_buffer(c, NULL, &result->origin_x, &result->origin_y);
  return 1;
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

