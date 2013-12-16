#include "common.h"
#include "SWFStream.h"
#include "VObject.h"
#include <vector>


#ifndef _TINYSWFPARSER_H
#define _TINYSWFPARSER_H

class Rect {
 public:
  int x_min;
  int x_max;
  int y_min;
  int y_max;
  void Dump() const;
};

class Matrix {
 public:
  Matrix() : scale_x(1.0), scale_y(1.0), rotate_skew0(0), rotate_skew1(0), translate_x(0), translate_y(0) {}
  float scale_x;
  float scale_y;
  float rotate_skew0;
  float rotate_skew1;
  int translate_x;
  int translate_y;
  void Dump() const;
};

class FillStyle {
 public:
  unsigned int type;
  unsigned int rgba;
  Matrix matrix;
  void Dump() const;
};

class LineStyle {
 public:
  unsigned int width;
  unsigned int start_cap_style;
  unsigned int join_style;
  bool has_fill;
  unsigned int no_hscale_flag;
  unsigned int no_vscale_flag;
  unsigned int pixel_hinting_flag;
  unsigned int no_close;
  unsigned int end_cap_style;
  unsigned int miter_limit_factor;
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
  unsigned int fill_style0; // Selectors of FillStyles and LineStyles. Arrays begin at index 1.
  unsigned int fill_style1;
  unsigned int line_style;
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
  unsigned int shape_id;
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

class ParsedSWF {
 public:
  Rect frame_size;
  float frame_rate;
  unsigned int frame_count;
  std::vector<Shape> shapes;
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
    int             getFILTERLIST(VObject &filterlistObject);    // SWF8 or later
	
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
    int             getGLOWFILTER(VObject &filterObject);
    int             getBEVELFILTER(VObject &filterObject);
    int             getGRADIENTGLOWFILTER(VObject &filterObject);
    int             getGRADIENTBEVELFILTER(VObject &filterObject);
};

#endif
