#ifndef _DISPLAYTREE_H
#define _DISPLAYTREE_H

#include "tiny_swfparser.h"

class renderer_base;
class renderer_scanline;

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

  Placement* placement;
  Shape* shape;
  Matrix matrix;
  std::vector<DisplayTree*> children;
  std::vector<Filter> filters;
  std::string name;
  bool visible;
};

#endif
