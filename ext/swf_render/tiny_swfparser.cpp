#include "tiny_common.h"
#include "tiny_swfparser.h"
#include "tiny_TagDefine.h"
#include "tiny_Util.h"


namespace {

void HandleDefineShape4(Tag* tag, TinySWFParser* parser, ParsedSWF* swf) {
  Shape shape;
	shape.shape_id = parser->getUI16();
	parser->getRECT(&shape.shape_bounds); // ShapeBounds
  if (tag->TagCode == TAG_DEFINESHAPE4) { // DefineShape4 only
    parser->getRECT(&shape.edge_bounds); // ShapeBounds
    parser->getUBits(5); // Reserved. Must be 0
    int UsesFillWindingRule, UsesNonScalingStrokes, UsesScalingStrokes;
    shape.uses_fill_winding_rule = parser->getUBits(1);
    shape.uses_non_scaling_strokes = parser->getUBits(1);
    shape.uses_scaling_strokes = parser->getUBits(1);
  }
	parser->getSHAPEWITHSTYLE(tag, &shape);
  swf->shapes.push_back(shape);
}

}  // namespace

void Rect::Dump() const {
  printf("(rect xmin=%d xmax=%d ymin=%d ymax=%d)", x_min, x_max, y_min, y_max);
}

void Matrix::Dump() const {
  printf("(matrix sx=%f sy=%f r0=%f r1=1%f tx=%d ty=%d)",
         scale_x, scale_y, rotate_skew0, rotate_skew1, translate_x, translate_y);
}

void FillStyle::Dump() const {
  printf("(fill type=%d rgba=%d matrix=", type, rgba);
  matrix.Dump();
  printf(")");
}

