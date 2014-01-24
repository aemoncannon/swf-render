#include "display_tree.h"

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

namespace {

DisplayTree* Build(
    const ParsedSWF& swf,
    const Sprite& sprite,
    DisplayTree* tree) {
  for (std::vector<Placement>::const_iterator it =
         sprite.placements.begin(); it != sprite.placements.end(); ++it) {
    DisplayTree* child = new DisplayTree();
    const Placement& placement = *it;
    child->placement = &placement;
    if (const Sprite* sprite = swf.SpriteByCharacterId(placement.character_id)) {
      Build(swf, *sprite, *child);
    }
    else if (const Shape* shape = swf.ShapeByCharacterId(placement.character_id)) {
      tree->shape = shape;
    }
  }
}

}  // namespace

DisplayTree* DisplayTree::Build(
    const ParsedSWF& swf,
    const Sprite& sprite) {
  DisplayTree* tree = new DisplayTree();
  Build(swf, sprite, tree);
  return tree;
}

static const char kNewline = '\n';
static const char kSingleQuote = '\'';
static const char kFalse = 'f';
static const char kTrue = 't';
static const char kColon = ':';
static const char kDot = '.';

enum SpecState {
  kAtStart,
  kOther
};

const ColorMatrix* DisplayTree::GetColorMatrix() const {
  for (std::vector<Filter>::const_iterator it =
         placement.filters.begin(); it != placement.filters.end(); ++it) {
    if (it->filter_type == Filter::kFilterColorMatrix) {
      return &it->color_matrix;
    }
  }
  for (std::vector<Filter>::const_iterator it =
         filters.begin(); it != filters.end(); ++it) {
    if (it->filter_type == Filter::kFilterColorMatrix) {
      return &it->color_matrix;
    }
  }
  return NULL;
}


int DisplayTree::Render(
    const Matrix& transform,
    const ColorMatrix* color_matrix,
    int clip_width,
    int clip_height,
    renderer_base& ren_base,
    renderer_scanline& ren) const {

  if (!visible) return;
  Matrix m(transform);
  m.premultiply(matrix);
  m.premultiply(placement->matrix);

  const ColorMatrix* color_m = GetColorMatrix();
  if (color_matrix) {
    // We don't currently support multiplying color matrices.
    assert(color_m == NULL);
  }
  if (shape) {
    RenderShape(*shape, m, color_m, clip_width, clip_height, ren_base, ren);
  }
  for (std::vector<const DisplayTree*>::const_iterator it =
         children.begin(); it != children.end(); ++it) {
    it->Render(m, color_m, clip_width, clip_height, ren_base, ren);
  }
  return 0;
}

typedef agg::pixfmt_rgba32_plain pixfmt;
typedef agg::renderer_base<pixfmt> renderer_base;
typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_scanline;
typedef agg::scanline_u8 scanline;

static int DisplayTree::RenderShape(
    const Shape& shape,
    const Matrix& transform,
    const ColorMatrix* color_matrix,
    int clip_width, int clip_height,
    renderer_base& ren_base,
    renderer_scanline& ren) {
  agg::compound_shape  m_shape;
  m_shape.set_shape(&shape);
  m_shape.m_affine = transform;
  m_shape.m_color_matrix = color_matrix;
  while (m_shape.read_next()) {
//    m_shape.scale(clip_width, height);
    agg::rasterizer_scanline_aa<agg::rasterizer_sl_clip_dbl> ras;
    agg::rasterizer_compound_aa<agg::rasterizer_sl_clip_dbl> rasc;
    agg::scanline_u8 sl;
    agg::scanline_bin sl_bin;
    Matrix m_scale;
    agg::conv_transform<agg::compound_shape> shape(m_shape, m_scale);
    agg::conv_stroke<agg::conv_transform<agg::compound_shape> > stroke(shape);
    agg::span_allocator<Color> alloc;
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
        const double width = (double)style.width * m_shape.m_affine.scale();
        stroke.width(width);
        switch (style.join_style) {
          case LineStyle::kJoinBevel:
            stroke.line_join(agg::bevel_join);
            break;
          case LineStyle::kJoinMiter:
            stroke.line_join(agg::miter_join);
            if (style.miter_limit_factor > 0) {
              stroke.miter_limit(style.miter_limit_factor);
            }
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
        Color c = make_rgba(style.rgba);
        if (color_matrix) {
          color_matrix->transform(&c);
        }
        ren.color(c);
        ras.add_path(stroke, m_shape.style(i).path_id);
        agg::render_scanlines(ras, sl, ren);
      }
    }
  }
  return 0;  
}

DisplayTree* DisplayTree::ChildByName(const char* name) {
  for (std::vector<DisplayTree*>::const_iterator it =
         children.begin(); it != children.end(); ++it) {
    const DisplayTree* child = *it;
    if (child->name == name) {
      return child;
    }
  }
  return NULL;
}

DisplayTree* DisplayTree::DescendantByPath(const char* path) {
  char* c = path;
  const char* name[200];
  char* n = name;
  Placement* p = NULL;
  while (*c) {
    if (*c == kDot || *c == kNewline) {
      *n = '\0';
      p = ChildByName(name);
      return *c == kNewline ? p : p->DescendantByPath(c + 1);
    }
    ++n;
    ++c;
  }
  return NULL;
}

void ApplyProperty(const char* property, DisplayTree* target) {
  char* c = property;
  while (*c && *c != kNewline) {
    ++c;
  }
  target->matrix.scale(mod.sx, mod.sy);
  target->visible = mod.visible;
  Filter filter;
  filter.filter_type = kFilterColorMatrix;
  filter.color_matrix = ColorMatrix::WithColor(make_rgba(mod.rgba));
  target->filters.push_back(filter);
}

void DisplayTree::ApplySpec(const char* spec) const {
  SpecState state = kAtStart;
  Modifier modifier;
  DisplayTree* target = NULL;
  char* c = spec;
  while (*c) {
    if (state == kAtStart) {
      if (*c == kColon) {
        placement = DescendantByPath(c + 1);
      } else if (placement) {
        ApplyProperty(c, placement);
      }
      state = kOther;
    }
    if (*c == kNewline) {
      state = kStart;
    }
    ++c;
  }
}

