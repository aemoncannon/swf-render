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

  struct Image {
    Image() : buf(NULL), screen_x1(0), screen_y1(0), screen_x2(0), screen_y2(0) {}
    rgba8* buf;
    int screen_x1;
    int screen_y1;
    int screen_x2;
    int screen_y2;
    const rgba8* find_color(int x, int y) const {
      printf("%d, %d\n", x, y);
      printf("%d, %d, %d, %d\n", screen_x1, screen_y1, screen_x2, screen_y2);
//      assert(x >= screen_x1);
//      assert(y >= screen_y1);
//      assert(x <= screen_x2);
//      assert(y <= screen_y2);
      x = std::max(x, screen_x1);
      x = std::min(x, screen_x2);
      y = std::max(y, screen_y1);
      y = std::min(y, screen_y2);

      const int w = screen_x2 - screen_x1;
      return buf + ((w * y) + (x - screen_x1));
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
            m_curve(m_path),
            m_trans(m_curve, m_affine),
            m_styles()
        {}

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
            const unsigned int v = fill.rgba;
            return rgba8((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF, v >> 24);

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
          const Image& grad = m_gradients[style];
          memcpy(span, grad.find_color(x, y), sizeof(rgba8) * len);
        }

        void set_shape(const Shape* shape)
        {
          m_shape = shape;
          m_fill_styles = &m_shape->fill_styles;
          m_record_index = 0;
        }

        void create_gradients() {
          for (int i = 0; i < m_fill_styles->size(); i++) {
            const FillStyle& fill_style = (*m_fill_styles)[i];
            if (fill_style.type == FillStyle::kGradientLinear ||
                fill_style.type == FillStyle::kGradientRadial) {
              // The initial gradient square is centered at (0,0),
              // and extends from (-16384,-16384) to (16384,16384).
              // Transform that box using the fill's matrix, then
              // for each point in the transformed (hopefully smaller)
              // box, lookup the color in the original box. Can we
              // find an inverse transformation in general?
              double x1 = -16384;
              double y1 = -16384;
              double x2 = 16384;
              double y2 = 16384;
              double x3 = -16384;
              double y3 = 16384;
              double x4 = 16384;
              double y4 = -16384;
              const double kGradWidth = x2 - x1;
              trans_affine m(m_affine);
              m.premultiply(fill_style.matrix);
              // Transform all four corners
              m.transform(&x1, &y1);
              m.transform(&x2, &y2);
              m.transform(&x3, &y3);
              m.transform(&x4, &y4);
              // Now find the screen rectangle that completely
              // contains the rotated rectangle.
              const int screen_x1 = floor(std::min(x1, std::min(x2, std::min(x3, x4))));
              const int screen_y1 = floor(std::min(y1, std::min(y2, std::min(y3, y4))));
              const int screen_x2 = ceil(std::max(x1, std::max(x2, std::max(x3, x4))));
              const int screen_y2 = ceil(std::max(y1, std::max(y2, std::max(y3, y4))));
              const int screen_w = screen_x2 - screen_x1;
              const int screen_h = screen_y2 - screen_y1;
              assert(screen_w > 0);
              assert(screen_h > 0);
              rgba8* buf = new rgba8[screen_w * screen_h];

              assert(m.is_valid());
              trans_affine inverse(m);
              inverse.invert();

              for (int x = screen_x1; x < (screen_x1 + screen_w); ++x) {
                for (int y = screen_y1; y < (screen_y1 + screen_h); ++y) {
                  double grad_x = x;
                  double grad_y = y;
                  inverse.transform(&grad_x, &grad_y);
                  // grad_x, grad_y now identify a point in the original
                  // gradient rect.
                  const int color_index = y * screen_w + x;
                  buf[color_index] =
                      fill_style.gradient_color(grad_x / kGradWidth);
                }
              }
              Image& grad = m_gradients[i];
              grad.buf = buf;
              grad.screen_x1 = screen_x1;
              grad.screen_y1 = screen_y1;
              grad.screen_x2 = screen_x2;
              grad.screen_y2 = screen_y2;
            }
          }
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
                }
                create_gradients();

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
                  last_line_style = sc->line_style;
                  style.line = sc->line_style;
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
        trans_affine                              m_affine;

    private:
        path_storage                              m_path;

        conv_curve<path_storage>                  m_curve;
        conv_transform<conv_curve<path_storage> > m_trans;
        pod_bvector<path_style>                   m_styles;
        double                                    m_x1, m_y1, m_x2, m_y2;
        int m_record_index;

        const Shape* m_shape;
        // gradients stored by fill style index.
        std::map<int, Image> m_gradients;
    };

}  // namespace agg

