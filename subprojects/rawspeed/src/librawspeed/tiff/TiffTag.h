// Authors:
//     Larry Ewing <lewing@novell.com>
//
//
// Copyright (C) 2004 - 2006 Novell, Inc (http://www.novell.com)
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

namespace rawspeed {

enum TiffTag {
  INTEROPERABILITYINDEX  = 0x0001,
  INTEROPERABILITYVERSION         = 0x0002,
  CANONSHOTINFO = 0x0004,
  CANONPOWERSHOTG9WB = 0x0029,
  PANASONIC_ISO_SPEED = 23,
  NEWSUBFILETYPE                  = 0x00FE,
  SUBFILETYPE                     = 0x00FF,
  PANASONIC_BITSPERSAMPLE         = 0xa,
  PANASONIC_RAWFORMAT             = 0x2d,
  MAKERNOTE_ALT                   = 0x2e,

  IMAGEWIDTH    = 0x0100,
  IMAGELENGTH    = 0x0101,
  BITSPERSAMPLE            = 0x0102,
  COMPRESSION    = 0x0103,
  PHOTOMETRICINTERPRETATION  = 0x0106,
  FILLORDER    = 0x010A,
  DOCUMENTNAME    = 0x010D,
  IMAGEDESCRIPTION   = 0x010E,
  MAKE     = 0x010F,
  MODEL     = 0x0110,
  STRIPOFFSETS    = 0x0111,
  ORIENTATION    = 0x0112,
  SAMPLESPERPIXEL   = 0x0115,
  ROWSPERSTRIP      = 0x0116,
  STRIPBYTECOUNTS   = 0x0117,
  PANASONIC_STRIPOFFSET = 0x118,
  XRESOLUTION    = 0x011A,
  YRESOLUTION    = 0x011B,
  PLANARCONFIGURATION   = 0x011C,

  GRAYRESPONSECURVE = 0x0123,

  T4OPTIONS                       = 0x0124,
  T6OPTIONS                       = 0x0125,

  RESOLUTIONUNIT    = 0x0128,
  TRANSFERFUNCTION   = 0x012D,
  FUJI_LAYOUT = 0x0130,
  SOFTWARE    = 0x0131,
  DATETIME   = 0x0132,
  ARTIST    = 0x013B,
  PREDICTOR                       = 0x013D,
  WHITEPOINT   = 0x013E,
  PRIMARYCHROMATICITIES  = 0x013F,

  HALFTONEHINTS                   = 0x0141,
  // TILED IMAGES
  TILEWIDTH                       = 0x0142,
  TILELENGTH                      = 0x0143,
  TILEOFFSETS                     = 0x0144,
  TILEBYTECOUNTS                  = 0x0145,

  SUBIFDS                         = 0x014A, // TIFF-EP

  // CMYK IMAGES
  INKSET                          = 0x014C,
  NUMBEROFINKS                    = 0x014E,
  INKNAMES                        = 0x014D,
  DOTRANGE                        = 0x0150,
  TARGETPRINTER                   = 0x0151,
  EXTRASAMPLES                    = 0x0152,
  SAMPLEFORMAT                    = 0x0153,
  SMINSAMPLEVALUE                 = 0x0154,
  SMAXSAMPLEVALUE                 = 0x0155,

  TRANSFERRANGE   = 0x0156,

  CLIPPATH                        = 0x0157, // TIFF PAGEMAKER TECHNOTE #2.

  JPEGTABLES                      = 0x015B, // TIFF-EP

  JPEGPROC   = 0x0200,
  JPEGINTERCHANGEFORMAT         = 0x0201,
  JPEGINTERCHANGEFORMATLENGTH = 0x0202,
  JPEGRESTARTINTERVAL             = 0x0203,
  JPEGLOSSLESSPREDICTORS          = 0x0205,
  JPEGPOINTTRANSFORMS             = 0x0206,
  JPEGQTABLES                     = 0x0207,
  JPEGDCTABLES                    = 0x0208,
  JPEGACTABLES                    = 0x0209,

  YCBCRCOEFFICIENTS  = 0x0211,
  YCBCRSUBSAMPLING  = 0x0212,
  YCBCRPOSITIONING  = 0x0213,

