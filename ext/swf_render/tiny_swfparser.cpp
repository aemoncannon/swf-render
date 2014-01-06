#include "tiny_common.h"
#include "tiny_swfparser.h"
#include "tiny_TagDefine.h"
#include "tiny_Util.h"

Color make_rgba(unsigned v) {
  return Color(v & 0xFF,
               (v >> 8) & 0xFF,
               (v >> 16) & 0xFF,
               v >> 24);
}

void Rect::Dump() const {
  printf("(rect xmin=%d xmax=%d ymin=%d ymax=%d)", x_min, x_max, y_min, y_max);
}

void ColorMatrix::Dump() const {
  printf("---------\n%f %f %f %f %f\n%f %f %f %f %f\n%f %f %f %f %f\n%f %f %f %f %f\n",
         m[0],m[1],m[2],m[3],m[4],m[5],m[6],m[7],m[8],m[9],m[10],m[11],m[12],
         m[13],m[14],m[15],m[16],m[17],m[18],m[19]);
}

Color FillStyle::gradient_color(double grad_x, double grad_y) const {
  static const double X1 = -16384;
  static const double Y1 = -16384;
  static const double X2 = 16384;
  static const double Y2 = 16384;
  static const double X3 = -16384;
  static const double Y3 = 16384;
  static const double X4 = 16384;
  static const double Y4 = -16384;
  static const double kGradMin = X1;
  static const double kGradWidth = X2 - X1;
  static const double kGradRadius = X2;
  double pos = 0.0;
  switch (type) {
  case FillStyle::kGradientLinear:
    pos = std::max(0.0, std::min(1.0, (grad_x - kGradMin) / kGradWidth));
    break;
  case FillStyle::kGradientRadial:
    pos = std::max(0.0, std::min(1.0, sqrt((grad_x * grad_x) + (grad_y * grad_y)) / kGradRadius));
    break;
  case FillStyle::kGradientFocal: {
    // See http://stackoverflow.com/questions/1073336/circle-line-collision-detection
    const double r = X2;
    const double x1 = r * focal_point;
    const double y1 = 0;
    const double dy = grad_y - y1;
    const double dx = grad_x - x1;
    const double fx = x1;
    const double fy = y1;
    const float a = dx*dx + dy*dy;
    const float b = 2 * (fx*dx + fy*dy);
    const float c = (fx*fx + fy*fy) - r * r;
    float discriminant = b * b - 4 * a * c;
    if (discriminant < 0) {/*no intersection*/}
    else {
      // ray didn't totally miss sphere,
      // so there is a solution to
      // the equation.
      discriminant = sqrt(discriminant);
      // either solution may be on or off the ray so need to test both
      // t1 is always the smaller value, because BOTH discriminant and
      // a are nonnegative.
      float t1 = (-b - discriminant) / (2 * a);
      float t2 = (-b + discriminant) / (2 * a);
      // 3x HIT cases:
      //          -o->             --|-->  |            |  --|->
      // Impale(t1 hit,t2 hit), Poke(t1 hit,t2>1), ExitWound(t1<0, t2 hit), 
      // 3x MISS cases:
      //       ->  o                     o ->              | -> |
      // FallShort (t1>1,t2>1), Past (t1<0,t2<0), CompletelyInside(t1<0, t2>1)
      if(t1 >= 0) {
        // t1 is the intersection, and it's closer than t2
        // (since t1 uses -b - discriminant)
        // Impale, Poke
        pos = 1.0 / t1;
//        printf("impale, poke\n");
      }
      // here t1 didn't intersect so we are either started
      // inside the sphere or completely past it
      if(t2 >= 0) {
        // ExitWound
        pos = 1.0 / t2;
//        printf("exit\n");
      }
      // no intn: FallShort, Past, CompletelyInside
//      printf("no intn\n");
    }
    break;
  }
  default: break;
  }
  const int len = gradient_entries.size();
  assert(len > 0);
  Color left_color = gradient_entries[0].second;
  double left_pos = 0.0;
  Color right_color = gradient_entries[len - 1].second;
  double right_pos = 1.0;
  for (int i = 0; i < gradient_entries.size(); i++) {
    if (pos < gradient_entries[i].first) {
      right_color = gradient_entries[i].second;
      right_pos = gradient_entries[i].first;
      break;
    }
    left_color = gradient_entries[i].second;
    left_pos = gradient_entries[i].first;
  }
  const double r = (pos - left_pos) / (right_pos - left_pos);
  Color color = left_color.gradient(right_color, r);
  //color.premultiply();
  return color;
}

