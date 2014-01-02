#include "tiny_common.h"
#include "tiny_SWFStream.h"
#include "tiny_VObject.h"
#include "agg_trans_affine.h"
#include "agg_color_rgba.h"
#include <vector>
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
  int x_min;
  int x_max;
  int y_min;
  int y_max;
  void Dump() const;
};

class FillStyle {
 public:
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
  void Dump() const;
};

class LineStyle {
 public:
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
  Shape() : uses_fill_winding_rule(false), uses_non_scaling_strokes(false),
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
    
    //// User Operation
    ParsedSWF* parse(const char *filename);
    ParsedSWF* parseWithCallback(const char *filename, ProgressUpdateFunctionPtr progressUpdate);

    int HandleSymbolClass(Tag *tag, ParsedSWF* swf);
    int HandleDefineSprite(Tag *tag, ParsedSWF* swf);
    void HandleDefineShape(Tag* tag, ParsedSWF* swf);
    void HandlePlaceObject23(Tag* tag, Sprite* sprite, int current_frame);
    
	//// Color Operations
	unsigned int	getRGB();
	unsigned int	getARGB() { return getUI32(); }
	unsigned int	getRGBA() { return getUI32(); }
	
	//// Gradient Operation
    int             getGRADIENT(Tag *tag, FillStyle* style);
    int             getFOCALGRADIENT(Tag *tag, FillStyle* style);
	
	//// Rectangle Operations
    int             getRECT(Rect* rect);
	
	//// Matrix Operation
    int             getMATRIX(Matrix* matrix);
	
	//// Transformation
	int             getCXFORM(VObject &cxObject);
	int             getCXFORMWITHALPHA(VObject &cxObject);
	
	//// Fill & Line Styles
    int             getFILLSTYLE(Tag *tag, FillStyle* styles);
	int             getFILLSTYLEARRAY(Tag *tag, std::vector<FillStyle>* styles);
	int             getLINESTYLEARRAY(Tag *tag, std::vector<LineStyle>* styles);
    
    //// Morph Fill & Line Styles
    int             getMORPHGRADIENT(Tag *tag, VObject &gradientObject);
    int             getMORPHFILLSTYLE(Tag *tag, VObject &fillStyleObject);
	int             getMORPHFILLSTYLEARRAY(Tag *tag, VObject &fillStyleArrayObject);
    int             getMORPHLINESTYLE(Tag *tag, VObject &lineStyleObject);
	int             getMORPHLINESTYLEARRAY(Tag *tag, VObject &lineStyleArrayObject);

	//// Shape Operations
	int             getSHAPE(Tag *tag, Shape* shape);
	int             getSHAPEWITHSTYLE(Tag *tag, Shape* shape);
	
    //// TEXTRECORD
    int             getTEXTRECORD(Tag *tag, unsigned int GlyphBits, unsigned int AdvanceBits);
    
    //// BUTTONRECORD
    int             getBUTTONRECORD(Tag *tag, VObject &btnRecordObject);
    int             getBUTTONCONDACTION(VObject &btnCondObject);
    
	//// ACTIONRECORD	
	int             getACTIONRECORD(VObject &actRecordObject);
    
    //// Action Operations
    int             getActionCodeAndLength(Action *action);
	
	//// CLIPACTIONS and CLIPACTIONRECORD
	#define CLIPEVENT_KEYPRESS_MASK (1<<22) // 0x00400000
	#define CLIPEVENT_DRAGOUT_MASK  (1<<23) // 0x00800000
		
	unsigned int    getCLIPEVENTFLAGS();	// FIXME
	unsigned int    getCLIPACTIONRECORD(VObject &clipActionRecordObject);
	int             getCLIPACTIONS(VObject &clipActionsObject);
    //// Filters
    int             getFILTERLIST(Placement* placement);    // SWF8 or later
	
    //// SOUNDINFO
    int             getSOUNDINFO(VObject &soundInfoObject);
    
    
	//// Tag Operations
	int             getTagCodeAndLength(Tag *tag);

private:
    
    //// Internally used Filter getters
    int             getCOLORMATRIXFILTER(VObject &filterObject);
    int             getCONVOLUTIONFILTER(VObject &filterObject);
    int             getBLURFILTER(VObject &filterObject);
    int             getDROPSHADOWFILTER(VObject &filterObject);
    int             getGLOWFILTER(Filter* filter);
    int             getBEVELFILTER(VObject &filterObject);
    int             getGRADIENTGLOWFILTER(VObject &filterObject);
    int             getGRADIENTBEVELFILTER(VObject &filterObject);
};

#endif
