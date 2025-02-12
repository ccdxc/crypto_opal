/*
 * rfc4175.h
 *
 * RFC4175 transport for uncompressed video
 *
 * Open Phone Abstraction Library
 *
 * Copyright (C) 2007 Post Increment
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 24413 $
 * $Author: rjongbloed $
 * $Date: 2010-05-27 19:41:47 -0500 (Thu, 27 May 2010) $
 */

#ifndef OPAL_CODEC_RFC4175_H
#define OPAL_CODEC_RFC4175_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <ptlib.h>

#include <opal/buildopts.h>

#if OPAL_RFC4175

#include <ptclib/random.h>

#include <opal/transcoders.h>
#include <codec/opalplugin.h>
#include <codec/vidcodec.h>


#define OPAL_RFC4175_YCbCr420  "RFC4175_YCbCr-4:2:0"
extern const OpalVideoFormat & GetOpalRFC4175_YCbCr420();
#define OpalRFC4175YCbCr420    GetOpalRFC4175_YCbCr420()

#define OPAL_RFC4175_RGB       "RFC4175_RGB"
extern const OpalVideoFormat & GetOpalRFC4175_RGB();
#define OpalRFC4175RGB         GetOpalRFC4175_RGB()


/////////////////////////////////////////////////////////////////////////////

class OpalRFC4175Transcoder : public OpalVideoTranscoder
{
  PCLASSINFO(OpalRFC4175Transcoder, OpalVideoTranscoder);
  public:
    OpalRFC4175Transcoder(      
      const OpalMediaFormat & inputMediaFormat,  ///<  Input media format
      const OpalMediaFormat & outputMediaFormat  ///<  Output media format
    );
    virtual PINDEX GetPgroupSize() const = 0;
    virtual PINDEX GetColsPerPgroup() const = 0;
    virtual PINDEX GetRowsPerPgroup() const = 0;

    virtual PINDEX PixelsToBytes(PINDEX pixels) const = 0;
    PINDEX RFC4175HeaderSize(PINDEX lines);

    struct ScanLineHeader {
      PUInt16b m_length;
      PUInt16b m_y;       // has field flag in top bit
      PUInt16b m_offset;  // has last line flag in top bit
    };
};

/////////////////////////////////////////////////////////////////////////////

class OpalRFC4175Encoder : public OpalRFC4175Transcoder
{
  PCLASSINFO(OpalRFC4175Encoder, OpalRFC4175Transcoder);
  public:
    OpalRFC4175Encoder(      
      const OpalMediaFormat & inputMediaFormat,  ///<  Input media format
      const OpalMediaFormat & outputMediaFormat  ///<  Output media format
    );

    bool ConvertFrames(const RTP_DataFrame & input, RTP_DataFrameList & output);

  protected:
    virtual void StartEncoding(const RTP_DataFrame & input);
    virtual void EndEncoding() = 0;

    void EncodeFullFrame();
    void EncodeScanLineSegment(PINDEX y, PINDEX offs, PINDEX width);
    void AddNewDstFrame();
    void FinishOutputFrame();

    DWORD m_extendedSequenceNumber;
    PINDEX m_maximumPacketSize;
    unsigned m_frameHeight;
    unsigned m_frameWidth;

    DWORD m_srcTimestamp;

    RTP_DataFrameList * m_dstFrames;
    std::vector<PINDEX> m_dstScanlineCounts;
    PINDEX m_dstScanLineCount;
    PINDEX m_dstPacketSize;
    ScanLineHeader * m_dstScanLineTable;
};

/////////////////////////////////////////////////////////////////////////////

class OpalRFC4175Decoder : public OpalRFC4175Transcoder
{
  PCLASSINFO(OpalRFC4175Decoder, OpalRFC4175Transcoder);
  public:
    OpalRFC4175Decoder(      
      const OpalMediaFormat & inputMediaFormat,  ///<  Input media format
      const OpalMediaFormat & outputMediaFormat  ///<  Output media format
    );
    ~OpalRFC4175Decoder();

    virtual PINDEX PixelsToBytes(PINDEX pixels) const = 0;
    virtual PINDEX BytesToPixels(PINDEX pixels) const = 0;

    bool ConvertFrames(const RTP_DataFrame & input, RTP_DataFrameList & output);

  protected:
    void DecodeFramesAndSetFrameSize(RTP_DataFrameList & output);
    virtual bool DecodeFrames(RTP_DataFrameList & output) = 0;

    RTP_DataFrameList m_inputFrames;
    std::vector<PINDEX> m_scanlineCounts;
    PINDEX m_frameWidth, m_frameHeight;