void FillStyle::Dump() const {
  printf("(fill type=%d rgba=%x matrix=", type, rgba);
  printf("(matrix sx=%f sy=%f r0=%f r1=1%f tx=%f ty=%f)",
         matrix.sx, matrix.sy, matrix.shx, matrix.shy, matrix.tx, matrix.ty);
  printf(")");
}

void LineStyle::Dump() const {
  printf("(linestyle width=%d .... rgba=%x fill=", width, rgba);
  fill.Dump();
  printf(")");
}

void StyleChangeRecord::Dump() const {
  printf("(stylechange ");
  if (HasMoveTo()) {
    printf("moveto=%d,%d ", move_delta_x, move_delta_y);
  }
  if (HasFillStyle0()) {
    printf("fillstyle0=%d ", fill_style0);
  }
  if (HasFillStyle1()) {
    printf("fillstyle1=%d ", fill_style1);
  }
  if (HasLineStyle()) {
    printf("linestyle=%d ", line_style);
  }
  if (HasNewStyles()) {
    printf("fills=(");
    for (int i = 0; i < fill_styles.size(); i++) {
      fill_styles[i].Dump();
    }
    printf(") ");
    printf("linestyles=(");
    for (int i = 0; i < line_styles.size(); i++) {
      line_styles[i].Dump();
    }
    printf(") ");
  }
  printf(")");
}

void EdgeRecord::Dump() const {
  printf("(edge deltax=%d deltay=%d)", delta_x, delta_y);
}

void CurveRecord::Dump() const {
  printf("(curve control_deltax=%d control_deltay=%d anchor_delta_x=%d anchor_delta_y=%d)",
         control_delta_x, control_delta_y, anchor_delta_x, anchor_delta_y);
}

void Shape::Dump() const {
  printf("(shape ");
  printf("shape_bounds=");
  shape_bounds.Dump();
  printf("edge_bounds=");
  edge_bounds.Dump();
  printf("fills=(");
  for (int i = 0; i < fill_styles.size(); i++) {
    fill_styles[i].Dump();
    printf("\n");
  }
  printf(") ");
  printf("linestyles=(");
  for (int i = 0; i < line_styles.size(); i++) {
    line_styles[i].Dump();
    printf("\n");
  }
  printf(") ");
  printf("shape_records=(");
  for (int i = 0; i < records.size(); i++) {
    records[i]->Dump();
    printf("\n");
  }
  printf(") ");
}

void ParsedSWF::Dump() const {
  printf("shapes=(");
  for (int i = 0; i < shapes.size(); i++) {
    shapes[i].Dump();
    printf("\n");
  }
  printf(")");
}

const Sprite* ParsedSWF::SpriteByCharacterId(int character_id) const {
  auto it = character_id_to_sprite_index.find(character_id);
  if (it != character_id_to_sprite_index.end()) {
    const int index = it->second;
    return &sprites.at(index);
  }
  return NULL;
}

const Shape* ParsedSWF::ShapeByCharacterId(int character_id) const {
  auto it = character_id_to_shape_index.find(character_id);
  if (it != character_id_to_shape_index.end()) {
    const int index = it->second;
    return &shapes.at(index);
  }
  return NULL;
}

const Sprite* ParsedSWF::SpriteByClassName(const char* class_name) const {
  std::map<std::string, int>::const_iterator it;
  std::string ending(class_name);
  for (it = class_name_to_character_id.begin();
       it != class_name_to_character_id.end(); ++it) {
    if (it->first.length() >= ending.length()) {
      if (0 == it->first.compare(it->first.length() - ending.length(), ending.length(), ending)) {
        const int character_id = it->second;
        if (const Sprite* sprite = SpriteByCharacterId(character_id)) {
          return sprite;
        }
      }
    }
  }
  return NULL;
}

TinySWFParser::TinySWFParser()
{}


TinySWFParser::~TinySWFParser()
{}

