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

#include "tiny_common.h"
#include "tiny_swfparser.h"
#include "utils.h"

namespace agg
{
    struct path_style
    {
      unsigned path_id;
      int left_fill;
      int right_fill;
      int line;
      bool new_styles;
      bool operator<(const path_style& other) const {
return left_fill < other.left_fill;
      }
    };


    class compound_shape
    {
    public:

        ~compound_shape()
        {}

        compound_shape() :
            m_path(),
            m_affine(),
            m_color_matrix(NULL),
            m_curve(m_path),
            m_trans(m_curve, m_affine),
            m_styles()
        {}

        const LineStyle& line_style(unsigned line_style_index) const
        {
          return (*m_line_styles)[line_style_index];
        }

        bool is_solid(unsigned fill_style_index) const
        {
          const FillStyle& fill = (*m_fill_styles)[fill_style_index];
          switch (fill.type) {
          case FillStyle::kGradientLinear:
          case FillStyle::kGradientRadial:
          case FillStyle::kGradientFocal:
            return false;
          default: return true;
          }
        }

        // Just returns a color
        //---------------------------------------------
        Color color(unsigned fill_style_index) const
        {
          const FillStyle& fill = (*m_fill_styles)[fill_style_index];
          if (fill.type == FillStyle::kSolid) {
            Color c = make_rgba(fill.rgba);
            if (m_color_matrix) {
              m_color_matrix->transform(&c);
            }
            return c;
          } else if (fill.type == FillStyle::kGradientLinear) {
            return Color(0, 0, 0, 255);

          } else {
            return Color(255, 0, 0, 255);
          }
        }

        // Generate span. In our test case only one style (style=1)
        // can be a span generator, so that, parameter "style"
        // isn't used here.
        //---------------------------------------------
        void generate_span(Color* span, int x, int y, unsigned len, unsigned style) {
          const FillStyle& fill_style = (*m_fill_styles)[style];
          // The initial gradient square is centered at (0,0),
          // and extends from (-16384,-16384) to (16384,16384).
          // Transform each point *back* to this box and then
          // compute the gradient coordinate at that point.
          Matrix local_to_gradient(m_affine);
          local_to_gradient.premultiply(fill_style.matrix);
          local_to_gradient.invert();
          for (int i = 0; i < len; i++) {
            double grad_x = x + i;
            double grad_y = y;
            local_to_gradient.transform(&grad_x, &grad_y);
            span[i] = fill_style.gradient_color(grad_x, grad_y);
            if (m_color_matrix) {
              m_color_matrix->transform(&span[i]);
            }
          }
        }

        void set_shape(const Shape* shape)
        {
          m_shape = shape;
          m_fill_styles = &m_shape->fill_styles;
          m_line_styles = &m_shape->line_styles;
          m_record_index = 0;
        }

        // Advance to next simple shape (no groups, no overlapping)
        bool read_next()
        {
          if (m_record_index >= m_shape->records.size()) return false;
            m_path.remove_all();
            m_styles.clear();
            int last_move_x = 0;
            int last_move_y = 0;
            int last_fill0 = -1;
            int last_fill1 = -1;
            int last_line_style = -1;
            int fill_style_offset = 0;
            for (int i = m_record_index; i < m_shape->records.size(); ++i) {
              const ShapeRecord* record = m_shape->records[i];
              switch (record->RecordType()) {
              case ShapeRecord::kStyleChange: {
                const StyleChangeRecord* sc =
                    static_cast<const StyleChangeRecord*>(record);

                // This marks the beginning of a new grouping.
                if (sc->HasNewStyles() && i != m_record_index) {
                  m_record_index = i;
                  // We need to draw strokes in order of their line style.
                  // See: http://wahlers.com.br/claus/blog/hacking-swf-1-shapes-in-flash/
                  std::sort(m_styles.begin(), m_styles.end());
                  return true;
                }

                path_style style;
                style.path_id = m_path.start_new_path();
                style.new_styles = sc->HasNewStyles();
                if (sc->HasNewStyles()) {
                  m_fill_styles = &sc->fill_styles;
                  m_line_styles = &sc->line_styles;
                }

                if (sc->HasFillStyle0()) {
                  const int f = sc->fill_style0;
                  last_fill0 = f;
                  style.left_fill = f;
                } else {
                  style.left_fill = last_fill0;
                }

                if (sc->HasFillStyle1()) {
                  const int f = sc->fill_style1;
                  last_fill1 = f;
                  style.right_fill = f;
                } else {
                  style.right_fill = last_fill1;
                }

                if (sc->HasLineStyle()) {
                  const int f = sc->line_style;
                  last_line_style = f;
                  style.line = f;
                } else {
                  style.line = last_line_style;
                }

                if (sc->HasMoveTo()) {
                  last_move_x = sc->move_delta_x;
                  last_move_y = sc->move_delta_y;
                  m_path.move_to(last_move_x, last_move_y);
                } else {
                  m_path.move_to(last_move_x, last_move_y);
                }
                m_styles.push_back(style);
                break;
              }
              case ShapeRecord::kCurve: {
                const CurveRecord* c =
                    static_cast<const CurveRecord*>(record);
                last_move_x += c->control_delta_x;
                last_move_y += c->control_delta_y;
                m_path.curve3(last_move_x,
                              last_move_y,
                              last_move_x + c->anchor_delta_x,
                              last_move_y + c->anchor_delta_y);
                last_move_x += c->anchor_delta_x;
                last_move_y += c->anchor_delta_y;
                break;
              }
              case ShapeRecord::kEdge: {
                const EdgeRecord* e =
                    static_cast<const EdgeRecord*>(record);
                last_move_x += e->delta_x;
                last_move_y += e->delta_y;
                m_path.line_to(last_move_x,
                               last_move_y);
                break;
              }
              }
            }
            m_record_index = m_shape->records.size();
            return true;
        }