    bool    m_first;
    bool    m_missingPackets;
    PINDEX  m_maxWidth;
    PINDEX  m_maxHeight;
    DWORD   m_nextSequenceNumber;
    DWORD   m_lastTimeStamp;
    DWORD   m_timeStampOfFrame;
    DWORD   m_firstSequenceOfFrame;
};

/////////////////////////////////////////////////////////////////////////////

/**This class defines a transcoder implementation class that converts RFC4175 to YUV420P
 */
class Opal_RFC4175YCbCr420_to_YUV420P : public OpalRFC4175Decoder
{
  PCLASSINFO(Opal_RFC4175YCbCr420_to_YUV420P, OpalRFC4175Decoder);
  public:
    Opal_RFC4175YCbCr420_to_YUV420P() : OpalRFC4175Decoder(OpalRFC4175YCbCr420, OpalYUV420P) { }
    PINDEX GetPgroupSize() const        { return 6; }       
    PINDEX GetColsPerPgroup() const     { return 2; }   
    PINDEX GetRowsPerPgroup() const     { return 2; }

    PINDEX PixelsToBytes(PINDEX pixels) const { return pixels*12/8; }
    PINDEX BytesToPixels(PINDEX bytes) const  { return bytes*8/12; }

    bool DecodeFrames(RTP_DataFrameList & output);
};

class Opal_YUV420P_to_RFC4175YCbCr420 : public OpalRFC4175Encoder
{
  PCLASSINFO(Opal_YUV420P_to_RFC4175YCbCr420, OpalRFC4175Encoder);
  public:
    Opal_YUV420P_to_RFC4175YCbCr420() : OpalRFC4175Encoder(OpalYUV420P, OpalRFC4175YCbCr420) { }
    PINDEX GetPgroupSize() const        { return 6; }       
    PINDEX GetColsPerPgroup() const     { return 2; }   
    PINDEX GetRowsPerPgroup() const     { return 2; }

    PINDEX PixelsToBytes(PINDEX pixels) const { return pixels * 12 / 8; }
    PINDEX BytesToPixels(PINDEX bytes) const  { return bytes * 8 / 12; }

    void StartEncoding(const RTP_DataFrame & input);
    void EndEncoding();

  protected:
    BYTE * m_srcYPlane;
    BYTE * m_srcCbPlane;
    BYTE * m_srcCrPlane;
};

/**This class defines a transcoder implementation class that converts RFC4175 to RGB24
 */
class Opal_RFC4175RGB_to_RGB24 : public OpalRFC4175Decoder
{
  PCLASSINFO(Opal_RFC4175RGB_to_RGB24, OpalRFC4175Decoder);
  public:
    Opal_RFC4175RGB_to_RGB24() : OpalRFC4175Decoder(OpalRFC4175RGB, OpalRGB24) { }
    PINDEX GetPgroupSize() const        { return 3; }       
    PINDEX GetColsPerPgroup() const     { return 1; }   
    PINDEX GetRowsPerPgroup() const     { return 1; }

    PINDEX PixelsToBytes(PINDEX pixels) const { return pixels * 3; }
    PINDEX BytesToPixels(PINDEX bytes) const  { return bytes / 3; }

    bool DecodeFrames(RTP_DataFrameList & output);
};

class Opal_RGB24_to_RFC4175RGB : public OpalRFC4175Encoder
{
  PCLASSINFO(Opal_RGB24_to_RFC4175RGB, OpalRFC4175Encoder);
  public:
    Opal_RGB24_to_RFC4175RGB() : OpalRFC4175Encoder(OpalRGB24, OpalRFC4175RGB) { }
    PINDEX GetPgroupSize() const        { return 3; }       
    PINDEX GetColsPerPgroup() const     { return 1; }   
    PINDEX GetRowsPerPgroup() const     { return 1; }

    PINDEX PixelsToBytes(PINDEX pixels) const { return pixels * 3; }
    PINDEX BytesToPixels(PINDEX bytes) const  { return bytes / 3; }

    void StartEncoding(const RTP_DataFrame & input);
    void EndEncoding();

  protected:
    BYTE * m_rgbBase;
};


#define OPAL_REGISTER_RFC4175_VIDEO(oformat, rformat) \
  OPAL_REGISTER_TRANSCODER(Opal_RFC4175##rformat##_to_##oformat, OpalRFC4175##rformat, Opal##oformat); \
  OPAL_REGISTER_TRANSCODER(Opal_##oformat##_to_RFC4175##rformat, Opal##oformat, OpalRFC4175##rformat);

#define OPAL_REGISTER_RFC4175() \
  OPAL_REGISTER_RFC4175_VIDEO(YUV420P, YCbCr420); \
  OPAL_REGISTER_RFC4175_VIDEO(RGB24, RGB)


/////////////////////////////////////////////////////////////////////////////

#endif // OPAL_RFC4175

#endif // OPAL_CODEC_RFC4175_H