ParsedSWF* TinySWFParser::parse(const char *filename)
{
    assert(filename);
    if (!filename) {
        return NULL;
    }
    if (!open(filename)) {
      printf("Failed to open %s.", filename);
        return NULL;
    }

    ParsedSWF* swf = new ParsedSWF();
    // SWF Header
    getRECT(&swf->frame_size);
    float FrameRate = 0.0;
    FrameRate = getUI16();
    FrameRate = FIXED8TOFLOAT(FrameRate); // Fixed 8.8 ignore the part
    unsigned int FrameCount = 0;
    FrameCount = getUI16();
    swf->frame_rate   = FrameRate;
    swf->frame_count  = FrameCount;

    // Parse TagCodeAndLength (RECORDHEADER)
    unsigned int TagCode = 0, TagLength = 0;
    unsigned int tagNo = 0;

    do {
        Tag tag;
        getTagCodeAndLength(&tag);
        TagCode		= tag.TagCode;

        TagLength	= tag.TagLength;
        switch (TagCode) {
        case TAG_DEFINESHAPE:
        case TAG_DEFINESHAPE2:
        case TAG_DEFINESHAPE3:
        case TAG_DEFINESHAPE4: {
          HandleDefineShape(&tag, swf);
          break;
        }
        case TAG_DEFINESPRITE: {
          HandleDefineSprite(&tag, swf);
          break;
        }
        case TAG_SYMBOLCLASS: {
          HandleSymbolClass(&tag, swf);
          break;
        }
        default: seek(tag.NextTagPos);
        }
        tagNo++;
    } while ( TagCode != 0x0);
    if (getStreamPos() != getFileLength()) {
      printf("Fatal Error, not complete parsing, pos = %d, file length = %d\n", getStreamPos(), getFileLength());
        return NULL;
    }
    return swf;
}

int TinySWFParser::HandleSymbolClass(Tag *tag, ParsedSWF* swf)
{
    unsigned int NumSymbols = getUI16(); // Number of symbols that will be associtated by this tag.
    for (int i = 0; i < NumSymbols; i++) {
      unsigned int TagID = getUI16();
      std::string name(getSTRING());
      swf->class_name_to_character_id[name] = TagID;
    }
    return TRUE;
}

int TinySWFParser::HandleDefineSprite(Tag *tag, ParsedSWF* swf) // 39 = 0x27 (SWF3)
{
  Sprite sprite;
  sprite.character_id = getUI16();
  sprite.frame_count = getUI16();
  int current_frame = 0;
  unsigned int TagCode = 0, TagLength = 0;
  do {
    Tag tag2;
    getTagCodeAndLength(&tag2);
    TagCode = tag2.TagCode;
    TagLength = tag2.TagLength;

    // TODO(aemon): Currently skipping the processing of all frames after 0.
    if (current_frame > 0) {
      seek(tag2.NextTagPos);
      continue;
    }

    switch (TagCode) {
    case TAG_SHOWFRAME: {
      ++current_frame;
      seek(tag2.NextTagPos);
      break;
    }
    case TAG_PLACEOBJECT: {
      assert(false);
      break;
    }
    case TAG_PLACEOBJECT2:
    case TAG_PLACEOBJECT3: {
      HandlePlaceObject23(&tag2, &sprite, current_frame);
      break;
    }
    default: seek(tag2.NextTagPos);
    }
  } while (TagCode != 0x0);
  std::sort(sprite.placements.begin(), sprite.placements.end());
  swf->character_id_to_sprite_index[sprite.character_id] = swf->sprites.size();
  swf->sprites.push_back(sprite);
  return TRUE;
}

void TinySWFParser::HandleDefineShape(Tag* tag, ParsedSWF* swf) {
  Shape shape;
	shape.character_id = getUI16();
	getRECT(&shape.shape_bounds); // ShapeBounds
  if (tag->TagCode == TAG_DEFINESHAPE4) { // DefineShape4 only
    getRECT(&shape.edge_bounds); // ShapeBounds
    getUBits(5); // Reserved. Must be 0
    shape.uses_fill_winding_rule = getUBits(1);
    shape.uses_non_scaling_strokes = getUBits(1);
    shape.uses_scaling_strokes = getUBits(1);
  }
	getSHAPEWITHSTYLE(tag, &shape);
  swf->character_id_to_shape_index[shape.character_id] = swf->shapes.size();
  swf->shapes.push_back(shape);
}