  REFERENCEBLACKWHITE  = 0x0214,
  KODAKWB = 0x0F00,
  EPSONWB = 0x0E80,
  RELATEDIMAGEFILEFORMAT    = 0x1000,
  RELATEDIMAGEWIDTH  = 0x1001,
  RELATEDIMAGELENGTH  = 0x1002,
  OLYMPUSREDMULTIPLIER  = 0x1017,
  OLYMPUSBLUEMULTIPLIER  = 0x1018,
  OLYMPUSIMAGEPROCESSING = 0x2040,
  FUJIOLDWB  = 0x2ff0,

  CANONCOLORDATA = 0x4001,

  SONYGRBGLEVELS = 0x7303,
  SONYRGGBLEVELS = 0x7313,

  CFAREPEATPATTERNDIM  = 0x828D,
  CFAPATTERN   = 0x828E,
  BATTERYLEVEL   = 0x828F,
  COPYRIGHT   = 0x8298,
  EXPOSURETIME   = 0x829A,
  FNUMBER    = 0x829D,

  // THESE ARE FROM THE NIFF SPEC AND ONLY REALLY VALID WHEN THE HEADER BEGINS WITH IIN1
  // SEE THE NIFFTAG ENUM FOR THE SPECIFCATION SPECIFIC NAMES
  ROTATION                        = 0x82B9,
  NAVYCOMPRESSION                 = 0x82BA,
  TILEINDEX                       = 0x82BB,
  // END NIFF SPECIFIC

  IPTCNAA           = 0x83BB,

  LEAFMETADATA = 0x8606,

  PHOTOSHOPPRIVATE                = 0x8649,

  EXIFIFDPOINTER        = 0x8769,
  INTERCOLORPROFILE  = 0x8773,
  EXPOSUREPROGRAM   = 0x8822,
  SPECTRALSENSITIVITY  = 0x8824,
  GPSINFOIFDPOINTER  = 0x8825,
  ISOSPEEDRATINGS          = 0x8827,
  OECF    = 0x8828,
  EXIFVERSION   = 0x9000,
  DATETIMEORIGINAL  = 0x9003,
  DATETIMEDIGITIZED  = 0x9004,
  COMPONENTSCONFIGURATION         = 0x9101,
  COMPRESSEDBITSPERPIXEL         = 0x9102,
  SHUTTERSPEEDVALUE  = 0x9201,
  APERTUREVALUE   = 0x9202,
  BRIGHTNESSVALUE    = 0x9203,
  EXPOSUREBIASVALUE  = 0x9204,
  MAXAPERTUREVALUE  = 0x9205,
  SUBJECTDISTANCE   = 0x9206,
  METERINGMODE   = 0x9207,
  LIGHTSOURCE   = 0x9208,
  FLASH    = 0x9209,
  FOCALLENGTH   = 0x920A,

  FLASHENERGY_TIFFEP              = 0x920B,// TIFF-EP
  SPACIALFREQUENCYRESPONSE        = 0x920C,// TIFF-EP
  NOISE                           = 0x920D,// TIFF-EP
  FOCALPLANEXRESOLUTION_TIFFEP    = 0x920E,// TIFF-EP
  FOCALPLANEYRESOLUTION_TIFFEP    = 0x920F,// TIFF-EP
  FOCALPLANERESOLUTIONUNIT_TIFFEP = 0x9210,// TIFF-EP
  IMAGENAME                       = 0x9211,// TIFF-EP
  SECURITYCLASSIFICATION          = 0x9212,// TIFF-EP

  IMAGEHISTORY                    = 0x9213, // TIFF-EP NULL SEPARATED LIST

  SUBJECTAREA   = 0x9214,

  EXPOSUREINDEX_TIFFEP            = 0x9215, // TIFF-EP
  TIFFEPSTANDARDID                = 0x9216, // TIFF-EP
  SENSINGMETHOD_TIFFEP            = 0x9217, // TIFF-EP