        unsigned operator [] (unsigned i) const
        {
            return m_styles[i].path_id;
        }

        unsigned paths() const { return m_styles.size(); }
        const path_style& style(unsigned i) const
        {
            return m_styles[i];
        }

        void rewind(unsigned path_id)
        {
            m_trans.rewind(path_id);
        }

        unsigned vertex(double* x, double* y)
        {
            return m_trans.vertex(x, y);
        }

        double scale() const
        {
            return m_affine.scale();
        }

        void approximation_scale(double s)
        {
            m_curve.approximation_scale(m_affine.scale() * s);
        }

        const std::vector<FillStyle>* m_fill_styles;
        const std::vector<LineStyle>* m_line_styles;
        trans_affine                              m_affine;
        const ColorMatrix*                              m_color_matrix;

    private:
        path_storage                              m_path;
        conv_curve<path_storage>                  m_curve;
        conv_transform<conv_curve<path_storage> > m_trans;
        std::vector<path_style>                   m_styles;
        double                                    m_x1, m_y1, m_x2, m_y2;
        int m_record_index;

        const Shape* m_shape;
    };

}  // namespace agg

  typedef agg::pixfmt_rgba32_plain pixfmt;
  typedef agg::renderer_base<pixfmt> renderer_base;
  typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_scanline;
  typedef agg::scanline_u8 scanline;


void get_bounds(const ParsedSWF& swf,
                const Shape& shape,
                const Matrix& transform,
                double* x_min_out,
                double* x_max_out,
                double* y_min_out,
                double* y_max_out) {
  const Rect& r = shape.shape_bounds;
  double x_min = r.x_min;
  double x_max = r.x_max;
  double y_min = r.y_min;
  double y_max = r.y_max;
  if (shape.edge_bounds.is_valid()) {
    x_min = std::min(x_min, (double)shape.edge_bounds.x_min);
    y_min = std::min(y_min, (double)shape.edge_bounds.y_min);
    x_max = std::max(x_max, (double)shape.edge_bounds.x_max);
    y_max = std::max(y_max, (double)shape.edge_bounds.y_max);
  }
  transform.transform(&x_min, &y_min);
  transform.transform(&x_max, &y_max);
  if (*x_min_out == 0 && *x_max_out == 0 &&
      *y_min_out == 0 && *y_max_out == 0) {
    *x_min_out = x_min;
    *x_max_out = x_max;
    *y_min_out = y_min;
    *y_max_out = y_max;
  } else {
    *x_min_out = std::min(x_min, *x_min_out);
    *x_max_out = std::max(x_max, *x_max_out);
    *y_min_out = std::min(y_min, *y_min_out);
    *y_max_out = std::max(y_max, *y_max_out);
  }
}

void get_bounds(const ParsedSWF& swf,
                const Sprite& sprite,
                const Matrix& transform,
                double* x_min_out,
                double* x_max_out,
                double* y_min_out,
                double* y_max_out) {
  for (std::vector<Placement>::const_iterator it =
         sprite.placements.begin(); it != sprite.placements.end(); ++it) {
    const Placement& placement = *it;
    Matrix m(transform);
    m.premultiply(placement.matrix);
    if (const Sprite* sprite = swf.SpriteByCharacterId(placement.character_id)) {
      get_bounds(swf, *sprite, m, x_min_out, x_max_out, y_min_out, y_max_out);
    }
    else if (const Shape* shape = swf.ShapeByCharacterId(placement.character_id)) {
      get_bounds(swf, *shape, m, x_min_out, x_max_out, y_min_out, y_max_out);
    }
  }
}