void TinySWFParser::HandlePlaceObject23(Tag* tag, Sprite* sprite, int current_frame) {
  Placement placement;
  //// PlaceObject2 SWF3 or later = 70
//// PlaceObject3 SWF8 or later = 26
  unsigned int PlaceFlagHasClipActions, PlaceFlagHasClipDepth, PlaceFlagHasName, PlaceFlagHasRatio, PlaceFlagHasColorTransform, PlaceFlagHasMatrix, PlaceFlagHasCharacter, PlaceFlagHasMove;
  unsigned int PlaceFlagHasImage, PlaceFlagHasClassName, PlaceFlagHasCacheAsBitmap, PlaceFlagHasBlendMode, PlaceFlagHasFilterList;
  unsigned int Depth, CharacterId, Ratio, ClipDepth;
  setByteAlignment();
  PlaceFlagHasClipActions                = getUBits(1); // SWF5 and later (sprite characters only)
  PlaceFlagHasClipDepth                = getUBits(1);
  PlaceFlagHasName                        = getUBits(1);
  PlaceFlagHasRatio                        = getUBits(1);
  PlaceFlagHasColorTransform        = getUBits(1);
  PlaceFlagHasMatrix                        = getUBits(1);
  PlaceFlagHasCharacter                = getUBits(1);
  PlaceFlagHasMove                        = getUBits(1);

    if (tag->TagCode == TAG_PLACEOBJECT3) {   // PlaceObject3 only
        getUBits(3);    // Reserved, must be 0
        PlaceFlagHasImage = getUBits(1);
        PlaceFlagHasClassName = getUBits(1);
        PlaceFlagHasCacheAsBitmap = getUBits(1);
        PlaceFlagHasBlendMode = getUBits(1);
        PlaceFlagHasFilterList = getUBits(1);
    }
    placement.depth = getUI16();
    if (tag->TagCode == TAG_PLACEOBJECT3) {   // PlaceObject3 only
        // ClassName : String
        if (PlaceFlagHasClassName || (PlaceFlagHasImage & PlaceFlagHasCharacter)) {
            const char* class_name = getSTRING();
        }
    }
        if (PlaceFlagHasCharacter) {
          placement.character_id = getUI16();
        }
        if (PlaceFlagHasMatrix) {
          getMATRIX(&placement.matrix); // Transform matrix data
        }
        if (PlaceFlagHasColorTransform) {
          getCXFORMWITHALPHA();
        }
        if (PlaceFlagHasRatio) {
          Ratio = getUI16();
        }
        if (PlaceFlagHasName) {
          const char* name = getSTRING();
        }
        if (PlaceFlagHasClipDepth) {
          printf("Warning: unhandled clipping mask.\n");
          ClipDepth = getUI16();
        }
    if (tag->TagCode == TAG_PLACEOBJECT3) {   // PlaceObject3 only
        if (PlaceFlagHasFilterList) {
          getFILTERLIST(&placement);
        }
        if (PlaceFlagHasBlendMode) {
          printf("Warning: unhandled blend mode.\n");
            unsigned int BlendMode;
            BlendMode = getUI8();
        }
        if (PlaceFlagHasCacheAsBitmap) {
            unsigned int BitmapCache;
            BitmapCache = getUI8();
        }
    }
        if (PlaceFlagHasClipActions) {
          assert(false);
//          getCLIPACTIONS(tagObject["ClipActions"]);
        }
    if (tag->TagCode == TAG_PLACEOBJECT3) {
        if (tag->NextTagPos == (getStreamPos() + 1))
            getUI8(); // FIXME: wierd padding? not sure what's wrong.
    }

  if (PlaceFlagHasMove) {
    printf("Oops. Placeobject has move. See SWF format p.35\n");
  } else {
    sprite->placements.push_back(placement);
  }
}

///////////////////////////////////////
//// Color Operations
///////////////////////////////////////
unsigned int TinySWFParser::getRGB()
{
    unsigned int value = 0;

    setByteAlignment();
    value = getUI8();                   // R
    value = value | (getUI8() << 8);    // G
    value = value | (getUI8() << 16);   // B
    // value = 0xBBGGRR
    return value;
}

///////////////////////////////////////
//// Gradient Operation
///////////////////////////////////////
int TinySWFParser::getGRADIENT(Tag *tag, FillStyle* style)
{
	unsigned int SpreadMode, InterpolationMode, NumGradients, i;
	setByteAlignment();
	SpreadMode = getUBits(2);           // 0 = Pad mode, 1 = Reflect mode, 2 = Repeat mode, 3 = Reserved
	InterpolationMode = getUBits(2);    // 0 = Nomral RGB mode, 1 = Linear RGB mode, 2 and 3 = Reserved
	NumGradients = getUBits(4); // 1 to 15
  if (NumGradients) {
        for (i = 0; i < NumGradients; i++) {
            // GRADRECORD
            unsigned int Ratio, EntryColor;
            Ratio = getUI8();
            // gradientRecord["Ratio"] = Ratio;
            if ((tag->TagCode == TAG_DEFINESHAPE) || (tag->TagCode == TAG_DEFINESHAPE2)) {
                EntryColor = (0xFF << 24) | getRGB(); // for DefineShape and DefineShape2
                // gradientRecord["Color"] = Color2String(Color, 0);
            } else {
                EntryColor = getRGBA(); // for DefineShape3 or later?
                // gradientRecord["Color"] = Color2String(Color, 1);
            }
            const float r = (float)Ratio / 255.0;
            style->gradient_entries.push_back(
                std::pair<float, Color>(r, make_rgba(EntryColor)));
        }
    }
	return TRUE;
}

