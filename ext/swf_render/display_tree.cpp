#include "display_tree.h"
#include <cstdlib>

namespace {

void BuildTree(
    const ParsedSWF& swf,
    const Sprite& sprite,
    DisplayTree* tree) {
  for (std::vector<Placement>::const_iterator it =
         sprite.placements.begin(); it != sprite.placements.end(); ++it) {
    const Placement& placement = *it;
    DisplayTree* child = new DisplayTree();
    child->placement = &placement;
    child->name = placement.name;
    if (const Sprite* sprite = swf.SpriteByCharacterId(placement.character_id)) {
      BuildTree(swf, *sprite, child);
    }
    else if (const Shape* shape = swf.ShapeByCharacterId(placement.character_id)) {
      child->shape = shape;
    }
    tree->children.push_back(child);
  }
}

}  // namespace

DisplayTree* DisplayTree::Build(
    const ParsedSWF& swf,
    const Sprite& sprite) {
  DisplayTree* tree = new DisplayTree();
  BuildTree(swf, sprite, tree);
  return tree;
}

void DisplayTree::GetBounds(
    const Matrix& transform,
    double* x_min_out,
    double* x_max_out,
    double* y_min_out,
    double* y_max_out) const {
  if (!visible) return;
  Matrix m(transform);
  if (placement) {
    m.premultiply(placement->matrix);
  }
  m.premultiply(matrix);
  if (shape) {
    GetShapeBounds(*shape, m, x_min_out, x_max_out, y_min_out, y_max_out);
  }
  for (std::vector<DisplayTree*>::const_iterator it =
         children.begin(); it != children.end(); ++it) {
    (*it)->GetBounds(m, x_min_out, x_max_out, y_min_out, y_max_out);
  }
}

const ColorMatrix* DisplayTree::GetColorMatrix() const {
  if (placement) {
    for (std::vector<Filter>::const_iterator it =
           placement->filters.begin(); it != placement->filters.end(); ++it) {
      if (it->filter_type == Filter::kFilterColorMatrix) {
        return &it->color_matrix;
      }
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

  if (!visible) return 0;
  Matrix m(transform);
  if (placement) {
    m.premultiply(placement->matrix);
  }
  m.premultiply(matrix);
  const ColorMatrix* color_m = color_matrix;
  if (const ColorMatrix* cm = GetColorMatrix()) {
    // We don't currently support multiplying color matrices.
    assert(color_m == NULL);
    color_m = cm;
  }
  if (shape) {
    RenderShape(*shape, m, color_m, clip_width, clip_height, ren_base, ren);
  }
  for (std::vector<DisplayTree*>::const_iterator it =
         children.begin(); it != children.end(); ++it) {
    (*it)->Render(m, color_m, clip_width, clip_height, ren_base, ren);
  }
  return 0;
}

void DisplayTree::GetShapeBounds(
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

int DisplayTree::RenderShape(
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

static const char kNewline = '\n';
static const char kSingleQuote = '\'';
static const char kFalse = 'f';
static const char kTrue = 't';
static const char kColon = ':';
static const char kDot = '.';
static const char kEquals = '=';

enum SpecState {
  kAtStart,
  kOther
};

DisplayTree* DisplayTree::ChildByName(const char* name) {
  for (std::vector<DisplayTree*>::const_iterator it =
         children.begin(); it != children.end(); ++it) {
    DisplayTree* child = *it;
    if (child->name == name) {
      return child;
    }
  }
  return NULL;
}

DisplayTree* DisplayTree::DescendantByPath(const char* path) {
  const char* c = path;
  char name[200];
  char* n = name;
  DisplayTree* p = NULL;
  while (*c) {
    if (*c == kDot || *c == kNewline) {
      *n = '\0';
      p = ChildByName(name);
      return *c == kNewline ? p : p->DescendantByPath(c + 1);
    } else {
      *n = *c;
    }
    ++n;
    ++c;
  }
  return NULL;
}

void DisplayTree::SetColor(const Color& color) {
  Filter filter;
  filter.filter_type = Filter::kFilterColorMatrix;
  filter.color_matrix = ColorMatrix::WithColor(color);
  filters.push_back(filter);
}

void ApplyProperty(const char* property, DisplayTree* target) {
  const char* c = property;
  char key[100];
  char value[100];
  char* n = key;
  while (*c && *c != kNewline) {
    if (*c == kEquals) {
      *n = '\0';
      n = value;
    } else {
      *n = *c;
      ++n;
    }
    ++c;
  }
  *n = '\0';

  double sx = 1.0;
  double sy = 1.0;
  if (strcmp(key, "v") == 0) {
    target->visible = value[0] == 't';
  }
  else if (strcmp(key, "c") == 0) {
    char* v = value;
    v += 3; // Go past '0x
    char * p;
    const unsigned n = strtoul(v, &p, 16);
    assert(*p == '\'');
    Color color((n >> 16) & 0xFF,
                (n >> 8) & 0xFF,
                n & 0xFF,
                0xFF);
    target->SetColor(color);
  }
  else if (strcmp(key, "sx") == 0) {
    char * p;
    sx = strtod(value, &p);
  }
  else if (strcmp(key, "sy") == 0) {
    char * p;
    sy = strtod(value, &p);
  }
  target->matrix.scale(sx, sy);
}

void DisplayTree::ApplySpec(const char* spec) {
  SpecState state = kAtStart;
  DisplayTree* target = NULL;
  const char* c = spec;
  while (*c) {
    if (state == kAtStart) {
      if (*c == kColon) {
        target = DescendantByPath(c + 1);
      } else if (target) {
        ApplyProperty(c, target);
      }
      state = kOther;
    }
    if (*c == kNewline) {
      state = kAtStart;
    }
    ++c;
  }
}