  MAKERNOTE   = 0x927C,
  USERCOMMENT   = 0x9286,
  SUBSECTIME   = 0x9290,
  SUBSECTIMEORIGINAL  = 0x9291,
  SUBSECTIMEDIGITIZED  = 0x9292,
  FLASHPIXVERSION   = 0xA000,
  COLORSPACE   = 0xA001,
  PIXELXDIMENSION   = 0xA002,
  PIXELYDIMENSION   = 0xA003,
  RELATEDSOUNDFILE  = 0xA004,
  INTEROPERABILITYIFDPOINTER = 0xA005,
  SAMSUNG_WB_RGGBLEVELSUNCORRECTED = 0xa021,
  SAMSUNG_WB_RGGBLEVELSBLACK = 0xa028,
  FLASHENERGY   = 0xA20B,
  SPATIALFREQUENCYRESPONSE = 0xA20C,
  FOCALPLANEXRESOLUTION         = 0xA20E,
  FOCALPLANEYRESOLUTION         = 0xA20F,
  FOCALPLANERESOLUTIONUNIT = 0xA210,
  SUBJECTLOCATION   = 0xA214,
  EXPOSUREINDEX   = 0xA215,
  SENSINGMETHOD   = 0xA217,
  FILESOURCE   = 0xA300,
  SCENETYPE   = 0xA301,
  EXIFCFAPATTERN          = 0xA302,
  CUSTOMRENDERED    = 0xA401,
  EXPOSUREMODE   = 0xA402,
  WHITEBALANCE   = 0xA403,
  DIGITALZOOMRATIO  = 0xA404,
  FOCALLENGTHIN35MMFILM         = 0xA405,
  SCENECAPTURETYPE  = 0xA406,
  GAINCONTROL   = 0xA407,
  CONTRAST   = 0xA408,
  SATURATION   = 0xA409,
  SHARPNESS   = 0xA40A,
  DEVICESETTINGDESCRIPTION = 0xA40B,
  SUBJECTDISTANCERANGE  = 0xA40C,
  IMAGEUNIQUEID     = 0xA420,

  // THE FOLLOWING IDS ARE NOT DESCRIBED THE EXIF SPEC
#ifndef GAMMA
  GAMMA                           = 0xA500,
#endif

  // THE XMP SPEC DECLARES THAT XMP DATA SHOULD LIVE 0x2BC WHEN
  // EMBEDDED IN TIFF IMAGES.
  XMP                             = 0x02BC,
  // Canon tag for uncompressed RGB preview
  CANON_UNCOMPRESSED              = 0xC5D9,

  // FROM THE DNG SPEC
  DNGVERSION                      = 0xC612, // IFD0
  DNGBACKWARDVERSION              = 0xC613, // IFD0
  UNIQUECAMERAMODEL               = 0xC614, // IFD0
  LOCALIZEDCAMERAMODEL            = 0xC615, // IFD0
  CFAPLANECOLOR                   = 0xC616, // RAWIFD
  CFALAYOUT                       = 0xC617, // RAWIFD
  LINEARIZATIONTABLE              = 0xC618, // RAWIFD
  BLACKLEVELREPEATDIM             = 0xC619, // RAWIFD
  BLACKLEVEL                      = 0xC61A, // RAWIFD
  BLACKLEVELDELTAH                = 0xC61B, // RAWIFD
  BLACKLEVELDELTAV                = 0xC61C, // RAWIFD
  WHITELEVEL                      = 0xC61D, // RAWIFD
  DEFAULTSCALE                    = 0xC61E, // RAWIFD
  DEFAULTCROPORIGIN               = 0xC61F, // RAWIFD
  DEFAULTCROPSIZE                 = 0xC620, // RAWIFD
  COLORMATRIX1                    = 0xC621, // IFD0
  COLORMATRIX2                    = 0xC622, // IFD0
  CAMERACALIBRATION1              = 0xC623, // IFD0
  CAMERACALIBRATION2              = 0xC624, // IFD0
  REDUCTIONMATRIX1                = 0xC625, // IFD0
  REDUCTIONMATRIX2                = 0xC626, // IFD0
  ANALOGBALANCE                   = 0xC627, // IFD0
  ASSHOTNEUTRAL                   = 0xC628, // IFD0
  ASSHOTWHITEXY                   = 0xC629, // IFD0
  BASELINEEXPOSURE                = 0xC62A, // IFD0
  BASELINENOISE                   = 0xC62B, // IFD0
  BASELINESHARPNESS               = 0xC62C, // IFD0
  BAYERGREESPIT                   = 0xC62D, // IFD0
  LINEARRESPONSELIMIT             = 0xC62E, // IFD0
  CAMERASERIALNUMBER              = 0xC62F, // IFD0
  LENSINFO                        = 0xC630, // IFD0
  CHROMABLURRADIUS                = 0xC631, // RAWIFD
  ANTIALIASSTRENGTH               = 0xC632, // RAWIFD
  DNGPRIVATEDATA                  = 0xC634, // IFD0

  MAKERNOTESAFETY                 = 0xC635, // IFD0

