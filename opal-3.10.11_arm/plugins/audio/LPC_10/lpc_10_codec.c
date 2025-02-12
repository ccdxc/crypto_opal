/*
 * LPC 10 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) 2004 Post Increment, All Rights Reserved
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
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 22678 $
 * $Author: rjongbloed $
 * $Date: 2009-05-20 19:35:59 -0500 (Wed, 20 May 2009) $
 */

#define _CRT_NONSTDC_NO_DEPRECATE 1
#define _CRT_SECURE_NO_WARNINGS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PLUGIN_CODEC_DLL_EXPORTS
#include "plugin-config.h"
#endif

#include <codec/opalplugin.h>

#include "lpc10.h"


enum {
  SamplesPerFrame = 180,    // 22.5 milliseconds
  BitsPerFrame    = 54,        // Encoded size
  BytesPerFrame   = (BitsPerFrame+7)/8,
  BitsPerSecond   = 2400
};

#define   SampleValueScale 32768.0
#define   MaxSampleValue   32767.0
#define   MinSampleValue   -32767.0

#define PREF_FRAMES_PER_PACKET  4
#define MAX_FRAMES_PER_PACKET   7

/////////////////////////////////////////////////////////////////////////////

static void * create_encoder(const struct PluginCodec_Definition * codec)
{
  struct lpc10_encoder_state * encoder = (struct lpc10_encoder_state *)malloc((unsigned)sizeof(struct lpc10_encoder_state));
  init_lpc10_encoder_state(encoder);
  return encoder;
}

static int codec_encoder(const struct PluginCodec_Definition * codec, 
                                           void * context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,         
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  int i;
  INT32 bits[BitsPerFrame];
  real speech[SamplesPerFrame];
  unsigned char * buffer = (unsigned char *)to;
  const short * sampleBuffer = (const short *)from;

  if ((*fromLen)/2 != SamplesPerFrame || *toLen < BytesPerFrame)
    return 0;

  for (i = 0; i < SamplesPerFrame; i++)
    speech[i] = (real)(sampleBuffer[i]/SampleValueScale);

  lpc10_encode(speech, bits, (struct lpc10_encoder_state *)context);

  memset(to, 0, BytesPerFrame);
  for (i = 0; i < BitsPerFrame; i++) {
    if (bits[i])
      buffer[i>>3] |= 1 << (i&7);
  }

  *toLen = BytesPerFrame;

  return 1;
}

static void destroy_encoder(const struct PluginCodec_Definition * codec, void * context)
{
  free((struct lpc10_encoder_state *)context);
}

static void * create_decoder(const struct PluginCodec_Definition * codec)
{
  struct lpc10_decoder_state * decoder = (struct lpc10_decoder_state *)malloc((unsigned)sizeof(struct lpc10_decoder_state));
  init_lpc10_decoder_state(decoder);
  return decoder;
}

static int codec_decoder(const struct PluginCodec_Definition * codec, 
                                           void * context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,         
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  int i;
  INT32 bits[BitsPerFrame];
  real speech[SamplesPerFrame];
  short * sampleBuffer = (short *)to;
  const unsigned char * buffer = (const unsigned char *)from;

  if (*fromLen < BytesPerFrame || *toLen < SamplesPerFrame*2)
    return 0;

  for (i = 0; i < BitsPerFrame; i++)
    bits[i] = (buffer[i>>3]&(1<<(i&7))) != 0;

  lpc10_decode(bits, speech, (struct lpc10_decoder_state *)context);

  for (i = 0; i < SamplesPerFrame; i++) {
    real sample = (real)(speech[i]*SampleValueScale);
    if (sample < MinSampleValue)
      sample = MinSampleValue;
    else if (sample > MaxSampleValue)
      sample = MaxSampleValue;
    sampleBuffer[i] = (short)sample;
  }

  *toLen = SamplesPerFrame*2;

  return 1;
}

static void destroy_decoder(const struct PluginCodec_Definition * codec, void * context)
{
  free((struct lpc10_decoder_state *)context);
}