int render_to_buffer(const char* input_swf, unsigned char* buf, int width, int height) {
  TinySWFParser parser;
  ParsedSWF* swf = parser.parse(input_swf);
//  swf->Dump();

    agg::compound_shape        m_shape;

    const Shape* shape = &swf->shapes[0];

    m_shape.set_shape(shape);

    agg::trans_affine          m_scale;


 typedef agg::pixfmt_bgra32_pre pixfmt;

        agg::rendering_buffer rbuf;
        rbuf.attach(buf, width, height, width * 4);

        typedef agg::renderer_base<pixfmt> renderer_base;
        typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_scanline;
        typedef agg::scanline_u8 scanline;

        pixfmt pixf(rbuf);
        renderer_base ren_base(pixf);
        ren_base.clear(agg::rgba(1.0, 1.0, 1.0));
        renderer_scanline ren(ren_base);

        unsigned i;
        unsigned w = unsigned(width);

        // m_shape.m_affine.scale(0.1, 0.1);
        // m_shape.m_affine.translate(100, 200);

        const int x1 = shape->shape_bounds.x_min - 10;
        const int x2 = shape->shape_bounds.x_max + 10;
        const int y1 = shape->shape_bounds.y_min - 50;
        const int y2 = shape->shape_bounds.y_max + 50;
        agg::trans_viewport vp;
        vp.preserve_aspect_ratio(0.5, 0.5, agg::aspect_ratio_meet);
        vp.world_viewport(x1, y1, x2, y2);
        vp.device_viewport(0, 0, width, height);
        m_shape.m_affine = vp.to_affine();

        while (m_shape.read_next()) {
//          m_shape.scale(width, height);
          agg::rasterizer_scanline_aa<agg::rasterizer_sl_clip_dbl> ras;
          agg::rasterizer_compound_aa<agg::rasterizer_sl_clip_dbl> rasc;
          agg::scanline_u8 sl;
          agg::scanline_bin sl_bin;
          agg::conv_transform<agg::compound_shape> shape(m_shape, m_scale);
          agg::conv_stroke<agg::conv_transform<agg::compound_shape> > stroke(shape);
          agg::span_allocator<agg::rgba8> alloc;
//          m_shape.approximation_scale(m_scale.scale());
//          printf("Filling shapes.\n");
          // Fill shape
          //----------------------
          rasc.clip_box(0, 0, width, height);
          rasc.reset();
          rasc.layer_order(agg::layer_direct);
          for(i = 0; i < m_shape.paths(); i++)
          {
            rasc.styles(m_shape.style(i).left_fill,
                        m_shape.style(i).right_fill);
            rasc.add_path(shape, m_shape.style(i).path_id);
          }
          agg::render_scanlines_compound(rasc, sl, sl_bin, ren_base, alloc, m_shape);

//sh          printf("Drawing strokes.\n");
          // ras.clip_box(0, 0, width, height);
          // stroke.width(sqrt(m_scale.scale()));
          // stroke.line_join(agg::round_join);
          // stroke.line_cap(agg::round_cap);
          // for(i = 0; i < m_shape.paths(); i++)
          //   {
          //     ras.reset();
          //     if(m_shape.style(i).line >= 0)
          //       {
          //         ras.add_path(stroke, m_shape.style(i).path_id);
          //         ren.color(agg::rgba8(0,0,0, 128));
          //         agg::render_scanlines(ras, sl, ren);
          //       }
          //   }
        }

    return 1;
}

int render_to_png_file(const char* input_swf, const char* output_png) {
  int width = 600;
  int height = 400;
  unsigned char* buf = new unsigned char[width * height * 4];
  render_to_buffer(input_swf, buf, width, height);
  unsigned error = lodepng_encode32_file(output_png, buf, width, height);
  if(error) {
    printf("Error %u: %s\n", error, lodepng_error_text(error));
    return 1;
  } else {
    return 0;
  }
}

int render_to_png_buffer(const char* input_swf,
                         int width,
                         int height,
                         unsigned char** out,
                         size_t* outsize) {
  unsigned char* buf = new unsigned char[width * height * 4];
  render_to_buffer(input_swf, buf, width, height);
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
  return render_to_png_file(argv[1], "out.png");
}