  // THE SPEC SAYS BESTQUALITYSCALE IS 0xC635 BUT IT APPEARS TO BE WRONG
  //BESTQUALITYSCALE                = 0xC635, // RAWIFD
  BESTQUALITYSCALE                = 0xC65C, // RAWIFD  THIS LOOKS LIKE THE CORRECT VALUE
  SHADOWSCALE           = 50739,
  RAWDATAUNIQUEID    = 50781,
  ORIGINALRAWFILENAME   = 50827,
  ORIGINALRAWFILEDATA   = 50828,
  ACTIVEAREA     = 50829,
  MASKEDAREAS     = 50830,
  ASSHOTICCPROFILE    = 50831,
  ASSHOTPREPROFILEMATRIX  = 50832,
  CURRENTICCPROFILE    = 50833,
  CURRENTPREPROFILEMATRIX  = 50834,
  COLORIMETRICREFERENCE   = 50879,
  KODAKKDCPRIVATEIFD   = 65024,
  CAMERACALIBRATIONSIGNATURE = 0xC6F3,
  PROFILECALIBRATIONSIGNATURE = 0xC6F4,
  EXTRACAMERAPROFILES = 0xC6F5,
  ASSHOTPROFILENAME = 0xC6F6,
  NOISEREDUCTIONAPPLIED = 0xC6F7,
  PROFILENAME = 0xC6F8,
  PROFILEHUESATMAPDIMS = 0xC6F9,
  PROFILEHUESATMAPDATA1 = 0xC6FA,
  PROFILEHUESATMAPDATA2 = 0xC6FB,
  PROFILETONECURVE = 0xC6FC,
  PROFILEEMBEDPOLICY = 0xC6FD,
  PROFILECOPYRIGHT = 0xC6FE,
  FORWARDMATRIX1 = 0xC714,
  FORWARDMATRIX2 = 0xC715,
  PREVIEWAPPLICATIONNAME = 0xC716,
  PREVIEWAPPLICATIONVERSION = 0xC717,
  PREVIEWSETTINGSNAME = 0xC718,
  PREVIEWSETTINGSDIGEST = 0xC719,
  PREVIEWCOLORSPACE = 0xC71A,
  PREVIEWDATETIME = 0xC71B,
  RAWIMAGEDIGEST = 0xC71C,
  ORIGINALRAWFILEDIGEST = 0xC71D,
  SUBTILEBLOCKSIZE = 0xC71E,
  ROWINTERLEAVEFACTOR = 0xC71F,
  PROFILELOOKTABLEDIMS = 0xC725,
  PROFILELOOKTABLEDATA = 0xC726,
  OPCODELIST1 = 0xC740,
  OPCODELIST2 = 0xC741,
  OPCODELIST3 = 0xC742,
  NOISEPROFILE = 0xC761,
  CANONCR2SLICE                   = 0xC640,   // CANON CR2
  CANON_SRAWTYPE                  = 0xC6C5, // IFD3
  CANON_SENSOR_INFO               = 0x00E0, // MakerNote
  CANON_RAW_DATA_OFFSET           = 0x0081, // MakerNote TIF

  CALIBRATIONILLUMINANT1          = 0xC65A, // IFD0
  CALIBRATIONILLUMINANT2          = 0xC65B, // IFD0
  SONY_CURVE = 28688,
  SONY_OFFSET = 0x7200,
  SONY_LENGTH = 0x7201,
  SONY_KEY    = 0x7221,

  // PRINT IMAGE MATCHING DATA
  PIMIFDPOINTER                   = 0xC4A5,
  FUJI_RAW_IFD = 0xF000,
  FUJI_RAWIMAGEFULLWIDTH = 0xF001,
  FUJI_RAWIMAGEFULLHEIGHT = 0xF002,
  FUJI_BITSPERSAMPLE = 0xF003,
  FUJI_STRIPOFFSETS = 0xF007,
  FUJI_STRIPBYTECOUNTS = 0xF008,
  FUJI_BLACKLEVEL = 0xF00A,
  FUJI_WB_GRBLEVELS = 0xF00E,

  KODAK_IFD = 0x8290,
  KODAK_LINEARIZATION = 0x090D,
  KODAK_KDC_WB = 0xFA2A,
  KODAK_KDC_OFFSET = 0xFD04,
  KODAK_KDC_WIDTH = 0xFD00,
  KODAK_KDC_HEIGHT = 0xFD01,
  KODAK_KDC_SENSOR_WIDTH = 0xFA13,
  KODAK_KDC_SENSOR_HEIGHT = 0xFA14,
  KODAK_IFD2 = 0xFE00,
};

} // namespace rawspeed
