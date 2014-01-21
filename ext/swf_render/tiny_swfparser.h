#include "tiny_common.h"
#include "tiny_SWFStream.h"
#include "agg_trans_affine.h"
#include "agg_color_rgba.h"
#include <vector>
#include <string>
#include <map>
#include <assert.h>


#ifndef _TINYSWFPARSER_H
#define _TINYSWFPARSER_H

typedef agg::rgba8 Color;
typedef agg::trans_affine Matrix;

Color make_rgba(unsigned v);

class Rect {
 public:
 Rect() : x_min(0), y_min(0), x_max(0), y_max(0) {}
  bool is_valid() const {
    return x_min != 0 || y_min != 0 || x_max != 0 || y_max != 0;
  }
  int x_min;
  int x_max;
  int y_min;
  int y_max;
  void Dump() const;
};

class FillStyle {
 public:
  FillStyle()
    : type(kSolid), rgba(0), focal_point(0) {}
  enum Type {
    kSolid = 0x00,
    kGradientLinear = 0x10,
    kGradientRadial = 0x12,
    kGradientFocal = 0x13,
    kRepeatingBitmap = 0x40,
    kClippedBitmap = 0x41,
    kNonSmoothedRepeatingBitmap = 0x42,
    kNonSmoothedClippedBitmap = 0x43
  };
  Type type;
  unsigned int rgba;
  Matrix matrix;
  // Expects coordinates in the untransformed gradient coordinate space.
  Color gradient_color(double grad_x, double grad_y) const;
  // Pairs of ratio (0.0-1.0), rgba
  std::vector<std::pair<float, Color> > gradient_entries;
  float focal_point;
  void Dump() const;
};

class LineStyle {
 public:
 LineStyle()
   : width(1),
    start_cap_style(kCapRound),
    join_style(kJoinRound),
    has_fill(false),
    no_hscale_flag(0),
    no_vscale_flag(0),
    pixel_hinting_flag(0),
    no_close(0),
    end_cap_style(kCapRound),
    miter_limit_factor(0),
    rgba(0) {}
  enum JoinStyle {
    kJoinRound = 0,
    kJoinBevel = 1,
    kJoinMiter = 2
  };
  enum CapStyle {
    kCapRound = 0,
    kCapNone = 1,
    kCapSquare = 2
  };
  unsigned int width;
  CapStyle start_cap_style;
  JoinStyle join_style;
  bool has_fill;
  unsigned int no_hscale_flag;
  unsigned int no_vscale_flag;
  unsigned int pixel_hinting_flag;
  unsigned int no_close;
  CapStyle end_cap_style;
  float miter_limit_factor;
  unsigned int rgba;
  FillStyle fill;
  void Dump() const;
};

class ShapeRecord {
 public:
  enum Type {
    kStyleChange,
    kCurve,
    kEdge
  };
  virtual ~ShapeRecord() {}
  virtual void Dump() const = 0;
  virtual Type RecordType() const = 0;
};

class StyleChangeRecord : public ShapeRecord {
 public:
   StyleChangeRecord()
   : flags(0),
    move_delta_x(0),
    move_delta_y(0),
    fill_style0(-1),
    fill_style1(-1),
    line_style(-1) {}
  unsigned int flags;
  int move_delta_x;
  int move_delta_y;
  int fill_style0; // Selectors of FillStyles and LineStyles. Arrays begin at index 1. -1 denotes no style.
  int fill_style1;
  int line_style;
  bool HasNewStyles() const { return (flags >> 4) & 1; }
  bool HasLineStyle() const { return (flags >> 3) & 1; }
  bool HasFillStyle1() const { return (flags >> 2) & 1; }
  bool HasFillStyle0() const { return (flags >> 1) & 1; }
  bool HasMoveTo() const { return flags & 1; }
  std::vector<FillStyle> fill_styles;
  std::vector<LineStyle> line_styles;
  virtual void Dump() const;
  virtual Type RecordType() const { return kStyleChange; }
};

class EdgeRecord : public ShapeRecord {
 public:
  int delta_x;
  int delta_y;
  virtual void Dump() const;
  virtual Type RecordType() const { return kEdge; }
};

class CurveRecord : public ShapeRecord {
 public:
  int control_delta_x;
  int control_delta_y;
  int anchor_delta_x;
  int anchor_delta_y;
  virtual void Dump() const;
  virtual Type RecordType() const { return kCurve; }
};

class Shape {
 public:
 Shape() : character_id(-1),
    uses_fill_winding_rule(false),
    uses_non_scaling_strokes(false),
    uses_scaling_strokes(false) {}
  int character_id;
  Rect shape_bounds;
  Rect edge_bounds;
  bool uses_fill_winding_rule;
  bool uses_non_scaling_strokes;
  bool uses_scaling_strokes;
  std::vector<const ShapeRecord*> records;
  std::vector<FillStyle> fill_styles;
  std::vector<LineStyle> line_styles;
  void Dump() const;
};

