#include "flash_rasterizer.h"

#include <math.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
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
    };

    class compound_shape
    {
    public:
      class styles
      {
      public:
        styles() {

          for(unsigned i = 0; i < 100; i++)
            {
              solid_colors.push_back(agg::rgba8(
                (rand() & 0xFF),
                (rand() & 0xFF),
                (rand() & 0xFF),
                230));
            }
        }
      
        //---------------------------------------------
        bool is_solid(unsigned style) const
        {
          return true;//style != 1;
        }
      
        // Just returns a color
        //---------------------------------------------
        rgba8 color(unsigned style) const
        {
          if (style == 0) return rgba8(0, 0, 0, 0);
          return solid_colors[style-1];
        }

        void add_rgba_solid(unsigned int v) {
          printf("0x%x\n", v);
          solid_colors.push_back(rgba8((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF, v >> 24));
        }
      
        // Generate span. In our test case only one style (style=1)
        // can be a span generator, so that, parameter "style"
        // isn't used here.
        //---------------------------------------------
        void generate_span(rgba8* span, int x, int y, unsigned len, unsigned style)
        {
//          printf("genearte span");
//          memcpy(span, m_gradient + x, sizeof(rgba8) * len);
        }

        void remove_all()
        {
          solid_colors.clear();
        }
      
        std::vector<rgba8> solid_colors;
      };

        ~compound_shape()
        {}

        compound_shape() :
            m_path(),
            m_affine(),
            m_curve(m_path),
            m_trans(m_curve, m_affine),
            m_styles()
        {}

        void set_shape(const Shape* shape)
        {
          m_shape = shape;
        }

        bool read_next()
        {
            m_path.remove_all();
            m_styles.remove_all();
            style_library.remove_all();
            for (int i = 0; i < m_shape->fill_styles.size(); ++i) {
              const FillStyle& fill = m_shape->fill_styles[i];
              style_library.add_rgba_solid(fill.rgba);
            }

            int last_move_y = 0;
            int last_move_x = 0;
            int last_fill0 = -1;
            int last_fill1 = -1;
            int last_line_style = -1;
            double ax, ay, cx, cy;
            for (int i = 0; i < m_shape->records.size(); ++i) {
              const ShapeRecord* record = m_shape->records[i];
              switch (record->RecordType()) {
              case ShapeRecord::kStyleChange: {
                const StyleChangeRecord* sc =
                    static_cast<const StyleChangeRecord*>(record);

                if (sc->HasNewStyles()) {
                  for (int i = 0; i < sc->fill_styles.size(); ++i) {
                    const FillStyle& fill = sc->fill_styles[i];
                    style_library.add_rgba_solid(fill.rgba);
                  }
                }

                path_style style;
                style.path_id = m_path.start_new_path();
                style.left_fill = sc->HasFillStyle0() ? sc->fill_style0 : last_fill0;
                style.right_fill = sc->HasFillStyle1() ? sc->fill_style1 : last_fill1;
                style.line = sc->HasLineStyle() ? sc->line_style : last_line_style;
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

        styles style_library;

    private:
        path_storage                              m_path;
        trans_affine                              m_affine;
        conv_curve<path_storage>                  m_curve;
        conv_transform<conv_curve<path_storage> > m_trans;
        pod_bvector<path_style>                   m_styles;
        double                                    m_x1, m_y1, m_x2, m_y2;


        const Shape* m_shape;
    };




}


VObject* GetProperty(VObject* vobj, const char* key) {
  PropertyList *plist = vobj->getPropertyList();
  for (std::vector<Property>::iterator it = plist->begin(); it != plist->end(); it++) {
    if (it->name == key) {
      return it->value;
    }
  }
  return NULL;
}

VObject* GetByType(VObject* vobj, const char* tpe) {
  PropertyList *plist = vobj->getPropertyList();
  for (std::vector<Property>::iterator it = plist->begin(); it != plist->end(); it++) {
    if (strcmp(it->value->getTypeInfo(), tpe) == 0) {
      return it->value;
    }
  }
  return NULL;
}


int render_to_buffer(const char* input_swf, unsigned char* buf, int width, int height) {
	  TinySWFParser parser;
    ParsedSWF* swf = parser.parse(input_swf);
  swf->Dump();

    agg::compound_shape        m_shape;

    m_shape.set_shape(&swf->shapes[0]);
    m_shape.read_next();
    m_shape.scale(width, height);

    agg::trans_affine          m_scale;


 typedef agg::pixfmt_bgra32_pre pixfmt;

        agg::rendering_buffer rbuf;
        rbuf.attach(buf, width, height, width * 4);

        typedef agg::renderer_base<pixfmt> renderer_base;
        typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_scanline;
        typedef agg::scanline_u8 scanline;

        pixfmt pixf(rbuf);
        renderer_base ren_base(pixf);
        ren_base.clear(agg::rgba(1.0, 1.0, 0.95));
        renderer_scanline ren(ren_base);

        unsigned i;
        unsigned w = unsigned(width);

        agg::rasterizer_scanline_aa<agg::rasterizer_sl_clip_dbl> ras;
        agg::rasterizer_compound_aa<agg::rasterizer_sl_clip_dbl> rasc;
        agg::scanline_u8 sl;
        agg::scanline_bin sl_bin;
        agg::conv_transform<agg::compound_shape> shape(m_shape, m_scale);
        agg::conv_stroke<agg::conv_transform<agg::compound_shape> > stroke(shape);

        agg::span_allocator<agg::rgba8> alloc;

        m_shape.approximation_scale(m_scale.scale());

        printf("Filling shapes.\n");
        // Fill shape
        //----------------------
        rasc.clip_box(0, 0, width, height);
        rasc.reset();
        //rasc.filling_rule(agg::fill_even_odd);
        for(i = 0; i < m_shape.paths(); i++)
        {
            if(m_shape.style(i).left_fill >= 0 ||
               m_shape.style(i).right_fill >= 0)
            {
                rasc.styles(m_shape.style(i).left_fill,
                            m_shape.style(i).right_fill);
                rasc.add_path(shape, m_shape.style(i).path_id);
            }
        }
        agg::render_scanlines_compound(rasc, sl, sl_bin, ren_base, alloc, m_shape.style_library);

        printf("Drawing strokes.\n");
        ras.clip_box(0, 0, width, height);
        stroke.width(sqrt(m_scale.scale()));
        stroke.line_join(agg::round_join);
        stroke.line_cap(agg::round_cap);
        for(i = 0; i < m_shape.paths(); i++)
          {
            ras.reset();
            if(m_shape.style(i).line >= 0)
              {
                ras.add_path(stroke, m_shape.style(i).path_id);
                ren.color(agg::rgba8(0,0,0, 128));
                agg::render_scanlines(ras, sl, ren);
              }
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