int TinySWFParser::getFOCALGRADIENT(Tag *tag, FillStyle* style)
{
  getGRADIENT(tag, style);
  style->focal_point = getFIXED8(); // Focal point location, -1.0 to 1.0
	return TRUE;
}

///////////////////////////////////////
//// Rectangle Operations
///////////////////////////////////////
int TinySWFParser::getRECT(Rect* rect)
{
	unsigned int Nbits = 0;
	signed int Xmin, Xmax, Ymin, Ymax;
	setByteAlignment();
	Nbits	= getUBits(5);
	Xmin	= getSBits(Nbits);
	Xmax	= getSBits(Nbits);
	Ymin	= getSBits(Nbits);
	Ymax	= getSBits(Nbits);
	rect->x_min = Xmin;
	rect->x_max = Xmax;
	rect->y_min = Ymin;
	rect->y_max = Ymax;
	return TRUE;
}

///////////////////////////////////////
//// Matrix Operation
///////////////////////////////////////
int TinySWFParser::getMATRIX(Matrix* matrix)
{
	unsigned int HasScale, NScaleBits, HasRotate, NRotateBits, NTranslateBits;
	float ScaleX, ScaleY, RotateSkew0, RotateSkew1; // FIXME : These should be FIXED values
	signed int TranslateX, TranslateY;
	setByteAlignment(); // MATRIX Record must be byte aligned.
	HasScale = getUBits(1);
  double sx(1.0);
  double sy(1.0);
  double shx(0.0);
  double shy(0.0);
  double tx(0.0);
  double ty(0.0);
	if (HasScale) {
		NScaleBits = getUBits(5);
		sx = FIXED2FLOAT(getSBits(NScaleBits));
		sy = FIXED2FLOAT(getSBits(NScaleBits));
	}
	HasRotate = getUBits(1);
	if (HasRotate) {
		NRotateBits = getUBits(5);
    // TODO(aemon): double check order here
		shy = FIXED2FLOAT(getSBits(NRotateBits));
		shx = FIXED2FLOAT(getSBits(NRotateBits));
	}
	NTranslateBits = getUBits(5);
	tx = getSBits(NTranslateBits);
	ty = getSBits(NTranslateBits);
  *matrix = Matrix(sx, shy, shx, sy, tx, ty);
	return TRUE;
}

///////////////////////////////////////
//// Color Transformation
///////////////////////////////////////
int TinySWFParser::getCXFORM()
{
	unsigned int HasAddTerms, HasMultTerms, Nbits;
	signed int RedMultTerm, GreenMultTerm, BlueMultTerm, RedAddTerm, GreenAddTerm, BlueAddTerm;
	setByteAlignment(); // CXFORM Record must be byte aligned.
	HasAddTerms		= getUBits(1);
	HasMultTerms	= getUBits(1);
	Nbits			= getUBits(4);
	if (HasMultTerms) {
		RedMultTerm		= getSBits(Nbits);  // in swf file, it's still a SB
    GreenMultTerm	= getSBits(Nbits);
		BlueMultTerm	= getSBits(Nbits);
	}
	if (HasAddTerms) {
		RedAddTerm		= getSBits(Nbits);
    GreenAddTerm	= getSBits(Nbits);
		BlueAddTerm		= getSBits(Nbits);
	}
	return TRUE;
}

///////////////////////////////////////
//// Fill Style & Line Style
///////////////////////////////////////
//// FillStyleType :
//// 0x00 = solid fill
//// 0x10 = linear gradient fill
//// 0x12 = radial gradient fill
//// 0x13 = focal radial gradient fill (SWF8 or later)
//// 0x40 = repeating bitmap fill
//// 0x41 = clipped bitmap fill
//// 0x42 = non-smoothed repeating bitmap
//// 0x43 = non-smoothed clipped bitmap
int TinySWFParser::getFILLSTYLE(Tag *tag, FillStyle* style)
{
    // FILLSTYLE
    unsigned int FillStyleType, Color;
    FillStyleType = getUI8();
    style->type = static_cast<FillStyle::Type>(FillStyleType);
    if (FillStyleType == 0x00) { // Solid Color Fill
        if ((tag->TagCode == TAG_DEFINESHAPE) || (tag->TagCode == TAG_DEFINESHAPE2)) {
          style->rgba = (0xFF << 24) | getRGB();   // DefineShape/DefineShape2
        } else {
          style->rgba = getRGBA();  // DefineShape3 or 4?
        }
    }
    if ((FillStyleType == 0x10) ||
        (FillStyleType == 0x12) ||
        (FillStyleType == 0x13)) { // Gradient Fill
        getMATRIX(&style->matrix);
        if ((FillStyleType == 0x10) ||
            (FillStyleType == 0x12)) {
            getGRADIENT(tag, style);
        } else { // 0x13
            getFOCALGRADIENT(tag, style); // SWF8 or later
        }
    }
    if ((FillStyleType == 0x40) ||
        (FillStyleType == 0x41) ||
        (FillStyleType == 0x42) ||
        (FillStyleType == 0x43)) { // Bitmap Fill
        unsigned int BitmapId = getUI16();
        Matrix matrix;
        getMATRIX(&style->matrix); // Matrix for bitmap fill
    }
    return TRUE;
}

