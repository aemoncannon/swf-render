#include "flash_rasterizer.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <map>
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

namespace agg
{
    struct path_style
    {
      unsigned path_id;
      int left_fill;
      int right_fill;
      int line;
      bool new_styles;
    };


    class compound_shape
    {
    public:

        ~compound_shape()
        {}

        compound_shape() :
            m_path(),
            m_affine(),
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
        rgba8 color(unsigned fill_style_index) const
        {
          const FillStyle& fill = (*m_fill_styles)[fill_style_index];
          if (fill.type == FillStyle::kSolid) {
            return make_rgba(fill.rgba);

          } else if (fill.type == FillStyle::kGradientLinear) {
            return rgba8(0, 0, 0, 255);

          } else {
            return rgba8(255, 0, 0, 255);
          }
        }

        // Generate span. In our test case only one style (style=1)
        // can be a span generator, so that, parameter "style"
        // isn't used here.
        //---------------------------------------------
        void generate_span(rgba8* span, int x, int y, unsigned len, unsigned style) {
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
            m_path.remove_all();
            m_styles.remove_all();
            int last_move_y = 0;
            int last_move_x = 0;
            int last_fill0 = -1;
            int last_fill1 = -1;
            int last_line_style = -1;
            int fill_style_offset = 0;
            double ax, ay, cx, cy;
            for (int i = m_record_index; i < m_shape->records.size(); ++i) {
              const ShapeRecord* record = m_shape->records[i];
              switch (record->RecordType()) {
              case ShapeRecord::kStyleChange: {
                const StyleChangeRecord* sc =
                    static_cast<const StyleChangeRecord*>(record);

                // This marks the beginning of a new grouping.
                if (sc->HasNewStyles() && i != m_record_index) {
                  m_record_index = i;
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
                m_styles.add(style);
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
            return false;
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

        void scale(double w, double h)
        {
            m_affine.reset();
            double x1, y1, x2, y2;
            bounding_rect(m_path, *this, 0, m_styles.size(), 
                          &x1, &y1, &x2, &y2);
            if(x1 < x2 && y1 < y2)
            {
                trans_viewport vp;
                vp.preserve_aspect_ratio(0.5, 0.5, aspect_ratio_meet);
                vp.world_viewport(x1, y1, x2, y2);
                vp.device_viewport(0, 0, w, h);
                m_affine = vp.to_affine();
            }
            m_curve.approximation_scale(m_affine.scale());
        }

        void approximation_scale(double s)
        {
            m_curve.approximation_scale(m_affine.scale() * s);
        }

        const std::vector<FillStyle>* m_fill_styles;
        const std::vector<LineStyle>* m_line_styles;
        trans_affine                              m_affine;

    private:
        path_storage                              m_path;

        conv_curve<path_storage>                  m_curve;
        conv_transform<conv_curve<path_storage> > m_trans;
        pod_bvector<path_style>                   m_styles;
        double                                    m_x1, m_y1, m_x2, m_y2;
        int m_record_index;

        const Shape* m_shape;
    };

}  // namespace agg

  typedef agg::pixfmt_bgra32_pre pixfmt;
  typedef agg::renderer_base<pixfmt> renderer_base;
  typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_scanline;
  typedef agg::scanline_u8 scanline;

int render_shape(const ParsedSWF& swf,
                 const Shape& shape, const Matrix& transform,
                 int clip_width, int clip_height,
                 renderer_base& ren_base,
                 renderer_scanline& ren) {
  agg::compound_shape  m_shape;
  m_shape.set_shape(&shape);
  m_shape.m_affine = transform;
  m_shape.read_next();
  do {
//    m_shape.scale(clip_width, height);
    agg::rasterizer_scanline_aa<agg::rasterizer_sl_clip_dbl> ras;
    agg::rasterizer_compound_aa<agg::rasterizer_sl_clip_dbl> rasc;
    agg::scanline_u8 sl;
    agg::scanline_bin sl_bin;
    Matrix m_scale;
    agg::conv_transform<agg::compound_shape> shape(m_shape, m_scale);
    agg::conv_stroke<agg::conv_transform<agg::compound_shape> > stroke(shape);
    agg::span_allocator<agg::rgba8> alloc;
//    m_shape.approximation_scale(m_scale.scale());
//    printf("Filling shapes.\n");
    // Fill shape
    //----------------------
    rasc.clip_box(0, 0, clip_width, clip_height);
    rasc.reset();
    rasc.layer_order(agg::layer_direct);
    for(int i = 0; i < m_shape.paths(); i++)
    {
      rasc.styles(m_shape.style(i).left_fill,
                  m_shape.style(i).right_fill);
      rasc.add_path(shape, m_shape.style(i).path_id);
    }
    agg::render_scanlines_compound(rasc, sl, sl_bin, ren_base, alloc, m_shape);

    ras.clip_box(0, 0, clip_width, clip_height);
    for(int i = 0; i < m_shape.paths(); i++) {
      ras.reset();
      if(m_shape.style(i).line >= 0) {
        const LineStyle& style = m_shape.line_style(m_shape.style(i).line);
        if (style.width == 0) continue;
        const float width = (float)style.width * m_shape.m_affine.scale();
        stroke.width(width);
        switch (style.join_style) {
        case LineStyle::kJoinBevel:
          stroke.line_join(agg::bevel_join);
          break;
        case LineStyle::kJoinMiter:
          stroke.line_join(agg::miter_join);
          stroke.miter_limit(style.miter_limit_factor);
          break;
        case LineStyle::kJoinRound:  // Fall through
        default:
          stroke.line_join(agg::round_join);
          break;
        }
        switch (style.start_cap_style) {
        case LineStyle::kCapRound:
          stroke.line_cap(agg::round_cap);
          break;
        case LineStyle::kCapSquare:
          stroke.line_cap(agg::square_cap);
          break;
        case LineStyle::kCapNone:  // Fall through
        default:
          stroke.line_cap(agg::butt_cap);
          break;
        }
        ras.add_path(stroke, m_shape.style(i).path_id);
        ren.color(make_rgba(style.rgba));
        agg::render_scanlines(ras, sl, ren);
      }
    }
  } while (m_shape.read_next());
  return 0;
}

int render_sprite(const ParsedSWF& swf,
                  const Sprite& sprite,
                  const Matrix& transform,
                  int clip_width,
                  int clip_height,
                  renderer_base& ren_base,
                  renderer_scanline& ren) {
  for (auto it = sprite.placements.begin(); it != sprite.placements.end(); ++it) {
    const Placement& placement = *it;
    Matrix m(transform);
    m.premultiply(placement.matrix);
    if (const Sprite* sprite = swf.SpriteByCharacterId(placement.character_id)) {
      render_sprite(swf, *sprite, m, clip_width, clip_height, ren_base, ren);
    }
    if (const Shape* shape = swf.ShapeByCharacterId(placement.character_id)) {
      render_shape(swf, *shape, m, clip_width, clip_height, ren_base, ren);
    }
  }
  return 0;
}

int render_to_buffer(const char* input_swf, const char* class_name, unsigned char* buf, int width, int height) {
  TinySWFParser parser;
  ParsedSWF* swf = parser.parse(input_swf);
  //swf->Dump();
  assert(swf);

  const Shape* shape = &swf->shapes[0];
  const int x1 = shape->shape_bounds.x_min - 10;
  const int x2 = shape->shape_bounds.x_max + 10;
  const int y1 = shape->shape_bounds.y_min - 50;
  const int y2 = shape->shape_bounds.y_max + 50;
  agg::trans_viewport vp;
  vp.preserve_aspect_ratio(0.5, 0.5, agg::aspect_ratio_meet);
  vp.world_viewport(x1, y1, x2, y2);
  vp.device_viewport(0, 0, width, height);
  const Matrix view_transform = vp.to_affine();

  agg::rendering_buffer rbuf;
  rbuf.attach(buf, width, height, width * 4);
  pixfmt pixf(rbuf);
  renderer_base ren_base(pixf);
//  ren_base.clear(agg::rgba(1.0, 1.0, 1.0));
  renderer_scanline ren(ren_base);

  if (const Sprite* sprite = swf->SpriteByClassName(class_name)) {
    render_sprite(*swf, *sprite, view_transform, width, height, ren_base, ren);
  }

  return 0;
}

int render_to_png_file(const char* input_swf, const char* class_name, const char* output_png) {
  int width = 1200;
  int height = 800;
  unsigned char* buf = new unsigned char[width * height * 4];
  render_to_buffer(input_swf, class_name, buf, width, height);
  unsigned error = lodepng_encode32_file(output_png, buf, width, height);
  if(error) {
    printf("Error %u: %s\n", error, lodepng_error_text(error));
    return 1;
  } else {
    return 0;
  }
}

int render_to_png_buffer(const char* input_swf,
                         const char* class_name,
                         int width,
                         int height,
                         unsigned char** out,
                         size_t* outsize) {
  unsigned char* buf = new unsigned char[width * height * 4];
  render_to_buffer(input_swf, class_name, buf, width, height);
  unsigned error = lodepng_encode32(out, outsize, buf, width, height);
  if(error) {
    printf("Error %u: %s\n", error, lodepng_error_text(error));
    return 1;
  } else {
    return 0;
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s file.swf\n", argv[0]);
    return TRUE;
  }
  return render_to_png_file(argv[1], argv[2], "out.png");
}

