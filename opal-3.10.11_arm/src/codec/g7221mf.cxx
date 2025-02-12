/*
 * g7221mf.cxx
 *
 * GSM-AMR Media Format descriptions
 *
 * Open Phone Abstraction Library
 * Formally known as the Open H323 project.
 *
 * Copyright (c) 2008 Vox Lucida
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
 * The Original Code is Open Phone Abstraction Library
 *
 * The Initial Developer of the Original Code is Vox Lucida
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 21964 $
 * $Author: rjongbloed $
 * $Date: 2009-01-28 20:07:30 -0600 (Wed, 28 Jan 2009) $
 */

#include <ptlib.h>
#include <opal/buildopts.h>

#include <opal/mediafmt.h>
#include <codec/opalplugin.h>
#include <h323/h323caps.h>


#define new PNEW


/////////////////////////////////////////////////////////////////////////////

#if OPAL_H323
class H323_G7221Capability : public H323GenericAudioCapability
{
  public:
    H323_G7221Capability()
      : H323GenericAudioCapability(OpalPluginCodec_Identifer_G7221)
    {
    }

    virtual PObject * Clone() const
    {
      return new H323_G7221Capability(*this);
    }

    virtual PString GetFormatName() const
    {
      return OpalG7221;
    }
};
#endif // OPAL_H323

const OpalAudioFormat & GetOpalG7221()
{
  static class OpalG7221Format : public OpalAudioFormat
  {
    public:
      OpalG7221Format()
        : OpalAudioFormat(OPAL_G7221, RTP_DataFrame::DynamicBase, "G7221",  80, 320, 1, 1, 1, 16000)
      {
      }
  } const G7221_Format;

#if OPAL_H323
  static H323CapabilityFactory::Worker<H323_G7221Capability> G7221_Factory(OPAL_G7221, true);
#endif // OPAL_H323

  return G7221_Format;
}


// End of File ///////////////////////////////////////////////////////////////
