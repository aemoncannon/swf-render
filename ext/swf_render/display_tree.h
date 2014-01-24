#ifndef _DISPLAYTREE_H
#define _DISPLAYTREE_H

#include "agg_rendering_buffer.h"
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
#include "agg_pixfmt_rgba.h"
#include "agg_bounding_rect.h"

#include "tiny_swfparser.h"


typedef agg::pixfmt_rgba32_plain pixfmt;
typedef agg::renderer_base<pixfmt> renderer_base;
typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_scanline;
typedef agg::scanline_u8 scanline;

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


class DisplayTree {
public:
  DisplayTree() 
    : placement(NULL),
      shape(NULL),
      visible(true) {}

  static DisplayTree* Build(
      const ParsedSWF& swf,
      const Sprite& sprite);

  // Apply a sequence of modification commands to
  // the display tree. A poor man's Actionscript.
  void ApplySpec(const char* spec);

  // Returns the child who's instance name is name, or
  // NULL if no such child exists.
  DisplayTree* ChildByName(const char* name);

  // Returns the descendent denoted by the chain of instance names
  // name(.name)*. Returns NULL if no such descendent exists.
  DisplayTree* DescendantByPath(const char* path);

  const ColorMatrix* GetColorMatrix() const;

  void SetColor(const Color& color);

  void GetBounds(
      const Matrix& transform,
      double* x_min_out,
      double* x_max_out,
      double* y_min_out,
      double* y_max_out) const;

  int Render(const Matrix& transform,
             const ColorMatrix* color_matrix,
             int clip_width,
             int clip_height,
             renderer_base& ren_base,
             renderer_scanline& ren) const;

  static int RenderShape(
      const Shape& shape,
      const Matrix& transform,
      const ColorMatrix* color_matrix,
      int clip_width, int clip_height,
      renderer_base& ren_base,
      renderer_scanline& ren);

  static void GetShapeBounds(
      const Shape& shape,
      const Matrix& transform,
      double* x_min_out,
      double* x_max_out,
      double* y_min_out,
      double* y_max_out);

  const Placement* placement;
  const Shape* shape;
  Matrix matrix;
  std::vector<DisplayTree*> children;
  std::vector<Filter> filters;
  std::string name;
  bool visible;
};

#endif