int TinySWFParser::getFILLSTYLEARRAY(Tag *tag, std::vector<FillStyle>* styles)
{
	unsigned int FillStyleCount, i;
	FillStyleCount = getUI8();
	if (FillStyleCount == 0xff)
		FillStyleCount = getUI16();
    if (FillStyleCount) {
        for (i = 0; i < FillStyleCount; i++) {
          FillStyle style;
          getFILLSTYLE(tag, &style);
          styles->push_back(style);
        }
    }
	return TRUE;
}



int TinySWFParser::getLINESTYLEARRAY(Tag *tag, std::vector<LineStyle>* styles)
{

	unsigned int LineStyleCount, i;
	LineStyleCount = getUI8();
	if (LineStyleCount == 0xff) LineStyleCount = getUI16();
  if (!LineStyleCount) {
    return TRUE;
  }
	// LineStyles
    // DefineShape1/2/3 => LINESTYLE[]
    // DefineShape4     => LINESTYLE2[]
    if (tag->TagCode == TAG_DEFINESHAPE4) {   // DefineShape4 => LINESTYLE2[]

        for (i = 0; i < LineStyleCount; i++) { // Index start from 1
            LineStyle style;
            // LINESTYLE2
            unsigned int Width, StartCapStyle, JoinStyle, HasFillFlag, NoHScaleFlag, NoVScaleFlag, PixelHintingFlag, NoClose, EndCapStyle, MiterLimitFactor, Color;
            style.width           = getUI16();

            style.start_cap_style   = static_cast<LineStyle::CapStyle>(getUBits(2)); // 0 = Round cap, 1 = No cap, 2 = Square cap
            style.join_style       = static_cast<LineStyle::JoinStyle>(getUBits(2)); // 0 = Round join, 1 = Bevel join, 2 = Miter join
            style.has_fill     = getUBits(1);
            style.no_hscale_flag    = getUBits(1);
            style.no_vscale_flag    = getUBits(1);
            style.pixel_hinting_flag    = getUBits(1);
            getUBits(5); // Reserved must be 0
            style.no_close         = getUBits(1);
            style.end_cap_style     = static_cast<LineStyle::CapStyle>(getUBits(2));
            if (style.join_style == LineStyle::kJoinMiter) {
                style.miter_limit_factor = getFIXED8();   // Miter limit factor is an 8.8 fixed-point value.
            }
            if (!style.has_fill) {
                style.rgba = getRGBA();         // Color
            } else {
                DEBUGMSG(",\nFillType : ");
                getFILLSTYLE(tag, &style.fill);  // FillType : FILLSTYLE
            }
            styles->push_back(style);
        }

    } else { // DefineShape1/2/3 => LINESTYLE[]
        for (i = 0; i < LineStyleCount; i++) {
            LineStyle style;
            unsigned int Width, Color = 0;
            style.width = getUI16();
            if (tag->TagCode == TAG_DEFINESHAPE3) { // DefineShape3
                style.rgba = getRGBA();
            } else {
                style.rgba = (0xFF << 24) | getRGB();   // DefineShape1/2
            }
            styles->push_back(style);
        }
	}
	return TRUE;
}


///////////////////////////////////////
//// Shape Operations
///////////////////////////////////////