struct ColorMatrix {
ColorMatrix() {
  m[0] = 1;
  m[1] = 0;
  m[2] = 0;
  m[3] = 0;
  m[4] = 0;
  m[5] = 0;
  m[6] = 1;
  m[7] = 0;
  m[8] = 0;
  m[9] = 0;
  m[10] = 0;
  m[11] = 0;
  m[12] = 1;
  m[13] = 0;
  m[14] = 0;
  m[15] = 0;
  m[16] = 0;
  m[17] = 0;
  m[18] = 1;
  m[19] = 0;
 }
  float m[20];
  void transform(Color* c) const {
    int r = m[0]*(float)c->r + m[1]*(float)c->g + m[2]*(float)c->b + m[3]*(float)c->a + m[4];
    r = r < 0 ? 0 : r;
    r = r > 255 ? 255 : r;
    int g = m[5]*(float)c->r + m[6]*(float)c->g + m[7]*(float)c->b + m[8]*(float)c->a + m[9];
    g = g < 0 ? 0 : g;
    g = g > 255 ? 255 : g;
    int b = m[10]*(float)c->r + m[11]*(float)c->g + m[12]*(float)c->b + m[13]*(float)c->a + m[14];
    b = b < 0 ? 0 : b;
    b = b > 255 ? 255 : b;
    int a = m[15]*(float)c->r + m[16]*(float)c->g + m[17]*(float)c->b + m[18]*(float)c->a + m[19];
    a = a < 0 ? 0 : a;
    a = a > 255 ? 255 : a;
    *c = Color(r, g, b, a);
  }
  void Dump() const;
};

class Filter {
 public:
  enum FilterType {
    kFilterDropShadow,
    kFilterBlur,
    kFilterGlow,
    kFilterBevel,
    kFilterGradientGlow,
    kFilterConvolution,
    kFilterColorMatrix,
    kFilterGradientBevel
  };
  FilterType filter_type;
  unsigned int rgba;
  ColorMatrix color_matrix;
};

class Placement {
 public:
 Placement() : character_id(-1), depth(-1) {}
  bool operator<(const Placement& other) const { return depth < other.depth; }
  int character_id;
  int depth;
  std::vector<Filter> filters;
  Matrix matrix;
};

class Sprite {
 public:
  unsigned int character_id;
  unsigned int frame_count;
  std::vector<Placement> placements;
  void Dump() const {}
};

class ParsedSWF {
 public:
  Rect frame_size;
  float frame_rate;
  unsigned int frame_count;
  std::vector<Shape> shapes;
  std::vector<Sprite> sprites;
  std::map<int, int> character_id_to_shape_index;
  std::map<int, int> character_id_to_sprite_index;
  std::map<std::string, int> class_name_to_character_id;
  const Sprite* SpriteByClassName(const char* class_name) const;
  const Sprite* SpriteByCharacterId(int character_id) const;
  const Shape* ShapeByCharacterId(int character_id) const;

  void Dump() const;
};

// return false if want to stop parsing, give the caller a chance to stop the parsing loop.
typedef int (*ProgressUpdateFunctionPtr)(unsigned int progress);

class TinySWFParser	: public SWFStream {
 public:
  TinySWFParser();
  ~TinySWFParser();
  ParsedSWF* parse(const char *filename);

 private:
  int HandleSymbolClass(Tag *tag, ParsedSWF* swf);
  int HandleDefineSprite(Tag *tag, ParsedSWF* swf);
  void HandleDefineShape(Tag* tag, ParsedSWF* swf);
  void HandlePlaceObject23(Tag* tag, Sprite* sprite, int current_frame);
  unsigned int	getRGB();
  unsigned int	getARGB() { return getUI32(); }
  unsigned int	getRGBA() { return getUI32(); }
  int             getGRADIENT(Tag *tag, FillStyle* style);
  int             getFOCALGRADIENT(Tag *tag, FillStyle* style);
  int             getRECT(Rect* rect);
  int             getMATRIX(Matrix* matrix);
  int             getFILLSTYLE(Tag *tag, FillStyle* styles);
  int             getFILLSTYLEARRAY(Tag *tag, std::vector<FillStyle>* styles);
  int             getLINESTYLEARRAY(Tag *tag, std::vector<LineStyle>* styles);
  int             getSHAPE(Tag *tag, Shape* shape);
  int             getSHAPEWITHSTYLE(Tag *tag, Shape* shape);
  int             getFILTERLIST(Placement* placement);    // SWF8 or later
  int             getTagCodeAndLength(Tag *tag);
  int             getCXFORM();
  int             getCXFORMWITHALPHA();
  int             getCOLORMATRIXFILTER(Filter* filter);
  int             getBLURFILTER(Filter* filter);
  int             getGLOWFILTER(Filter* filter);
};

#endif