void LineStyle::Dump() const {
  printf("(linestyle width=%d .... rgba=%d fill=", width, rgba);
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

TinySWFParser::TinySWFParser()
{}


TinySWFParser::~TinySWFParser()
{}

ParsedSWF* TinySWFParser::parse(const char *filename)
{
    return parseWithCallback(filename, NULL);
}

ParsedSWF* TinySWFParser::parseWithCallback(const char *filename, ProgressUpdateFunctionPtr progressUpdate)
{
    if (!filename) {
        return NULL;
    }
    if (!open(filename)) {
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

    DEBUGMSG(",\nTags : [\n");
    do {
        Tag tag;
        getTagCodeAndLength(&tag);
        TagCode		= tag.TagCode;
        TagLength	= tag.TagLength;
        if (progressUpdate) {
            if (!progressUpdate((tag.TagBodyOffset * 100) / FileLength)) {
                // the caller want to stop the parsing.
                break;
            }
        }
        switch (TagCode) {
        case TAG_DEFINESHAPE4: {
          HandleDefineShape4(&tag, this, swf);
          break;
        }
        default: seek(tag.NextTagPos);
        }
        tagNo++;
    } while ( TagCode != 0x0); /* Parse untile END Tag(0x0) */
    DEBUGMSG("]\n}\n"); // End of SWFStream
	if (getStreamPos() != getFileLength()) {
		DEBUGMSG("Fatal Error, not complete parsing, pos = %d, file length = %d\n", getStreamPos(), getFileLength());
        return NULL;
    }
    return swf;
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
  // gradientObject["SpreadMode"] = SpreadMode;
  // gradientObject["InterpolationMode"] = InterpolationMode;
  // gradientObject["NumGradients"] = NumGradients;
  if (NumGradients) {
        for (i = 0; i < NumGradients; i++) {
            // GRADRECORD
            unsigned int Ratio, Color;
            Ratio = getUI8();
            // gradientRecord["Ratio"] = Ratio;
            if ((tag->TagCode == TAG_DEFINESHAPE) || (tag->TagCode == TAG_DEFINESHAPE2)) {
                Color = getRGB(); // for DefineShape and DefineShape2
                // gradientRecord["Color"] = Color2String(Color, 0);
            } else {
                Color = getRGBA(); // for DefineShape3 or later?
                // gradientRecord["Color"] = Color2String(Color, 1);
            }
        }
    }
	return TRUE;
}

int TinySWFParser::getFOCALGRADIENT(Tag *tag, FillStyle* style)
{
    getGRADIENT(tag, style);
    float FocalPoint;
    FocalPoint = getFIXED8();    // Focal point location
    //focalObject["FocalPoint"] = FocalPoint;
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
	if (HasScale) {
		NScaleBits = getUBits(5);
		matrix->scale_x = FIXED2FLOAT(getSBits(NScaleBits));
		matrix->scale_y = FIXED2FLOAT(getSBits(NScaleBits));
	}
	HasRotate = getUBits(1);
	if (HasRotate) {
		NRotateBits = getUBits(5);
		matrix->rotate_skew0 = FIXED2FLOAT(getSBits(NRotateBits));
		matrix->rotate_skew1 = FIXED2FLOAT(getSBits(NRotateBits));
	}
	NTranslateBits = getUBits(5);
	matrix->translate_x = getSBits(NTranslateBits);
	matrix->translate_y = getSBits(NTranslateBits);
	return TRUE;
}

///////////////////////////////////////
//// Color Transformation
///////////////////////////////////////
int TinySWFParser::getCXFORM(VObject &cxObject)
{
	unsigned int HasAddTerms, HasMultTerms, Nbits;
	signed int RedMultTerm, GreenMultTerm, BlueMultTerm, RedAddTerm, GreenAddTerm, BlueAddTerm;
	
	setByteAlignment(); // CXFORM Record must be byte aligned.
	
	DEBUGMSG("{\n");
	cxObject.setTypeInfo("CXFORM");
    
	HasAddTerms		= getUBits(1);
	HasMultTerms	= getUBits(1);
	Nbits			= getUBits(4);
    
    cxObject["HasAddTerms"]     = HasAddTerms;
    cxObject["HasMultTerms"]    = HasMultTerms;
    cxObject["Nbits"]           = Nbits;
	DEBUGMSG("HasAddTerms : %d,\nHasMultTerms : %d,\nNbits : %d", Nbits, HasAddTerms, HasMultTerms);
	
	if (HasMultTerms) {
		RedMultTerm		= getSBits(Nbits);  // in swf file, it's still a SB
        GreenMultTerm	= getSBits(Nbits);
		BlueMultTerm	= getSBits(Nbits);
        cxObject["RedMultTerm"]     = FIXED8TOFLOAT(RedMultTerm);   // convert it to FIXED8 to be meaningful for the user.
        cxObject["GreenMultTerm"]   = FIXED8TOFLOAT(GreenMultTerm);
        cxObject["BlueMultTerm"]    = FIXED8TOFLOAT(BlueMultTerm);
		DEBUGMSG(",\nRedMultTerm : %f,\nGreenMultTerm : %f,\nBlueMultTerm : %f", RedMultTerm, GreenMultTerm, BlueMultTerm);
	}
	
	if (HasAddTerms) {
		RedAddTerm		= getSBits(Nbits);
        GreenAddTerm	= getSBits(Nbits);
		BlueAddTerm		= getSBits(Nbits);
        cxObject["RedAddTerm"] = RedAddTerm;
        cxObject["GreenAddTerm"] = GreenAddTerm;
        cxObject["BlueAddTerm"] = BlueAddTerm;
		DEBUGMSG(",\nRedAddTerm : %d,\nGreenAddTerm : %d,\nBlueAddTerm : %d", RedAddTerm, GreenAddTerm, BlueAddTerm);
	}
	DEBUGMSG("\n}");
	
	return TRUE;
}

int TinySWFParser::getCXFORMWITHALPHA(VObject &cxObject)
{
	unsigned int HasAddTerms, HasMultTerms, Nbits;
	signed int RedMultTerm, GreenMultTerm, BlueMultTerm, AlphaMultTerm, RedAddTerm, GreenAddTerm, BlueAddTerm, AlphaAddTerm;
	
	setByteAlignment(); // CXFORM Record must be byte aligned.

	DEBUGMSG("{\n");
	cxObject.setTypeInfo("CXFORMWITHALPHA");
    
	HasAddTerms		= getUBits(1);
	HasMultTerms	= getUBits(1);
	Nbits = getUBits(4);

    cxObject["HasAddTerms"]     = HasAddTerms;
    cxObject["HasMultTerms"]    = HasMultTerms;
    cxObject["Nbits"]           = Nbits;
    
	DEBUGMSG("HasAddTerms : %d,\nHasMultTerms : %d,\nNbits : %d", HasAddTerms, HasMultTerms, Nbits);
	
	if (HasMultTerms) {
		RedMultTerm		= getSBits(Nbits);
        GreenMultTerm	= getSBits(Nbits);
		BlueMultTerm	= getSBits(Nbits);
		AlphaMultTerm	= getSBits(Nbits);
        
        cxObject["RedMultTerm"]     = FIXED8TOFLOAT(RedMultTerm);
        cxObject["GreenMultTerm"]   = FIXED8TOFLOAT(GreenMultTerm);
        cxObject["BlueMultTerm"]    = FIXED8TOFLOAT(BlueMultTerm);
        cxObject["AlphaMultTerm"]   = FIXED8TOFLOAT(AlphaMultTerm);
        
        DEBUGMSG(",\nRedMultTerm : %d,\nGreenMultTerm : %d,\nBlueMultTerm : %d,\nAlphaMultTerm : %d", RedMultTerm, GreenMultTerm, BlueMultTerm, AlphaMultTerm);
	}
	
	if (HasAddTerms) {
		RedAddTerm		= getSBits(Nbits);
        GreenAddTerm	= getSBits(Nbits);
		BlueAddTerm		= getSBits(Nbits);
		AlphaAddTerm	= getSBits(Nbits);

        cxObject["RedAddTerm"] = RedAddTerm;
        cxObject["GreenAddTerm"] = GreenAddTerm;
        cxObject["BlueAddTerm"] = BlueAddTerm;
        cxObject["AlphaAddTerm"] = AlphaAddTerm;
        
        DEBUGMSG(",\nRedAddTerm : %d,\nGreenAddTerm : %d,\nBlueAddTerm : %d,\nAlphaAddTerm : %d", RedAddTerm, GreenAddTerm, BlueAddTerm, AlphaAddTerm);
	}
	
    DEBUGMSG("\n}");
	
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
            style->rgba = getRGB();   // DefineShape/DefineShape2
        } else {
            style->rgba = getRGBA();  // DefineShape3 or 4?
        }
    }
    if ((FillStyleType == 0x10) ||
        (FillStyleType == 0x12) ||
        (FillStyleType == 0x13)) { // Gradient Fill
        getMATRIX(&style->matrix); // GradientMatrix
        if ((FillStyleType == 0x10) ||
            (FillStyleType == 0x12)) {
            getGRADIENT(tag, style);
        } else { // 0x13
            getFOCALGRADIENT(tag, style); // SWF8 or later
        }
        //DEBUGMSG("} // Gradient Fill\n");
    }
    if ((FillStyleType == 0x40) ||
        (FillStyleType == 0x41) ||
        (FillStyleType == 0x42) ||
        (FillStyleType == 0x43)) { // Bitmap Fill
        unsigned int BitmapId = getUI16();
        // TODO(aemon): Save bitmap id and matrix.
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
            style.start_cap_style   = getUBits(2); // 0 = Round cap, 1 = No cap, 2 = Square cap
            style.join_style       = getUBits(2); // 0 = Round join, 1 = Bevel join, 2 = Miter join
            style.has_fill     = getUBits(1);
            style.no_hscale_flag    = getUBits(1);
            style.no_vscale_flag    = getUBits(1);
            style.pixel_hinting_flag    = getUBits(1);
            getUBits(5); // Reserved must be 0
            style.no_close         = getUBits(1);
            style.end_cap_style     = getUBits(2);
            if (JoinStyle == 2) {
                style.miter_limit_factor = getUI16();   // Miter limit factor is an 8.8 fixed-point value.
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
                style.rgba = getRGB();   // DefineShape1/2
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