int TinySWFParser::getSHAPE(Tag *tag, Shape* shape)
{
	unsigned int NumFillBits = 0, NumLineBits = 0;// ShapeRecordNo = 0;
	setByteAlignment(); // reset bit buffer for byte-alignment
	NumFillBits = getUBits(4); // NumFillBits
	NumLineBits = getUBits(4); // NumLineBits
    DEBUGMSG(",\nShapeRecords : [\n");
  unsigned int recNo = 0;

	while(1) {
    ASSERT (getStreamPos() > tag->NextTagPos);
		unsigned int TypeFlag = getUBits(1); // TypeFlag => 0 : Non-edge, 1: Edge Reocrds
    if (!TypeFlag) { // Non-edge Records where TypeFlag == 0
			unsigned int Flags = getUBits(5);
			if (Flags == 0) { // ENDSHAPERECORD
				return TRUE;
			} else { // STYLECHANGERECORD
       StyleChangeRecord* record = new StyleChangeRecord();
       shape->records.push_back(record);
				unsigned int StateNewStyles, StateLineStyle, StateFillStyle1, StateFillStyle0, StateMoveTo;
				signed int MoveDeltaX, MoveDeltaY;
				unsigned int FillStyle0, FillStyle1, LineStyle; // Selector of FillStyles and LineStyles. Arrays begin at index 1.
				StateNewStyles  = (Flags >> 4) & 1;
				StateLineStyle  = (Flags >> 3) & 1;
				StateFillStyle1 = (Flags >> 2) & 1;
				StateFillStyle0 = (Flags >> 1) & 1;
				StateMoveTo = Flags & 1;
       record->flags = Flags;
				if (StateMoveTo) {
					unsigned int MoveBits = getUBits(5);
					record->move_delta_x = getSBits(MoveBits);
					record->move_delta_y = getSBits(MoveBits);
				}
				if (StateFillStyle0) {
          // Note: convert from 1-indexed to 0-indexed
					record->fill_style0 = getUBits(NumFillBits) - 1;
				}
				if (StateFillStyle1) {
          // Note: convert from 1-indexed to 0-indexed
					record->fill_style1 = getUBits(NumFillBits) - 1;
				}
				if (StateLineStyle) {
          // Note: convert from 1-indexed to 0-indexed
					record->line_style = getUBits(NumLineBits) - 1;
				}
				if (StateNewStyles) {
         getFILLSTYLEARRAY(tag, &record->fill_styles);
					getLINESTYLEARRAY(tag, &record->line_styles);
         NumFillBits = getUBits(4);
					NumLineBits = getUBits(4);
				}
			}
		} else { // Edge Records where TypeFlag == 1
			unsigned int StraightFlag, NumBits;
			StraightFlag = getUBits(1);
			NumBits = getUBits(4);
			if (StraightFlag) { // STRAIGHTEDGERECORD
        EdgeRecord* record = new EdgeRecord();
        shape->records.push_back(record);
				unsigned int GeneralLineFlag = getUBits(1);
				signed int VertLineFlag, DeltaX, DeltaY;
				if (GeneralLineFlag) { // General Line
					record->delta_x = getSBits(NumBits + 2);
					record->delta_y = getSBits(NumBits + 2);
				} else { // Vert/Horz Line
					VertLineFlag = getUBits(1);
					if (VertLineFlag) {
						record->delta_y = getSBits(NumBits + 2);
						record->delta_x = 0;
					} else {
						record->delta_x = getSBits(NumBits + 2);
						record->delta_y = 0;
					}
				}
			} else { // CURVEDEDGERECORD
       CurveRecord* record = new CurveRecord();
       shape->records.push_back(record);
				record->control_delta_x	= getSBits(NumBits + 2);
				record->control_delta_y	= getSBits(NumBits + 2);
				record->anchor_delta_x	= getSBits(NumBits + 2);
				record->anchor_delta_y	= getSBits(NumBits + 2);
			}

		}
    recNo++;
	} // while
}

int TinySWFParser::getSHAPEWITHSTYLE(Tag *tag, Shape* shape)
{
	getFILLSTYLEARRAY(tag, &shape->fill_styles);
	getLINESTYLEARRAY(tag, &shape->line_styles);
	getSHAPE(tag, shape);
	return TRUE;
}

///////////////////////////////////////
//// Tag Operation
///////////////////////////////////////
int TinySWFParser::getTagCodeAndLength(Tag *tag)
{
	unsigned int TagCode, TagLength;
    
    tag->TagHeaderOffset = getStreamPos();
    
	TagCode = getUI16();
	TagLength = TagCode & 0x3f; /* Lower 6 bits */
	
	if (TagLength == 0x3f) { /* RECORDHEADER(long) */
		TagLength = getUI32();
	}
	
	TagCode = TagCode >> 6; /* Upper 10 bits */
    
    //DEBUGMSG("TagCode : %d, \nTagLength : %d\n", TagCode, TagLength);
	tag->TagCode    = TagCode;
	tag->TagLength  = TagLength;
    
    tag->TagBodyOffset = getStreamPos();
    tag->TagHeaderLength = tag->TagBodyOffset - tag->TagHeaderOffset;
    tag->NextTagPos = tag->TagBodyOffset + TagLength; // Actually it points to the next tag
	
	return TRUE;
}