int render_to_buffer(const RunConfig& c, unsigned char* buf) {
  TinySWFParser parser;
  ParsedSWF* swf = parser.parse(c.input_swf.c_str());
  //swf->Dump();
  assert(swf);

  agg::rendering_buffer rbuf;
  rbuf.attach(buf, c.width, c.height, c.width * 4);
  pixfmt pixf(rbuf);
  renderer_base ren_base(pixf);
  ren_base.clear(Color(0, 0, 0, 0));
  renderer_scanline ren(ren_base);
  const double pad = c.padding;
  if (const Sprite* sprite = swf->SpriteByClassName(c.class_name.c_str())) {
    double x1 = 0;
    double x2 = 0;
    double y1 = 0;
    double y2 = 0;
    Matrix identity;
    get_bounds(*swf, *sprite, identity, &x1, &x2, &y1, &y2);
    agg::trans_viewport vp;
    vp.preserve_aspect_ratio(0.5, 0.5, agg::aspect_ratio_meet);
    vp.world_viewport(x1, y1, x2, y2);
    vp.device_viewport(0, 0, c.width, c.height);
    const double p = pad / vp.scale_x();  // Find world padding that will give the desired pixel padding.
    vp.world_viewport(x1-p, y1-p, x2+p, y2+p);
    vp.device_viewport(0, 0, c.width, c.height);
    const Matrix view_transform = vp.to_affine();
    DisplayTree* tree = DisplayTree::Build(*swf, *sprite);
    if (c.spec.size()) {
      tree->ApplySpec(c.spec.c_str());
    }
    tree->Render(view_transform, NULL, c.width, c.height, ren_base, ren);

  } else {
    assert(swf->shapes.size());
    double x1 = 0;
    double x2 = 0;
    double y1 = 0;
    double y2 = 0;
    Matrix identity;
    get_bounds(*swf, swf->shapes[0], identity, &x1, &x2, &y1, &y2);
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
  render_to_buffer(c, buf);
  unsigned error = lodepng_encode32_file(c.output_png.c_str(), buf, c.width, c.height);
  if(error) {
    printf("Error %u: %s\n", error, lodepng_error_text(error));
    return 1;
  } else {
    return 0;
  }
}

int render_to_png_buffer(const RunConfig& c,
                         unsigned char** out,
                         size_t* outsize) {
  unsigned char* buf = new unsigned char[c.width * c.height * 4];
  render_to_buffer(c, buf);
  unsigned error = lodepng_encode32(out, outsize, buf, c.width, c.height);
  if(error) {
    printf("Error %u: %s\n", error, lodepng_error_text(error));
    return 1;
  } else {
    return 0;
  }
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

  config.spec = "width\t50.0\nname\t'eyhnkwigkw'\ncreature_id\t2930\nsex\t'male'\nspecies\t'FuzCub'\nx\t0\ny\t0\nlife_stage\t\nspeed\t0.6000000000000001\nplayfulness\t0.75\nsleepiness\t0.0\n:body.bow\tv=f\n:body.face_patterns\tv=t\tc='0xc7ab91'\n:body.face.eyes.left_eye.lid\tv=t\tc='0xbb9754'\n:body.face.eyes.right_eye.lid\tv=t\tc='0xbb9754'\n:body.left_whiskers\tv=t\n:body.right_whiskers\tv=t\n:body.right_ear\tsy=0.5\n:body.left_ear\tsy=0.5\n:body.face.nose\tsx=3.0\tsy=3.0\n:body.face.eyes.right_eye\tsx=0.85\tsy=0.85\n:body.face.eyes.left_eye\tsx=0.85\tsy=0.85\n:body.face.eyes.left_eye.eye_color\tc='0x0d4a5e'\n:body.face.eyes.right_eye.eye_color\tc='0x0d4a5e'\n:body.color\tc='0xc9ac76'\n:right_leg.color\tc='0xc9ac76'\n:left_leg.color\tc='0xc9ac76'\n:right_foot.color\tc='0xc9ac76'\n:left_foot.color\tc='0xc9ac76'\n:body.right_ear.color\tc='0xc9ac76'\n:body.left_ear.color\tc='0xc9ac76'\n:body.face.eye_rings.color\tc='0xc9ac76'\n:body.hair.color\tc='0xc9ac76'\n:body.face_patterns.spot1\tv=f\n:body.face_patterns.spot2\tv=f\n:body.face_patterns.spot3\tv=f\n:body.face_patterns.spot4\tv=t\n:body.face_patterns.spot5\tv=f\n:body.face_patterns.spot6\tv=f\n:body.face_patterns.spot7\tv=f\n:body.face_patterns.spot8\tv=f\n:body.face_patterns.spot9\tv=f\n:body.face_patterns.spot10\tv=t\n:body.face_patterns.spot11\tv=f\n:body.face_patterns.spot12\tv=t\n:body.face_patterns.spot13\tv=f\n:body.face_patterns.spot14\tv=t\n:body.face_patterns.spot15\tv=f\n:body.face_patterns.spot16\tv=f\n:body.face_patterns.spot17\tv=t\n:body.face_patterns.spot18\tv=f\n:body.face_patterns.spot19\tv=t\n:body.face_patterns.spot20\tv=f\n:body.face_patterns.spot21\tv=f\n:body.face_patterns.spot22\tv=f\n:body.face_patterns.spot23\tv=f";

  if (optind > (argc - 1)) {
    fprintf(stderr, "Missing required input swf filename\n");
    return 1;
  }
  config.input_swf = argv[optind];

  return render_to_png_file(config);
}

