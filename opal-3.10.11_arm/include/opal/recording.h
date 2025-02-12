/*
 * audiorecord.h
 *
 * OPAL audio record manager
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
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
 * $Revision: 27110 $
 * $Author: rjongbloed $
 * $Date: 2012-03-04 22:12:36 -0600 (Sun, 04 Mar 2012) $
 */


#ifndef OPAL_OPAL_AUDIORECORD_H
#define OPAL_OPAL_AUDIORECORD_H


#include <opal/buildopts.h>

#if OPAL_HAS_MIXER


/** This is an abstract class for recording OPAL calls.
    A factory is used to created concrete classes based on the file extension
    supported by the individual record manager.
  */
class OpalRecordManager
{
  public:
    /** Factory for creating new recording managers. Selection is made based on the
        file extension of the file supplied to OpalManager::StartRecording().

        Currently only WAV files, and for WIndows only, AVI files, are supported.
        Howeer this factory allows an application to add their own file formats. */
    typedef PFactory<OpalRecordManager, PCaselessString> Factory;

#if OPAL_VIDEO
    enum VideoMode {
      eSideBySideLetterbox, /**< Two images side by side with black bars top and bottom.
                                 It is expected that the input frames and output are all
                                 the same aspect ratio, e.g. 4:3. Works well if inputs
                                 are QCIF and output is CIF for example. */
      eSideBySideScaled,    /**< Two images side by side, scaled to fit halves of output
                                 frame. It is expected that the output frame be double
                                 the width of the input data to maintain aspect ratio.
                                 e.g. for CIF inputs, output would be 704x288. */
      eStackedPillarbox,    /**< Two images, one on top of the other with black bars down
                                 the sides. It is expected that the input frames and output
                                 are all the same aspect ratio, e.g. 4:3. Works well if
                                 inputs are QCIF and output is CIF for example. */
      eStackedScaled,       /**< Two images, one on top of the other, scaled to fit halves
                                 of output frame. It is expected that the output frame be
                                 double the height of the input data to maintain aspect
                                 ratio. e.g. for CIF inputs, output would be 352x576. */
      eSeparateStreams,     ///< Unsupported
      NumVideoMixingModes
    };
#endif

    // Options for recording calls.
    struct Options {
      bool      m_stereo;       /**< Flag to indicate the recoding will be stereo where
                                      incoming & outgoing audio are in individual channels. */
      PString   m_audioFormat;  /**< Audio format for file output. The formats that are
                                     supported is dependent on the concrete
                                     OpalRecordManager class. For example, for WAV files
                                     "PCM-16", "G.723.1", "G.728", "G.729" or "MS-GSM"
                                     is supported. */

#if OPAL_VIDEO
      VideoMode m_videoMixing;  ///< Mode for how incoming video is mixed.
      PString   m_videoFormat;  /**< Audio format for file output. The formats that are
                                     supported is dependent on the concrete
                                     OpalRecordManager class. For example, for AVI files
                                     this will be the four letter code supported by the
                                     operating system, e.g. "MSVC" for Microsoft Video 1. */
      unsigned  m_videoWidth;   ///< Video mixer buffer width. Inputs are scaled accordingly.
      unsigned  m_videoHeight;  ///< Video mixer buffer heigth. Inputs are scaled accordingly.
      unsigned  m_videoRate;    /**< Video mixer output frame rate. This is independent of
                                     the input frame rates. */
#endif

      Options(
        bool         stereo = true,
#if OPAL_VIDEO
        VideoMode    videoMixing = eSideBySideLetterbox,
#endif
        const char * audioFormat = NULL
#if OPAL_VIDEO
        ,
        const char * videoFormat = NULL,
        unsigned width = PVideoFrameInfo::CIFWidth,
        unsigned height = PVideoFrameInfo::CIFHeight,
        unsigned rate = 15
#endif
      ) : m_stereo(stereo)
        , m_audioFormat(audioFormat)
#if OPAL_VIDEO
        , m_videoMixing(videoMixing)
        , m_videoFormat(videoFormat)
        , m_videoWidth(width)
        , m_videoHeight(height)
        , m_videoRate(rate)
#endif
      {
      }
    };

    virtual ~OpalRecordManager() { }

    /**Open the recording file.
      */
    bool Open(const PFilePath & fn)
    {
      return OpenFile(fn);
    }

    /**Open the recoding file indicating audio mode.
      */
    bool Open(const PFilePath & fn, bool mono) // For backward compatibility
    {
      m_options.m_stereo = !mono;
      return OpenFile(fn);
    }

    /**Open the recording file indicating the options to be used.
      */
    bool Open(const PFilePath & fn, const Options & options)
    {
      m_options = options;
      return Open(fn);
    }

    /**Indicate if the recording file is open.
      */
    virtual bool IsOpen() const = 0;

    /**Close the recording file.
       Note this may block until various sub-threads are termianted so
       care may be needed to avoid deadlocks.
      */
    virtual bool Close() = 0;

    /**Open an individual media stream using the provided identifier and format.
      */
    virtual bool OpenStream(
      const PString & strmId,         ///< Identifier for media stream.
      const OpalMediaFormat & format  ///< Media format for new stream
    ) = 0;

    /**Close the media stream based on the identifier provided.
      */
    virtual bool CloseStream(
      const PString & strmId  ///< Identifier for media stream.
    ) = 0;

    /**Write audio to the recording file.
      */
    virtual bool WriteAudio(
      const PString & strmId,     ///< Identifier for media stream.
      const RTP_DataFrame & rtp   ///< RTP data containing PCM-16 data
    ) = 0;

#if OPAL_VIDEO
    /**Write video to the recording file.
      */
    virtual bool WriteVideo(
      const PString & strmId,     ///< Identifier for media stream.
      const RTP_DataFrame & rtp   ///< RTP data containing a YUV420P frame
    ) = 0;
#endif

    /**Get the options for this recording.
      */
    const Options & GetOptions() const { return m_options; }

    /**Set the options for this recording.
      */
    void SetOptions(const Options & options)
    {
      m_options = options;
    }

  protected:
    virtual bool OpenFile(const PFilePath & fn) = 0;

    Options m_options;
};

// Force linking of modules
PFACTORY_LOAD(OpalWAVRecordManager);
#if OPAL_VIDEO && P_VFW_CAPTURE
PFACTORY_LOAD(OpalAVIRecordManager);
#endif

#endif // OPAL_HAS_MIXER


#endif // OPAL_OPAL_AUDIORECORD_H