int TinySWFParser::getGLOWFILTER(Filter* filter)
{
  unsigned int InnerShadow, Knockout, CompositeSource, Passes;
  float BlurX, BlurY, Strength;
  filter->rgba = getRGBA();
  BlurX = getFIXED();
  BlurY = getFIXED();
  Strength = getFIXED8();
  InnerShadow = getUBits(1);
  Knockout = getUBits(1);
  CompositeSource = getUBits(1);
  Passes = getUBits(5);
  return TRUE;
}

int TinySWFParser::getBLURFILTER(Filter* filter)
{
  printf("Warning: unhandled blur.\n");
    float BlurX, BlurY;
    BlurX = getFIXED();
    BlurY = getFIXED();
    unsigned int Passes;
    Passes = getUBits(5);
    ASSERT(getUBits(3));    // Reserved, must be 0
    return TRUE;
}

int TinySWFParser::getCOLORMATRIXFILTER(Filter* filter)
{
  for (int i = 0; i < 20; i++) {
    filter->color_matrix.m[i] = getFLOAT();
  }
  return TRUE;
}

///////////////////////////////////////////////////////////
// filter operations above is used by getFILTERLIST() only.
///////////////////////////////////////////////////////////
int TinySWFParser::getFILTERLIST(Placement* placement)    // SWF8 or later
{
    unsigned int NumberOfFilters = getUI8();
    if (NumberOfFilters > 0) {
        for (int i = 0; i < NumberOfFilters; i++) {
          Filter filter;
          filter.filter_type = static_cast<Filter::FilterType>(getUI8());
            switch (filter.filter_type) {
                case Filter::kFilterDropShadow:
                  assert(false);
                  //getDROPSHADOWFILTER(filter["DropShadowFilter"]);
                    break;
                case Filter::kFilterBlur:
                  getBLURFILTER(&filter);
                  break;
                case Filter::kFilterGlow:
                    getGLOWFILTER(&filter);
                    break;
                case Filter::kFilterBevel:
                  assert(false);
                  //getBEVELFILTER(filter["BevelFilter"]);
                    break;
                case Filter::kFilterGradientGlow:
                  assert(false);
                  //getGRADIENTGLOWFILTER(filter["GradientGlowFilter"]);
                    break;
                case Filter::kFilterConvolution:
                  assert(false);
                  //getCONVOLUTIONFILTER(filter["ConvolutionFilter"]);
                    break;
                case Filter::kFilterColorMatrix:
                  getCOLORMATRIXFILTER(&filter);
                  break;
                case Filter::kFilterGradientBevel:
                  assert(false);
                  //getGRADIENTBEVELFILTER(filter["GradientBevelFilter"]);
                    break;
                default:
                    printf("Undefined FilterID(%d) - Fatal Error", filter.filter_type); // Assert
                    assert(false);
                    return FALSE;
            } // switch
            placement->filters.push_back(filter);
        } // for
    }
    return TRUE;
}

int TinySWFParser::getCXFORMWITHALPHA()
{
  printf("Warning: unhandled color transform with alpha.\n");
	unsigned int HasAddTerms, HasMultTerms, Nbits;
	signed int RedMultTerm, GreenMultTerm, BlueMultTerm, AlphaMultTerm, RedAddTerm, GreenAddTerm, BlueAddTerm, AlphaAddTerm;
	setByteAlignment(); // CXFORM Record must be byte aligned.
	HasAddTerms		= getUBits(1);
	HasMultTerms	= getUBits(1);
	Nbits = getUBits(4);
	if (HasMultTerms) {
		RedMultTerm		= getSBits(Nbits);
    GreenMultTerm	= getSBits(Nbits);
		BlueMultTerm	= getSBits(Nbits);
		AlphaMultTerm	= getSBits(Nbits);
    FIXED8TOFLOAT(RedMultTerm);
    FIXED8TOFLOAT(GreenMultTerm);
    FIXED8TOFLOAT(BlueMultTerm);
    FIXED8TOFLOAT(AlphaMultTerm);
	}
	if (HasAddTerms) {
		RedAddTerm		= getSBits(Nbits);
    GreenAddTerm	= getSBits(Nbits);
		BlueAddTerm		= getSBits(Nbits);
		AlphaAddTerm	= getSBits(Nbits);
	}
	return TRUE;
}