/////////////////////////////////////////////////////////////////////////////

#define	EQUIVALENCE_COUNTRY_CODE            9
#define	EQUIVALENCE_EXTENSION_CODE          0
#define	EQUIVALENCE_MANUFACTURER_CODE       61

static const char lpc10CapStr[] = { "LPC-10" };

static struct PluginCodec_H323NonStandardCodecData lpc10Cap =
{
  NULL, 
  EQUIVALENCE_COUNTRY_CODE, 
  EQUIVALENCE_EXTENSION_CODE, 
  EQUIVALENCE_MANUFACTURER_CODE,
  lpc10CapStr, sizeof(lpc10CapStr)-1,
  NULL
};

static struct PluginCodec_information licenseInfo = {
  1083666706,                              // timestamp = Tue 04 May 2004 10:31:46 AM UTC = 

  "Craig Southeren, Post Increment",                           // source code author
  "1.0",                                                       // source code version
  "craigs@postincrement.com",                                  // source code email
  "http://www.postincrement.com",                              // source code URL
  "Copyright (C) 2004 by Post Increment, All Rights Reserved", // source code copyright
  "MPL 1.0",                                                   // source code license
  PluginCodec_License_MPL,                                     // source code license
  
  "LPC-10",                                                    // codec description
  "Andy Fingerhut",                                            // codec author
  NULL,                                                        // codec version
  "jaf@arl.wustl.edu",                                         // codec email
  NULL,                                                        // codec URL
  NULL,                                                        // codec copyright information
  NULL,                                                        // codec license
  PluginCodec_Licence_None                                     // codec license code
};

static const char L16Desc[]  = { "L16" };

static const char lpc10[]    = { "LPC-10" };

static const char sdpLPC10[] = { "lpc10" };

static struct PluginCodec_Definition lpc10CodecDefn[2] =
{
  { 
    // encoder
    PLUGIN_CODEC_VERSION,               // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    lpc10,                              // text decription
    L16Desc,                            // source format
    lpc10,                              // destination format

    (void *)NULL,                       // user data

    8000,                               // samples per second
    BitsPerSecond,                      // raw bits per second
    22500,                              // nanoseconds per frame
    SamplesPerFrame,                    // samples per frame
    BytesPerFrame,                      // bytes per frame
    PREF_FRAMES_PER_PACKET,             // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,              // maximum number of frames per packe
    0,                                  // IANA RTP payload code
    sdpLPC10,                           // RTP payload name

    create_encoder,                     // create codec function
    destroy_encoder,                    // destroy codec
    codec_encoder,                      // encode/decode
    NULL,                               // codec controls

    PluginCodec_H323Codec_nonStandard,  // h323CapabilityType 
    &lpc10Cap                           // h323CapabilityData
  },

  { 
    // decoder
    PLUGIN_CODEC_VERSION,               // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    lpc10,                              // text decription
    lpc10,                              // source format
    L16Desc,                            // destination format

    (const void *)NULL,                 // user data

    8000,                               // samples per second
    BitsPerSecond,                      // raw bits per second
    22500,                              // nanoseconds per frame
    SamplesPerFrame,                    // samples per frame
    BytesPerFrame,                      // bytes per frame
    PREF_FRAMES_PER_PACKET,             // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,              // maximum number of frames per packe
    0,                                  // IANA RTP payload code
    sdpLPC10,                           // RTP payload name

    create_decoder,                     // create codec function
    destroy_decoder,                    // destroy codec
    codec_decoder,                      // encode/decode
    NULL,                               // codec controls

    PluginCodec_H323Codec_nonStandard,  // h323CapabilityType 
    &lpc10Cap                           // h323CapabilityData
  }
};


PLUGIN_CODEC_IMPLEMENT_ALL(LPC 10, lpc10CodecDefn, PLUGIN_CODEC_VERSION)

/////////////////////////////////////////////////////////////////////////////
