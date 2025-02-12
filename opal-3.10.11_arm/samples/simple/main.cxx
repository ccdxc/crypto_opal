/*
 * main.cxx
 *
 * A simple H.323 "net telephone" application.
 *
 * Copyright (c) 2000 Equivalence Pty. Ltd.
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
 * The Original Code is Portable Windows Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 27919 $
 * $Author: rjongbloed $
 * $Date: 2012-06-27 03:26:42 -0500 (Wed, 27 Jun 2012) $
 */

#include <ptlib.h>

#include <opal/buildopts.h>

#if OPAL_IAX2
#include <iax2/iax2.h>
#endif

#if OPAL_CAPI
#include <lids/capi_ep.h>
#endif

#if OPAL_SIP
#include <sip/sip.h>
#endif

#if OPAL_H323
#include <h323/h323.h>
#include <h323/gkclient.h>
#endif

#if OPAL_FAX
#include <t38/t38proto.h>
#endif

#include <opal/transcoders.h>
#include <lids/lidep.h>
#include <ptclib/pstun.h>
#include <ptlib/config.h>
#include <codec/opalpluginmgr.h>
//dong motify for tcp/telnet control
#include <ptclib/cli.h>

#include "main.h"
#include "../../version.h"
#include "msg_set.h"


#define new PNEW

//PCREATE_PROCESS(SimpleOpalProcess);
int main(int argc, char ** argv, char ** envp)
{
	SimpleOpalProcess *pInstance = new SimpleOpalProcess();
	pInstance->PreInitialise(argc, argv, envp);
	int terminationValue = pInstance->InternalMain();
	delete pInstance;
	return terminationValue;
}

//dong motify for tcp/telnet control
#define TCP_CTRL_PORT 8888
///////////////////////////////////////////////////////////////

SimpleOpalProcess::SimpleOpalProcess()
  : PProcess("CHR_DZY_CK_WKS_CJL", "SDT_316",
             MAJOR_VERSION, MINOR_VERSION, BUILD_TYPE, BUILD_NUMBER)
{
}


void SimpleOpalProcess::Main()
{
  cout << GetName()
       << " Version " << GetVersion(PTrue)
       << " by " << GetManufacturer()
       << " on " << GetOSClass() << ' ' << GetOSName()
       << " (" << GetOSVersion() << '-' << GetOSHardware() << ")\n\n";

  // Get and parse all of the command line arguments.
  PArgList & args = GetArguments();
  args.Parse(
             "a-auto-answer."
             "b-bandwidth:"
             "D-disable:"
             "d-dial-peer:"
             "-disableui."
             "e-silence."
             "f-fast-disable."
             "g-gatekeeper:"
             "G-gk-id:"
             "-gk-token:"
             "-disable-grq."
             "h-help."
             "H-no-h323."
#if OPAL_PTLIB_SSL
             "-no-h323s."
             "-h323s-listen:"
             "-h323s-gk:"
#endif
             "-h323-listen:"
             "I-no-sip."
             "j-jitter:"
             "l-listen."
#if OPAL_LID
             "L-no-lid."
             "-lid:"
             "-country:"
#endif
             "-no-std-dial-peer."
#if PTRACING
             "o-output:"
#endif
             "O-option:"
             "P-prefer:"
             "p-password:"
             "-portbase:"
             "-portmax:"
             "R-require-gatekeeper."
             "r-register-sip:"
             "-rtp-base:"
             "-rtp-max:"
             "-rtp-tos:"
             "s-sound:"
             "S-no-sound."
             "-sound-in:"
             "-sound-out:"
             "-srcep:"
             "-sip-listen:"
             "-sip-proxy:"
             "-sip-domain:"
             "-sip-user-agent:"
             "-sip-ui:"
             "-stun:"
             "T-h245tunneldisable."
             "-translate:"
             "-tts:"

#if PTRACING
             "t-trace."
#endif
             "-tcp-base:"
             "-tcp-max:"
             "u-user:"
             "-udp-base:"
             "-udp-max:"
             "-use-long-mime."
#if OPAL_VIDEO
             "C-ratecontrol:"
             "-rx-video." "-no-rx-video."
             "-tx-video." "-no-tx-video."
             "-grabber:"
             "-grabdriver:"
             "-grabchannel:"
             "-display:"
             "-displaydriver:"
             "-preview:"
             "-previewdriver:"
             "-video-size:"
             "-video-rate:"
             "-video-bitrate:"
#endif
#if OPAL_IVR
             "V-no-ivr."
             "x-vxml:"
#endif
#if OPAL_IAX2
             "X-no-iax2."
	     "-iaxport:"
#endif
#if OPAL_CAPI
             "-no-capi."
#endif
          , PFalse);


  if (args.HasOption('h') || (!args.HasOption('l') && args.GetCount() == 0)) {
    cout << "Usage : " << GetFile().GetTitle() << " [options] -l\n"
            "      : " << GetFile().GetTitle() << " [options] [alias@]hostname   (no gatekeeper)\n"
            "      : " << GetFile().GetTitle() << " [options] alias[@hostname]   (with gatekeeper)\n"
            "General options:\n"
            "  -l --listen             : Listen for incoming calls.\n"
            "  -d --dial-peer spec     : Set dial peer for routing calls (see below)\n"
            "     --no-std-dial-peer   : Do not include the standard dial peers\n"
            "  -a --auto-answer        : Automatically answer incoming calls.\n"
            "  -u --user name          : Set local alias name(s) (defaults to login name).\n"
            "  -p --password pwd       : Set password for user (gk or SIP authorisation).\n"
            "  -D --disable media      : Disable the specified codec (may be used multiple times)\n"
            "  -P --prefer media       : Prefer the specified codec (may be used multiple times)\n"
            "  -O --option fmt:opt=val : Set codec option (may be used multiple times)\n"
            "                          :  fmt is name of codec, eg \"H.261\"\n"
            "                          :  opt is name of option, eg \"Target Bit Rate\"\n"
            "                          :  val is value of option, eg \"48000\"\n"
            "  --srcep ep              : Set the source endpoint to use for making calls\n"
            "  --disableui             : disable the user interface\n"
            "\n"
            "Audio options:\n"
            "  -j --jitter [min-]max   : Set minimum (optional) and maximum jitter buffer (in milliseconds).\n"
            "  -e --silence            : Disable transmitter silence detection.\n"
            "\n"
#if OPAL_VIDEO
            "Video options:\n"
            "     --rx-video           : Start receiving video immediately.\n"
            "     --tx-video           : Start transmitting video immediately.\n"
            "     --no-rx-video        : Don't start receiving video immediately.\n"
            "     --no-tx-video        : Don't start transmitting video immediately.\n"
            "     --grabber dev        : Set the video grabber device.\n"
            "     --grabdriver dev     : Set the video grabber driver (if device name is ambiguous).\n"
            "     --grabchannel num    : Set the video grabber device channel.\n"
            "     --display dev        : Set the remote video display device.\n"
            "     --displaydriver dev  : Set the remote video display driver (if device name is ambiguous).\n"
            "     --preview dev        : Set the local video preview device.\n"
            "     --previewdriver dev  : Set the local video preview driver (if device name is ambiguous).\n"
            "     --video-size size    : Set the size of the video for all video formats, use\n"
            "                          : \"qcif\", \"cif\", WxH etc\n"
            "     --video-rate rate    : Set the frame rate of video for all video formats\n"
            "     --video-bitrate rate : Set the bit rate for all video formats\n"
            "     -C string            : Enable and select video rate control algorithm\n"
            "\n"
#endif

#if OPAL_SIP
            "SIP options:\n"
            "  -I --no-sip             : Disable SIP protocol.\n"
            "  -r --register-sip host  : Register with SIP server.\n"
            "     --sip-proxy url      : SIP proxy information, may be just a host name\n"
            "                          : or full URL eg sip:user:pwd@host\n"
            "     --sip-listen iface   : Interface/port(s) to listen for SIP requests\n"
            "                          : '*' is all interfaces, (default udp$:*:5060)\n"
            "     --sip-user-agent name: SIP UserAgent name to use.\n"
            "     --sip-ui type        : Set type of user indications to use for SIP. Can be one of 'rfc2833', 'info-tone', 'info-string'.\n"
            "     --use-long-mime      : Use long MIME headers on outgoing SIP messages\n"
            "     --sip-domain str     : set authentication domain/realm\n"
            "\n"
#endif

#if OPAL_H323
            "H.323 options:\n"
            "  -H --no-h323            : Disable H.323 protocol.\n"
#if OPAL_PTLIB_SSL
            "     --no-h323s           : Do not create secure H.323 endpoint\n"
#endif
            "  -g --gatekeeper host    : Specify gatekeeper host, '*' indicates broadcast discovery.\n"
            "  -G --gk-id name         : Specify gatekeeper identifier.\n"
#if OPAL_PTLIB_SSL
            "     --h323s-gk host      : Specify gatekeeper host for secure H.323 endpoint\n"
#endif
            "  -R --require-gatekeeper : Exit if gatekeeper discovery fails.\n"
            "     --gk-token str       : Set gatekeeper security token OID.\n"
            "     --disable-grq        : Do not send GRQ when registering with GK\n"
            "  -b --bandwidth bps      : Limit bandwidth usage to bps bits/second.\n"
            "  -f --fast-disable       : Disable fast start.\n"
            "  -T --h245tunneldisable  : Disable H245 tunnelling.\n"
            "     --h323-listen iface  : Interface/port(s) to listen for H.323 requests\n"
#if OPAL_PTLIB_SSL
            "     --h323s-listen iface : Interface/port(s) to listen for secure H.323 requests\n"
#endif
            "                          : '*' is all interfaces, (default tcp$:*:1720)\n"
#endif

            "\n"
#if OPAL_LID
            "Line Interface options:\n"
            "  -L --no-lid             : Do not use line interface device.\n"
            "     --lid device         : Select line interface device (eg Quicknet:013A17C2, default *:*).\n"
            "     --country code       : Select country to use for LID (eg \"US\", \"au\" or \"+61\").\n"
            "\n"
#endif
            "Sound card options:\n"
            "  -S --no-sound           : Do not use sound input/output device.\n"
            "  -s --sound device       : Select sound input/output device.\n"
            "     --sound-in device    : Select sound input device.\n"
            "     --sound-out device   : Select sound output device.\n"
            "\n"
#if OPAL_IVR
            "IVR options:\n"
            "  -V --no-ivr             : Disable IVR.\n"
            "  -x --vxml file          : Set vxml file to use for IVR.\n"
            "  --tts engine            : Set the text to speech engine\n"
            "\n"
#endif
            "IP options:\n"
            "     --translate ip       : Set external IP address if masqueraded\n"
            "     --portbase n         : Set TCP/UDP/RTP port base\n"
            "     --portmax n          : Set TCP/UDP/RTP port max\n"
            "     --tcp-base n         : Set TCP port base (default 0)\n"
            "     --tcp-max n          : Set TCP port max (default base+99)\n"
            "     --udp-base n         : Set UDP port base (default 6000)\n"
            "     --udp-max n          : Set UDP port max (default base+199)\n"
            "     --rtp-base n         : Set RTP port base (default 5000)\n"
            "     --rtp-max n          : Set RTP port max (default base+199)\n"
            "     --rtp-tos n          : Set RTP packet IP TOS bits to n\n"
	          "     --stun server        : Set STUN server\n"
            "\n"
            "Debug options:\n"
#if PTRACING
            "  -t --trace              : Enable trace, use multiple times for more detail.\n"
            "  -o --output             : File for trace output, default is stderr.\n"
#endif
#if OPAL_IAX2
            "  -X --no-iax2            : Remove support for iax2\n"
            "     --iaxport n          : Set port to use (def. 4569)\n"
#endif
#if OPAL_CAPI
            "     --no-capi            : Remove support for CAPI ISDN\n"
#endif
            "  -h --help               : This help message.\n"
            "\n"
            "\n"
            "Dial peer specification:\n"
"  General form is pattern=destination where pattern is a regular expression\n"
"  matching the incoming calls destination address and will translate it to\n"
"  the outgoing destination address for making an outgoing call. For example,\n"
"  picking up a PhoneJACK handset and dialling 2, 6 would result in an address\n"
"  of \"pots:26\" which would then be matched against, say, a spec of\n"
"  pots:26=h323:10.0.1.1, resulting in a call from the pots handset to\n"
"  10.0.1.1 using the H.323 protocol.\n"
"\n"
"  As the pattern field is a regular expression, you could have used in the\n"
"  above .*:26=h323:10.0.1.1 to achieve the same result with the addition that\n"
"  an incoming call from a SIP client would also be routed to the H.323 client.\n"
"\n"
"  Note that the pattern has an implicit ^ and $ at the beginning and end of\n"
"  the regular expression. So it must match the entire address.\n"
"\n"
"  If the specification is of the form @filename, then the file is read with\n"
"  each line consisting of a pattern=destination dial peer specification. Lines\n"
"  without and equal sign or beginning with '#' are ignored.\n"
"\n"
"  The standard dial peers that will be included are:\n"
"    If SIP is enabled but H.323 & IAX2 are disabled:\n"
"      pots:.*\\*.*\\*.* = sip:<dn2ip>\n"
"      pots:.*         = sip:<da>\n"
"      pc:.*           = sip:<da>\n"
"\n"
"    If SIP & IAX2 are not enabled and H.323 is enabled:\n"
"      pots:.*\\*.*\\*.* = h323:<dn2ip>\n"
"      pots:.*         = h323:<da>\n"
"      pc:.*           = h323:<da>\n"
"\n"
"    If POTS is enabled:\n"
"      h323:.* = pots:<dn>\n"
"      sip:.*  = pots:<dn>\n"
"      iax2:.* = pots:<dn>\n"
"\n"
"    If POTS is not enabled and the PC sound system is enabled:\n"
"      iax2:.* = pc:\n"
"      h323:.* = pc:\n"
"      sip:. * = pc:\n"
"\n"
#if OPAL_IVR
"    If IVR is enabled then a # from any protocol will route it it, ie:\n"
"      .*:#  = ivr:\n"
"\n"
#endif
#if OPAL_IAX2
"    If IAX2 is enabled then you can make a iax2 call with a command like:\n"
"       simpleopal -I -H  iax2:guest@misery.digium.com/s\n"
"           ((Please ensure simplopal is the only iax2 app running on your box))\n"
#endif
            << endl;
    return;
  }

#if PTRACING
  PTrace::Initialise(args.GetOptionCount('t'),
                     args.HasOption('o') ? (const char *)args.GetOptionString('o') : NULL,
                     PTrace::Timestamp|PTrace::Thread|PTrace::FileAndLine);
#endif

  // Create the Opal Manager and initialise it
  opal = new MyManager;

  if (opal->Initialise(args))
    opal->Main(args);

  cout << "Exiting " << GetName() << endl;

  delete opal;
}


///////////////////////////////////////////////////////////////

MyManager::MyManager():
statisticsMainTimer(0,1,0,0,0),
statisticsDualTimer(0,1,0,0,0)
{
#if OPAL_LID
  potsEP = NULL;
#endif
  pcssEP = NULL;

#if OPAL_H323
  h323EP = NULL;
#endif
#if OPAL_SIP
  sipEP  = NULL;
#endif
#if OPAL_IAX2
  iax2EP = NULL;
#endif
#if OPAL_CAPI
  capiEP = NULL;
#endif
#if OPAL_IVR
  ivrEP  = NULL;
#endif
#if OPAL_FAX
  faxEP = NULL;
#endif

  pauseBeforeDialing = PFalse;
//dong motify for tcp/telnet control
  cli = new PCLISocket((WORD)TCP_CTRL_PORT);
  gkIp=PString("");
  e164=PString("");
  alias=PString("");
  remoteName=PString("");
  isGkUpdate =true;
  bandwidth=0;
}


MyManager::~MyManager()
{
#if OPAL_LID
  // Must do this before we destroy the manager or a crash will result
  if (potsEP != NULL)
    potsEP->RemoveAllLines();
#endif
}


PBoolean MyManager::Initialise(PArgList & args)
{
#if OPAL_VIDEO
  // Set the various global options
  if (args.HasOption("rx-video"))
    SetAutoStartReceiveVideo(true);
  if (args.HasOption("no-rx-video"))
    SetAutoStartReceiveVideo(false);
  if (args.HasOption("tx-video"))
    SetAutoStartTransmitVideo(true);
  if (args.HasOption("no-tx-video"))
    SetAutoStartTransmitVideo(false);

  // video input options
  if (args.HasOption("grabber")) {
    PVideoDevice::OpenArgs video = GetVideoInputDevice();
    video.deviceName = args.GetOptionString("grabber");
    video.driverName = args.GetOptionString("grabdriver");
    video.channelNumber = args.GetOptionString("grabchannel").AsInteger();
    if (args.HasOption("video-rate")) 
      video.rate = args.GetOptionString("video-rate").AsUnsigned();
    if (!SetVideoInputDevice(video)) {
      cerr << "Unknown grabber device " << video.deviceName << "\n"
              "Available devices are:" << setfill(',') << PVideoInputDevice::GetDriversDeviceNames("") << endl;
      return PFalse;
    }
  }

  // remote video display options
  if (args.HasOption("display")) {
    PVideoDevice::OpenArgs video = GetVideoOutputDevice();
    video.deviceName = args.GetOptionString("display");
    video.driverName = args.GetOptionString("displaydriver");
    if (!SetVideoOutputDevice(video)) {
      cerr << "Unknown display device " << video.deviceName << "\n"
              "Available devices are:" << setfill(',') << PVideoOutputDevice::GetDriversDeviceNames("") << endl;
      return PFalse;
    }
  }

  // local video preview options
  if (args.HasOption("preview")) {
    PVideoDevice::OpenArgs video = GetVideoPreviewDevice();
    video.deviceName = args.GetOptionString("preview");
    video.driverName = args.GetOptionString("previewdriver");
	if (!SetVideoPreviewDevice(video)) {
      cerr << "Unknown preview device " << video.deviceName << "\n"
              "Available devices are:" << setfill(',') << PVideoOutputDevice::GetDriversDeviceNames("") << endl;
      return PFalse;
    }
  }
#endif

  if (args.HasOption('j')) {
    unsigned minJitter;
    unsigned maxJitter;
    PStringArray delays = args.GetOptionString('j').Tokenise(",-");
    if (delays.GetSize() < 2) {
      maxJitter = delays[0].AsUnsigned();
      minJitter = PMIN(GetMinAudioJitterDelay(), maxJitter);
    }
    else {
      minJitter = delays[0].AsUnsigned();
      maxJitter = delays[1].AsUnsigned();
    }
    if (minJitter >= 20 && minJitter <= maxJitter && maxJitter <= 1000)
      SetAudioJitterDelay(minJitter, maxJitter);
    else {
      cerr << "Jitter should be between 20 and 1000 milliseconds.\n";
      return PFalse;
    }
  }

  silenceDetectParams.m_mode = args.HasOption('e') ? OpalSilenceDetector::NoSilenceDetection
                                                   : OpalSilenceDetector::AdaptiveSilenceDetection;

  if (args.HasOption('D'))
    SetMediaFormatMask(args.GetOptionString('D').Lines());
  if (args.HasOption('P'))
    SetMediaFormatOrder(args.GetOptionString('P').Lines());

  cout << "Jitter buffer: "  << GetMinAudioJitterDelay() << '-' << GetMaxAudioJitterDelay() << " ms\n";

  if (args.HasOption("translate")) {
    SetTranslationAddress(args.GetOptionString("translate"));
    cout << "External address set to " << GetTranslationAddress() << '\n';
  }

  if (args.HasOption("portbase")) {
    unsigned portbase = args.GetOptionString("portbase").AsUnsigned();
    unsigned portmax  = args.GetOptionString("portmax").AsUnsigned();
    SetTCPPorts  (portbase, portmax);
    SetUDPPorts  (portbase, portmax);
    SetRtpIpPorts(portbase, portmax);
  } else {
    if (args.HasOption("tcp-base"))
      SetTCPPorts(args.GetOptionString("tcp-base").AsUnsigned(),
                  args.GetOptionString("tcp-max").AsUnsigned());

    if (args.HasOption("udp-base"))
      SetUDPPorts(args.GetOptionString("udp-base").AsUnsigned(),
                  args.GetOptionString("udp-max").AsUnsigned());

    if (args.HasOption("rtp-base"))
      SetRtpIpPorts(args.GetOptionString("rtp-base").AsUnsigned(),
                    args.GetOptionString("rtp-max").AsUnsigned());
  }

  if (args.HasOption("rtp-tos")) {
    unsigned tos = args.GetOptionString("rtp-tos").AsUnsigned();
    if (tos > 255) {
      cerr << "IP Type Of Service bits must be 0 to 255.\n";
      return PFalse;
    }
    SetMediaTypeOfService(tos);
  }

  cout << "TCP ports: " << GetTCPPortBase() << '-' << GetTCPPortMax() << "\n"
          "UDP ports: " << GetUDPPortBase() << '-' << GetUDPPortMax() << "\n"
          "RTP ports: " << GetRtpIpPortBase() << '-' << GetRtpIpPortMax() << "\n"
          "RTP IP TOS: 0x" << hex << (unsigned)GetMediaTypeOfService() << dec << "\n"
          "STUN server: " << flush;

  if (args.HasOption("stun"))
    SetSTUNServer(args.GetOptionString("stun"));

  if (stun != NULL)
    cout << stun->GetServer() << " replies " << stun->GetNatTypeName();
  else
    cout << "None";
  cout << '\n';

  OpalMediaFormatList allMediaFormats;

  ///////////////////////////////////////
  // Open the LID if parameter provided, create LID based endpoint
#if OPAL_LID
  if (!args.HasOption('L')) {
    PStringArray devices = args.GetOptionString("lid").Lines();
    if (devices.IsEmpty() || devices[0] == "*" || devices[0] == "*:*")
      devices = OpalLineInterfaceDevice::GetAllDevices();
    for (PINDEX d = 0; d < devices.GetSize(); d++) {
      PINDEX colon = devices[d].Find(':');
      OpalLineInterfaceDevice * lid = OpalLineInterfaceDevice::Create(devices[d].Left(colon));
      if (lid->Open(devices[d].Mid(colon+1).Trim())) {
        if (args.HasOption("country")) {
          PString country = args.GetOptionString("country");
          if (!lid->SetCountryCodeName(country))
            cerr << "Could not set LID to country name \"" << country << '"' << endl;
        }

        // Create LID protocol handler, automatically adds to manager
        if (potsEP == NULL)
          potsEP = new OpalLineEndPoint(*this);
        if (potsEP->AddDevice(lid)) {
          cout << "Line interface device \"" << devices[d] << "\" added." << endl;
          allMediaFormats += potsEP->GetMediaFormats();
        }
      }
      else {
        cerr << "Could not open device \"" << devices[d] << '"' << endl;
        delete lid;
      }
    }
  }
#endif

  ///////////////////////////////////////
  // Create PC Sound System handler

  if (!args.HasOption('S')) {
    pcssEP = new MyPCSSEndPoint(*this);

    pcssEP->autoAnswer = args.HasOption('a');
    cout << "Auto answer is " << (pcssEP->autoAnswer ? "on" : "off") << "\n";
          
    if (!pcssEP->SetSoundDevice(args, "sound", PSoundChannel::Recorder))
      return PFalse;
    if (!pcssEP->SetSoundDevice(args, "sound", PSoundChannel::Player))
      return PFalse;
    if (!pcssEP->SetSoundDevice(args, "sound-in", PSoundChannel::Recorder))
      return PFalse;
    if (!pcssEP->SetSoundDevice(args, "sound-out", PSoundChannel::Player))
      return PFalse;

    allMediaFormats += pcssEP->GetMediaFormats();

    cout << "Sound output device: \"" << pcssEP->GetSoundChannelPlayDevice() << "\"\n"
            "Sound  input device: \"" << pcssEP->GetSoundChannelRecordDevice() << "\"\n"
#if OPAL_VIDEO
			"Video preview device: \"" << GetVideoPreviewDevice().deviceName << "\"\n"
            "Video output device: \"" << GetVideoOutputDevice().deviceName << "\"\n"
            "Video  input device: \"" << GetVideoInputDevice().deviceName << '"'
#endif
         << endl;
  }

#if OPAL_H323

  ///////////////////////////////////////
  // Create H.323 protocol handler
  if (!args.HasOption("no-h323")) {
    h323EP = new H323EndPoint(*this);
    if (!InitialiseH323EP(args, false, h323EP))
      return PFalse;
  }

#endif

#if OPAL_IAX2
  ///////////////////////////////////////
  // Create IAX2 protocol handler

  if (!args.HasOption("no-iax2")) {
    PINDEX port = 0;
    if (args.HasOption("iaxport"))
      port = args.GetOptionString("iaxport").AsInteger();
    cerr << " port is " << port << endl;

    if (port > 0)
      iax2EP = new IAX2EndPoint(*this, port);
    else
      iax2EP = new IAX2EndPoint(*this);
    
    if (!iax2EP->InitialisedOK()) {
      cerr << "IAX2 Endpoint is not initialised correctly" << endl;
      cerr << "Is there another application using the iax port? " << endl;
      return PFalse;
    }

    if (args.HasOption('p'))
      iax2EP->SetPassword(args.GetOptionString('p'));
    
    if (args.HasOption('u')) {
      PStringArray aliases = args.GetOptionString('u').Lines();
      iax2EP->SetLocalUserName(aliases[0]);
    }
  }
#endif

#if OPAL_CAPI
  ///////////////////////////////////////
  // Create CAPI handler

  if (!args.HasOption("no-capi")) {
    capiEP = new OpalCapiEndPoint(*this);
    
    if (!capiEP->OpenControllers())
      cerr << "CAPI Endpoint failed to initialise any controllers." << endl;
  }
#endif

#if OPAL_SIP

  ///////////////////////////////////////
  // Create SIP protocol handler

  if (!args.HasOption("no-sip")) {
    sipEP = new SIPEndPoint(*this);

    if (args.HasOption("sip-user-agent"))
      sipEP->SetUserAgent(args.GetOptionString("sip-user-agent"));

    PString str = args.GetOptionString("sip-ui");
    if (str *= "rfc2833")
      sipEP->SetSendUserInputMode(OpalConnection::SendUserInputAsRFC2833);
    else if (str *= "info-tone")
      sipEP->SetSendUserInputMode(OpalConnection::SendUserInputAsTone);
    else if (str *= "info-string")
      sipEP->SetSendUserInputMode(OpalConnection::SendUserInputAsString);

    if (args.HasOption("sip-proxy"))
      sipEP->SetProxy(args.GetOptionString("sip-proxy"));

    // set MIME format
    sipEP->SetMIMEForm(args.HasOption("use-long-mime"));

    // Get local username, multiple uses of -u indicates additional aliases
    if (args.HasOption('u')) {
      PStringArray aliases = args.GetOptionString('u').Lines();
      sipEP->SetDefaultLocalPartyName(aliases[0]);
    }

    // Start the listener thread for incoming calls.
    PStringArray listeners = args.GetOptionString("sip-listen").Lines();
    if (!sipEP->StartListeners(listeners)) {
      cerr <<  "Could not open any SIP listener from "
            << setfill(',') << listeners << endl;
      return PFalse;
    }
    cout <<  "SIP started on " << setfill(',') << sipEP->GetListeners() << setfill(' ') << endl;

    if (args.HasOption('r')) {
      SIPRegister::Params params;
      params.m_registrarAddress = args.GetOptionString('r');
      params.m_addressOfRecord = args.GetOptionString('u');
      params.m_password = args.GetOptionString('p');
      params.m_realm = args.GetOptionString("sip-domain");
      PString aor;
      if (sipEP->Register(params, aor))
        cout << "Using SIP registrar " << params.m_registrarAddress << " for " << aor << endl;
      else
        cout << "Could not use SIP registrar " << params.m_registrarAddress << endl;
      pauseBeforeDialing = PTrue;
    }
  }

#endif


#if OPAL_IVR
  ///////////////////////////////////////
  // Create IVR protocol handler

  if (!args.HasOption('V')) {
    ivrEP = new OpalIVREndPoint(*this);
    if (args.HasOption('x'))
      ivrEP->SetDefaultVXML(args.GetOptionString('x'));

    allMediaFormats += ivrEP->GetMediaFormats();

    PString ttsEngine = args.GetOptionString("tts");
    if (ttsEngine.IsEmpty() && PFactory<PTextToSpeech>::GetKeyList().size() > 0) 
      ttsEngine = PFactory<PTextToSpeech>::GetKeyList()[0];
    if (!ttsEngine.IsEmpty()) 
      ivrEP->SetDefaultTextToSpeech(ttsEngine);
  }
#endif

#if OPAL_FAX
  ///////////////////////////////////////
  // Create T38 protocol handler
  {
    OpalMediaFormat fmt(OpalT38); // Force instantiation of T.38 media format
    faxEP = new OpalFaxEndPoint(*this);
    allMediaFormats += faxEP->GetMediaFormats();
  }
#endif

  ///////////////////////////////////////
  // Set the dial peers

  if (args.HasOption('d')) {
    if (!SetRouteTable(args.GetOptionString('d').Lines())) {
      cerr <<  "No legal entries in dial peer!" << endl;
      return PFalse;
    }
  }

  if (!args.HasOption("no-std-dial-peer")) {
#if OPAL_IVR
    // Need to make sure wildcard on source ep type is first or it won't be
    // selected in preference to the specific entries below
    if (ivrEP != NULL)
      AddRouteEntry(".*:#  = ivr:"); // A hash from anywhere goes to IVR
#endif

#if OPAL_SIP
    if (sipEP != NULL) {
#if OPAL_FAX
      AddRouteEntry("t38:.*             = sip:<da>");
      AddRouteEntry("sip:.*\tfax@.*     = t38:received_fax_%s.tif;receive");
      AddRouteEntry("sip:.*\tsip:329@.* = t38:received_fax_%s.tif;receive");
#endif
      AddRouteEntry("pots:.*\\*.*\\*.*  = sip:<dn2ip>");
      AddRouteEntry("pots:.*            = sip:<da>");
      AddRouteEntry("pc:.*              = sip:<da>");
    }
#endif

#if OPAL_H323
    if (h323EP != NULL) {
      AddRouteEntry("pots:.*\\*.*\\*.* = h323:<dn2ip>");
      AddRouteEntry("pots:.*           = h323:<da>");
      AddRouteEntry("pc:.*             = h323:<da>");
#if OPAL_PTLIB_SSL
      {
        AddRouteEntry("pots:.*\\*.*\\*.* = h323s:<dn2ip>");
        AddRouteEntry("pots:.*           = h323s:<da>");
        AddRouteEntry("pc:.*             = h323s:<da>");
      }
#endif
    }
#endif

#if OPAL_LID
    if (potsEP != NULL) {
#if OPAL_H323
      AddRouteEntry("h323:.* = pots:<du>");
#if OPAL_PTLIB_SSL
      //if (h323sEP != NULL) 
        AddRouteEntry("h323s:.* = pots:<du>");
#endif
#endif
#if OPAL_SIP
      AddRouteEntry("sip:.*  = pots:<du>");
#endif
    }
    else
#endif // OPAL_LID
    if (pcssEP != NULL) {
#if OPAL_H323
      AddRouteEntry("h323:.* = pc:");
#if OPAL_PTLIB_SSL
      //if (h323sEP != NULL) 
        AddRouteEntry("h323s:.* = pc:");
#endif
#endif
#if OPAL_SIP
      AddRouteEntry("sip:.*  = pc:");
#endif
    }
  }
                                                                                                                                            
#if OPAL_IAX2
  if (pcssEP != NULL) {
    AddRouteEntry("iax2:.* = pc:");
    AddRouteEntry("pc:.*   = iax2:<da>");
  }
#endif

#if OPAL_CAPI
  if (capiEP != NULL) {
    AddRouteEntry("isdn:.* = pc:");
    AddRouteEntry("pc:.*   = isdn:<da>");
  }
#endif

#if OPAL_FAX
  if (faxEP != NULL) {
    AddRouteEntry("sip:.*  = t38:<da>");
    AddRouteEntry("sip:.*  = fax:<da>");
  }
#endif

  PString defaultSrcEP = pcssEP != NULL ? "pc:*"
                                      #if OPAL_LID
                                        : potsEP != NULL ? "pots:*"
                                      #endif
                                      #if OPAL_IVR
                                        : ivrEP != NULL ? "ivr:#"
                                      #endif
                                      #if OPAL_SIP
                                        : sipEP != NULL ? "sip:localhost"
                                      #endif
                                      #if OPAL_H323
                                        : h323EP != NULL ? "sip:localhost"
                                      #endif
                                        : "";
  srcEP = args.GetOptionString("srcep", defaultSrcEP);

  if (FindEndPoint(srcEP.Left(srcEP.Find(':'))) == NULL)
    srcEP = defaultSrcEP;

  allMediaFormats = OpalTranscoder::GetPossibleFormats(allMediaFormats); // Add transcoders
  for (PINDEX i = 0; i < allMediaFormats.GetSize(); i++) {
    if (!allMediaFormats[i].IsTransportable())
      allMediaFormats.RemoveAt(i--); // Don't show media formats that are not used over the wire
  }
  allMediaFormats.Remove(GetMediaFormatMask());
  allMediaFormats.Reorder(GetMediaFormatOrder());

  cout << "Local endpoint type: " << srcEP << "\n"
          "Codecs removed: " << setfill(',') << GetMediaFormatMask() << "\n"
          "Codec order: " << GetMediaFormatOrder() << "\n"
          "Available codecs: " << allMediaFormats << setfill(' ') << endl;

#if OPAL_VIDEO
  PString rcOption = args.GetOptionString('C');
  OpalMediaFormat::GetAllRegisteredMediaFormats(allMediaFormats);
  for (PINDEX i = 0; i < allMediaFormats.GetSize(); i++) {
    OpalMediaFormat mediaFormat = allMediaFormats[i];
    if (mediaFormat.GetMediaType() == OpalMediaType::Video()) {
      if (args.HasOption("video-size")) {
        PString sizeStr = args.GetOptionString("video-size");
        unsigned width, height;
        if (PVideoFrameInfo::ParseSize(sizeStr, width, height)) {
            mediaFormat.SetOptionInteger(OpalVideoFormat::FrameWidthOption(), width);
            mediaFormat.SetOptionInteger(OpalVideoFormat::FrameHeightOption(), height);
        }
        else
          cerr << "Unknown video size \"" << sizeStr << '"' << endl;
      }

      if (args.HasOption("video-rate")) {
        unsigned rate = args.GetOptionString("video-rate").AsUnsigned();
        unsigned frameTime = 90000 / rate;
        mediaFormat.SetOptionInteger(OpalMediaFormat::FrameTimeOption(), frameTime);
		//dong motify for tcp/telnet control
		mediaFormat.SetOptionInteger(OpalMediaFormat::FrameRateOption(), rate);
      }
      if (args.HasOption("video-bitrate")) {
        unsigned rate = args.GetOptionString("video-bitrate").AsUnsigned();
        mediaFormat.SetOptionInteger(OpalMediaFormat::TargetBitRateOption(), rate);
      }
      if (!rcOption.IsEmpty())
        mediaFormat.SetOptionString(OpalVideoFormat::RateControllerOption(), rcOption);
      OpalMediaFormat::SetRegisteredMediaFormat(mediaFormat);
    }
  }
#endif

  PStringArray options = args.GetOptionString('O').Lines();
  for (PINDEX i = 0; i < options.GetSize(); i++) {
    const PString & optionDescription = options[i];
    PINDEX colon = optionDescription.Find(':');
    PINDEX equal = optionDescription.Find('=', colon+2);
    if (colon == P_MAX_INDEX || equal == P_MAX_INDEX) {
      cerr << "Invalid option description \"" << optionDescription << '"' << endl;
      continue;
    }
    OpalMediaFormat mediaFormat = optionDescription.Left(colon);
    if (mediaFormat.IsEmpty()) {
      cerr << "Invalid media format in option description \"" << optionDescription << '"' << endl;
      continue;
    }
    PString optionName = optionDescription(colon+1, equal-1);
    if (!mediaFormat.HasOption(optionName)) {
      cerr << "Invalid option name in description \"" << optionDescription << '"' << endl;
      continue;
    }
    PString valueStr = optionDescription.Mid(equal+1);
    if (!mediaFormat.SetOptionValue(optionName, valueStr)) {
      cerr << "Invalid option value in description \"" << optionDescription << '"' << endl;
      continue;
    }
    OpalMediaFormat::SetRegisteredMediaFormat(mediaFormat);
    cout << "Set option \"" << optionName << "\" to \"" << valueStr << "\" in \"" << mediaFormat << '"' << endl;
  }

#if PTRACING
  allMediaFormats = OpalMediaFormat::GetAllRegisteredMediaFormats();
  ostream & traceStream = PTrace::Begin(3, __FILE__, __LINE__);
  traceStream << "Simple\tRegistered media formats:\n";
  for (PINDEX i = 0; i < allMediaFormats.GetSize(); i++)
    allMediaFormats[i].PrintOptions(traceStream);
  traceStream << PTrace::End;
#endif

  statisticsMainTimer.SetNotifier(PCREATE_NOTIFIER(setMainStatistic));
  statisticsDualTimer.SetNotifier(PCREATE_NOTIFIER(setDualStatistic));
  return PTrue;
}

#if OPAL_H323

PBoolean MyManager::InitialiseH323EP(PArgList & args, PBoolean secure, H323EndPoint * h323EP)
{
  h323EP->DisableFastStart(args.HasOption('f'));
  h323EP->DisableH245Tunneling(args.HasOption('T'));
  h323EP->SetSendGRQ(!args.HasOption("disable-grq"));


  // Get local username, multiple uses of -u indicates additional aliases
  if (args.HasOption('u')) {
    //dong add for multipoint test
	  /*-l -a -I --no-sip --video-bitrate 7936000 --video-size HD1080 --video-rate 59 -b 8000000 -e -t -o update.txt -g 192.168.1.167 -u 010666x010777x010888x010999*/
	  PStringArray aliases = args.GetOptionString('u').Tokenise("x");
   // PStringArray aliases = args.GetOptionString('u').Lines();
    h323EP->SetLocalUserName(aliases[0]);
    for (PINDEX i = 1; i < aliases.GetSize(); i++)
      h323EP->AddAliasName(aliases[i]);
  }

  if (args.HasOption('b')) {
    unsigned initialBandwidth = args.GetOptionString('b').AsUnsigned()/100;
    if (initialBandwidth == 0) {
      cerr << "Illegal bandwidth specified." << endl;
      return PFalse;
    }
    h323EP->SetInitialBandwidth(initialBandwidth);
  }

  h323EP->SetGkAccessTokenOID(args.GetOptionString("gk-token"));

  PString prefix = h323EP->GetPrefixName();

  cout << prefix << " Local username: " << h323EP->GetLocalUserName() << "\n"
       << prefix << " FastConnect is " << (h323EP->IsFastStartDisabled() ? "off" : "on") << "\n"
       << prefix << " H245Tunnelling is " << (h323EP->IsH245TunnelingDisabled() ? "off" : "on") << "\n"
       << prefix << " gk Token OID is " << h323EP->GetGkAccessTokenOID() << endl;


  // Start the listener thread for incoming calls.
  PStringArray  listeners = args.GetOptionString(secure ? "h323s-listen" : "h323-listen").Lines();
  if (!h323EP->StartListeners(listeners)) {
    cerr <<  "Could not open any " << prefix << " listener from "
         << setfill(',') << listeners << endl;
    return PFalse;
  }
  cout << prefix << " listeners: " << setfill(',') << h323EP->GetListeners() << setfill(' ') << endl;


  if (args.HasOption('p'))
    h323EP->SetGatekeeperPassword(args.GetOptionString('p'));

  // Establish link with gatekeeper if required.
  if (args.HasOption(secure ? "h323s-gk" : "gatekeeper")) {
    PString gkHost      = args.GetOptionString(secure ? "h323s-gk" : "gatekeeper");
    if (gkHost == "*")
      gkHost = PString::Empty();
    PString gkIdentifer = args.GetOptionString('G');
    PString gkInterface = args.GetOptionString(secure ? "h323s-listen" : "h323-listen");
    cout << "Gatekeeper: " << flush;
    if (h323EP->UseGatekeeper(gkHost, gkIdentifer, gkInterface))
      cout << *h323EP->GetGatekeeper() << endl;
    else {
      cout << "none." << endl;
      cerr << "Could not register with gatekeeper";
      if (!gkIdentifer)
        cerr << " id \"" << gkIdentifer << '"';
      if (!gkHost)
        cerr << " at \"" << gkHost << '"';
      if (!gkInterface)
        cerr << " on interface \"" << gkInterface << '"';
      if (h323EP->GetGatekeeper() != NULL) {
        switch (h323EP->GetGatekeeper()->GetRegistrationFailReason()) {
          case H323Gatekeeper::InvalidListener :
            cerr << " - Invalid listener";
            break;
          case H323Gatekeeper::DuplicateAlias :
            cerr << " - Duplicate alias";
            break;
          case H323Gatekeeper::SecurityDenied :
            cerr << " - Security denied";
            break;
          case H323Gatekeeper::TransportError :
            cerr << " - Transport error";
            break;
          default :
            cerr << " - Error code " << h323EP->GetGatekeeper()->GetRegistrationFailReason();
        }
      }
      cerr << '.' << endl;
      if (args.HasOption("require-gatekeeper")) 
        return PFalse;
    }
  }
  return PTrue;
}

#endif  //OPAL_H323


#if OPAL_PTLIB_CONFIG_FILE
void MyManager::NewSpeedDial(const PString & ostr)
{
  PString str = ostr;
  PINDEX idx = str.Find(' ');
  if (str.IsEmpty() || (idx == P_MAX_INDEX)) {
    cout << "Must specify speedial number and string" << endl;
    return;
  }
 
  PString key  = str.Left(idx).Trim();
  PString data = str.Mid(idx).Trim();
 
  PConfig config("Speeddial");
  config.SetString(key, data);
 
  cout << "Speedial " << key << " set to " << data << endl;
}
#endif // OPAL_PTLIB_CONFIG_FILE
 

void MyManager::Main(PArgList & args)
{
  // See if making a call or just listening.
  switch (args.GetCount()) {
    case 0 :
      cout << "Waiting for incoming calls\n";
      break;

    case 1 :
      if (pauseBeforeDialing) {
        cout << "Pausing to allow registration to occur..." << flush;
        PThread::Sleep(2000);
        cout << "done" << endl;
      }

      cout << "Initiating call to \"" << args[0] << "\"\n";
      SetUpCall(srcEP, args[0], currentCallToken);
      break;

    default :
      if (pauseBeforeDialing) {
        cout << "Pausing to allow registration to occur..." << flush;
        PThread::Sleep(2000);
        cout << "done" << endl;
      }
      cout << "Initiating call from \"" << args[0] << "\"to \"" << args[1] << "\"\n";
      SetUpCall(args[0], args[1], currentCallToken);
      break;
  }

  if (args.HasOption("disableui")) {
    while (FindCallWithLock(currentCallToken) != NULL) 
      PThread::Sleep(1000);
  }
  else {
    cout << "Press ? for help." << endl;

    PStringStream help;

    help << "Select:\n"
            "  0-9 : send user indication message\n"
            "  *,# : send user indication message\n"
            "  M   : send text message to remote user\n"
            "  C   : Connect to remote host\n"
            "  S   : Display statistics\n"
            "  O   : Put call on hold\n"
            "  T   : Transfer remote to new destination or held call\n"
            "  H   : Hang up phone\n"
            "  L   : List speed dials\n"
            "  D   : Create new speed dial\n"
            "  {}  : Increase/reduce record volume\n"
            "  []  : Increase/reduce playback volume\n"
      "  V   : Display current volumes\n";
	}
//dong motify for tcp/telnet control
	cli->StartContext(new PConsoleChannel(PConsoleChannel::StandardInput),
		new PConsoleChannel(PConsoleChannel::StandardOutput));
	//cli->SetPrompt("SDT_Terminal> ");
	cli->SetCommand("heartbeat", PCREATE_NOTIFIER(HeartBeat),
		"Get Heartbeat Of The Terminal", ": just heartbeat\n(e.g. heartbeat)");
	cli->SetCommand("call", PCREATE_NOTIFIER(CallRemote),
		"Call Remote Terminal","(ip e.g. 192.168.1.2) or (e164 e.g. 010122)\n(e.g. call 192.168.1.2)\n");
	cli->SetCommand("hang", PCREATE_NOTIFIER(HangupCurrentCall),
		"Hangup Current Call",": hang up current call\n(e.g. hang)\n");
	cli->SetCommand("dual", PCREATE_NOTIFIER(StartH239Connnection),
		"Start Dual Stream",": start H239 connnection\n(e.g. dual)\n");
	cli->SetCommand("stop", PCREATE_NOTIFIER(StopH239Connnection),
		"Stop Dual Stream",": stop H239 Connnection\n(e.g. stop)\n");
	cli->SetCommand("update", PCREATE_NOTIFIER(UpdatePicture),
		"Update [Remote or Local] [Main or Dual] Stream","[(remote) or (local)] [(main) or (dual)]\n(e.g. update local main)\n");
	cli->SetCommand("bandwidth", PCREATE_NOTIFIER(SetBandWidth),
		"Set [Total or Main or Dual] BandWidth","[(total) or (main)or(dual)] [data] Kb\ne.g.bandwidth total 800\n");
	cli->SetCommand("getbandwidth", PCREATE_NOTIFIER(GetBandWidth),
		"Get Total and Main and Dual BandWidth",": just type getbandwidth\n");
	cli->SetCommand("layout", PCREATE_NOTIFIER(SetLayout),
		"Set the Layout [num]",": layout [num]\n");
	cli->SetCommand("gk", PCREATE_NOTIFIER(SetGK),
		"set gatekeeper",": gk [on/off] [ip]\n");
	cli->SetCommand("e164", PCREATE_NOTIFIER(SetE164),
		"set E164",": e164 [e164 string] \n");
	cli->SetCommand("alias", PCREATE_NOTIFIER(SetAlias),
		"set Alias or UserName",": alias [alias string] \n");
	cli->SetCommand("statistics", PCREATE_NOTIFIER(GetStatistics),
		"Get Statistics",": statistics\n");
	

	cli->Start(false); // Do not spawn thread, wait till end of input
	cout << "\nExiting ..." << endl;
    /*for (;;) {
      // display the prompt
      cout << "Command ? " << flush;
       
       
      // terminate the menu loop if console finished
      char ch = (char)console.peek();
      if (console.eof()) {
        cout << "\nConsole gone - menu disabled" << endl;
        goto endSimpleOPAL;
      }
       
      PString line;
      console >> line;
      line = line.LeftTrim();
      ch = line[0];
      line = line.Mid(1).Trim();

      PTRACE(3, "console in audio test is " << ch);
      switch (tolower(ch)) {
      case 'x' :
      case 'q' :
        goto endSimpleOPAL;

      case '?' :       
        cout << help ;
        break;

#if OPAL_HAS_MIXER
      case 'z':
        if (currentCallToken.IsEmpty())
         cout << "Cannot stop or start record whilst no call in progress.\n";
        else if (ch == 'z') {
          StartRecording(currentCallToken, "record.wav");
          cout << "Recording started.\n";
        }
        else {
          StopRecording(currentCallToken);
          cout << "Recording stopped.\n";
        }
        break;
#endif
        
      case 'y' :
        if ( pcssEP != NULL &&
            !pcssEP->incomingConnectionToken &&
            !pcssEP->AcceptIncomingConnection(pcssEP->incomingConnectionToken))
          cout << "Could not answer connection " << pcssEP->incomingConnectionToken << endl;
        break;

      case 'n' :
        if ( pcssEP != NULL &&
            !pcssEP->incomingConnectionToken &&
            !pcssEP->RejectIncomingConnection(pcssEP->incomingConnectionToken))
          cout << "Could not reject connection " << pcssEP->incomingConnectionToken << endl;
        break;

      case 'b' :
        if ( pcssEP != NULL &&
            !pcssEP->incomingConnectionToken &&
            !pcssEP->RejectIncomingConnection(pcssEP->incomingConnectionToken, OpalConnection::EndedByLocalBusy))
          cout << "Could not reject connection " << pcssEP->incomingConnectionToken << endl;
        break;

#if OPAL_PTLIB_CONFIG_FILE
      case 'l' :
        ListSpeedDials();
        break;
 
      case 'd' :
        NewSpeedDial(line);
        break;
#endif // OPAL_PTLIB_CONFIG_FILE
        
      case 'h' :
        HangupCurrentCall();
        break;

      case 'c' :
        StartCall(line);
        break;

      case 'o' :
        HoldRetrieveCall();
        break;

      case 't' :
        TransferCall(line);
        break;

      case 'r':
        cout << "Current call token is \"" << currentCallToken << "\"\n";
        if (!heldCallToken.IsEmpty())
          cout << "Held call token is \"" << heldCallToken << "\"\n";
        break;

      case 'm' :
        SendMessageToRemoteNode(line);
        break;

      case 'f' :
        SendTone('x');
        break;

      default:
        if (isdigit(ch) || ch == '*' || ch == '#')
          SendTone(ch);
        break;
      }
    }
  endSimpleOPAL:
    if (!currentCallToken.IsEmpty())
      HangupCurrentCall();
  }

  cout << "Console finished " << endl;;*/
}

void MyManager::HangupCurrentCall()
{
  PString & token = currentCallToken.IsEmpty() ? heldCallToken : currentCallToken;

  PSafePtr<OpalCall> call = FindCallWithLock(token);
  if (call == NULL)
    cout << "No call to hang up!\n";
  else {
    cout << "Clearing call " << *call << endl;
    call->Clear();
    token.MakeEmpty();
  }
}


void MyManager::HoldRetrieveCall()
{
  if (currentCallToken.IsEmpty() && heldCallToken.IsEmpty()) {
    cout << "Cannot do hold while no call in progress\n";
    return;
  }

  if (heldCallToken.IsEmpty()) {
    PSafePtr<OpalCall> call = FindCallWithLock(currentCallToken);
    if (call == NULL)
      cout << "Current call disappeared!\n";
    else if (call->Hold()) {
      cout << "Call held.\n";
      heldCallToken = currentCallToken;
      currentCallToken.MakeEmpty();
    }
  }
  else {
    PSafePtr<OpalCall> call = FindCallWithLock(heldCallToken);
    if (call == NULL)
      cout << "Held call disappeared!\n";
    else if (call->Retrieve()) {
      cout << "Call retrieved.\n";
      currentCallToken = heldCallToken;
      heldCallToken.MakeEmpty();
    }
  }
}


void MyManager::TransferCall(const PString & dest)
{
  if (currentCallToken.IsEmpty()) {
    cout << "Cannot do transfer while no call in progress\n";
    return;
  }

  if (dest.IsEmpty() && heldCallToken.IsEmpty()) {
    cout << "Must supply a destination for transfer, or have a call on hold!\n";
    return;
  }

  PSafePtr<OpalCall> call = FindCallWithLock(currentCallToken);
  if (call == NULL) {
    cout << "Current call disappeared!\n";
    return;
  }

  for (PSafePtr<OpalConnection> connection = call->GetConnection(0); connection != NULL; ++connection) {
    if (PIsDescendant(&(*connection), OpalPCSSConnection))
      break;
#if OPAL_LID
    if (PIsDescendant(&(*connection), OpalLineConnection))
      break;
#endif
    connection->TransferConnection(dest.IsEmpty() ? heldCallToken : dest);
    break;
  }
}


void MyManager::SendMessageToRemoteNode(const PString & str)
{
  if (str.IsEmpty()) {
    cout << "Must supply a message to send!\n";
    return;
  }

  PSafePtr<OpalCall> call = FindCallWithLock(currentCallToken);
  if (call == NULL) {
    cout << "Cannot send a message while no call in progress\n";
    return;
  }

  PSafePtr<OpalConnection> conn = call->GetConnection(0);
  while (conn != NULL) {
    conn->SendUserInputString(str);
    cout << "Sent \"" << str << "\" to " << conn->GetRemotePartyName() << endl;
    ++conn;
  }
}

void MyManager::SendTone(const char tone)
{
  if (currentCallToken.IsEmpty()) {
    cout << "Cannot send a digit while no call in progress\n";
    return;
  }

  PSafePtr<OpalCall> call = FindCallWithLock(currentCallToken);
  if (call == NULL) {
    cout << "Cannot send a message while no call in progress\n";
    return;
  }

  PSafePtr<OpalConnection> conn = call->GetConnection(0);
  while (conn != NULL) {
    conn->SendUserInputTone(tone, 180);
    cout << "Sent \"" << tone << "\" to " << conn->GetRemotePartyName() << endl;
    ++conn;
  }
}


void MyManager::StartCall(const PString & dest)
{
  if (!currentCallToken.IsEmpty()) {
    cout << "Cannot make call whilst call in progress\n";
    return;
  }

  if (dest.IsEmpty()) {
    cout << "Must supply hostname to connect to!\n";
    return ;
  }

  PString str = dest;

#if OPAL_PTLIB_CONFIG_FILE
  // check for speed dials, and match wild cards as we go
  PString key, prefix;
  if ((str.GetLength() > 1) && (str[str.GetLength()-1] == '#')) {
 
    key = str.Left(str.GetLength()-1).Trim(); 
    str = PString();
    PConfig config("Speeddial");
    PINDEX p;
    for (p = key.GetLength(); p > 0; p--) {
 
      PString newKey = key.Left(p);
      prefix = newKey;
      PINDEX q;
 
      // look for wild cards
      str = config.GetString(newKey + '*').Trim();
      if (!str.IsEmpty())
        break;
 
      // look for digit matches
      for (q = p; q < key.GetLength(); q++)
        newKey += '?';
      str = config.GetString(newKey).Trim();
      if (!str.IsEmpty())
        break;
    }
    if (str.IsEmpty())
      cout << "Speed dial \"" << key << "\" not defined\n";
  }
#endif // OPAL_PTLIB_CONFIG_FILE

  if (!str.IsEmpty())
    SetUpCall(srcEP, str, currentCallToken);

  return;
}

#if OPAL_PTLIB_CONFIG_FILE
void MyManager::ListSpeedDials()
{
  PConfig config("Speeddial");
  PStringList keys = config.GetKeys();
  if (keys.GetSize() == 0) {
    cout << "No speed dials defined\n";
    return;
  }
 
  PINDEX i;
  for (i = 0; i < keys.GetSize(); i++)
    cout << keys[i] << ":   " << config.GetString(keys[i]) << endl;
}
#endif // OPAL_PTLIB_CONFIG_FILE

void MyManager::OnEstablishedCall(OpalCall & call)
{
  currentCallToken = call.GetToken();
  //dong backup below
  /*cout << "In call with " << call.GetPartyB() << " using " << call.GetPartyA() << endl; */ 
//dong motify for tcp/telnet control
  PStringStream strm;
  strm  << "\ncall " << call.GetPartyB() << " or " << call.GetPartyA() <<" ok"<< endl;
  cli->Broadcast(strm);
  SetLayoutInternal(LayoutType::nodualCallSuccess);
  setRemoteName();
  statisticsMainTimer.RunContinuous(1000);
}
//dong add for dual notify
void MyManager::OnDualEstablished(OpalConnection & connection)
{
	SetLayoutInternal(LayoutType::dualCallSuccess);
	statisticsDualTimer.RunContinuous(1000);
}
void MyManager::OnClosedDualMediaStream( const OpalMediaStream & stream )
{
	SetLayoutInternal(LayoutType::nodualCallSuccess);
	statisticsDualTimer.Stop(true);
	statisticsMyH239Encode.stop();
	statisticsMyH239Decode.stop();
}
//dong add for gk
void MyManager::OnGKRegisterSuccess(PString _gkIp)
{
	if( (gkIp.IsEmpty() || gkIp !=_gkIp) || isGkUpdate)
	{
		isGkUpdate=false;
		gkIp = _gkIp;
		PStringStream strm;
		strm  << "gk "<<gkIp<<" register success"<< endl;
		cli->Broadcast(strm);
	}
}
void MyManager::OnGKRegisterFail()
{
	PStringStream strm;
	strm  << "gk register fail"<< endl;
	cli->Broadcast(strm);
}
void MyManager::OnGKUnRegisterSuccess()
{
	gkIp.MakeEmpty();
	PStringStream strm;
	strm  << "\ngk unregister success"<< endl;
	cli->Broadcast(strm);
}

void MyManager::OnClearedCall(OpalCall & call)
{
  if (currentCallToken == call.GetToken())
    currentCallToken.MakeEmpty();
  else if (heldCallToken == call.GetToken())
    heldCallToken.MakeEmpty();
//dong motify for tcp/telnet control
  PStringStream strm;
  strm  << "hang  ok"<< endl;
  cli->Broadcast(strm);
  SetLayoutInternal(LayoutType::Hang);
  statisticsMainTimer.Stop(true);
  remoteName.MakeEmpty();
  bandwidth=0;
  statisticsMyAudioEncode.stop();
  statisticsMyAudioDecode.stop();
  statisticsMyH264Encode.stop();
  statisticsMyH264Decode.stop();
  statisticsMyH239Encode.stop();
  statisticsMyH239Decode.stop();

  //dong backup below
  /*PString remoteName = '"' + call.GetPartyB() + '"';
  switch (call.GetCallEndReason()) {
    case OpalConnection::EndedByRemoteUser :
      cout << remoteName << " has cleared the call";
      break;
    case OpalConnection::EndedByCallerAbort :
      cout << remoteName << " has stopped calling";
      break;
    case OpalConnection::EndedByRefusal :
      cout << remoteName << " did not accept your call";
      break;
    case OpalConnection::EndedByNoAnswer :
      cout << remoteName << " did not answer your call";
      break;
    case OpalConnection::EndedByTransportFail :
      cout << "Call with " << remoteName << " ended abnormally";
      break;
    case OpalConnection::EndedByCapabilityExchange :
      cout << "Could not find common codec with " << remoteName;
      break;
    case OpalConnection::EndedByNoAccept :
      cout << "Did not accept incoming call from " << remoteName;
      break;
    case OpalConnection::EndedByAnswerDenied :
      cout << "Refused incoming call from " << remoteName;
      break;
    case OpalConnection::EndedByNoUser :
      cout << "Gatekeeper or registrar could not find user " << remoteName;
      break;
    case OpalConnection::EndedByNoBandwidth :
      cout << "Call to " << remoteName << " aborted, insufficient bandwidth.";
      break;
    case OpalConnection::EndedByUnreachable :
      cout << remoteName << " could not be reached.";
      break;
    case OpalConnection::EndedByNoEndPoint :
      cout << "No phone running for " << remoteName;
      break;
    case OpalConnection::EndedByHostOffline :
      cout << remoteName << " is not online.";
      break;
    case OpalConnection::EndedByConnectFail :
      cout << "Transport error calling " << remoteName;
      break;
    default :
      cout << "Call with " << remoteName << " completed";
  }
  PTime now;
  cout << ", on " << now.AsString("w h:mma") << ". Duration "
       << setprecision(0) << setw(5) << (now - call.GetStartTime())
       << "s." << endl;*/

  OpalManager::OnClearedCall(call);
}


PBoolean MyManager::OnOpenMediaStream(OpalConnection & connection,
                                  OpalMediaStream & stream)
{
  if (!OpalManager::OnOpenMediaStream(connection, stream))
    return PFalse;


  cout << "Started ";

  PCaselessString prefix = connection.GetEndPoint().GetPrefixName();
  if (prefix == "pc" || prefix == "pots")
    cout << (stream.IsSink() ? "playing " : "grabbing ") << stream.GetMediaFormat();
  else if (prefix == "ivr")
    cout << (stream.IsSink() ? "streaming " : "recording ") << stream.GetMediaFormat();
  else
  {
    cout << (stream.IsSink() ? "sending " : "receiving ") << stream.GetMediaFormat()
          << (stream.IsSink() ? " to " : " from ")<< prefix;
	setBandWidthInternal();
	setProtocolInternal(stream);
  }
  cout << endl;

  return PTrue;
}



void MyManager::OnUserInputString(OpalConnection & connection, const PString & value)
{
  cout << "User input received: \"" << value << '"' << endl;
  OpalManager::OnUserInputString(connection, value);
}


///////////////////////////////////////////////////////////////

MyPCSSEndPoint::MyPCSSEndPoint(MyManager & mgr)
  : OpalPCSSEndPoint(mgr)
{
}


PBoolean MyPCSSEndPoint::OnShowIncoming(const OpalPCSSConnection & connection)
{
  incomingConnectionToken = connection.GetToken();

  if (autoAnswer)
    AcceptIncomingConnection(incomingConnectionToken);
  else {
    PTime now;
    cout << "\nCall on " << now.AsString("w h:mma")
         << " from " << connection.GetRemotePartyName()
         << ", answer (Yes/No/Busy)? " << flush;
  }

  return PTrue;
}


PBoolean MyPCSSEndPoint::OnShowOutgoing(const OpalPCSSConnection & connection)
{
  PTime now;
  cout << connection.GetRemotePartyName() << " is ringing on "
       << now.AsString("w h:mma") << " ..." << endl;
  return PTrue;
}


PBoolean MyPCSSEndPoint::SetSoundDevice(PArgList & args,
                                    const char * optionName,
                                    PSoundChannel::Directions dir)
{
  if (!args.HasOption(optionName))
    return PTrue;

  PString dev = args.GetOptionString(optionName);

  if (dir == PSoundChannel::Player) {
    if (SetSoundChannelPlayDevice(dev))
      return PTrue;
  }
  else {
    if (SetSoundChannelRecordDevice(dev))
      return PTrue;
  }

  cerr << "Device for " << optionName << " (\"" << dev << "\") must be one of:\n";

  PStringArray names = PSoundChannel::GetDeviceNames(dir);
  for (PINDEX i = 0; i < names.GetSize(); i++)
    cerr << "  \"" << names[i] << "\"\n";

  return PFalse;
}
//dong motify for tcp/telnet control
void MyManager::HeartBeat(PCLI::Arguments & args, INT)
{
	ostream & out = args.GetContext();
		out <<"heartbeat response"<< '\n';
	out.flush();
	PTime now;
	cout << "heartbeat : " << now.AsString("w h:mma")<<endl;
}
void MyManager::CallRemote(PCLI::Arguments & args, INT)
{
	ostream & out = args.GetContext();
	if (args.GetCount() == 0) {
		args.WriteUsage();
		return;
	}
	 PString & token = currentCallToken.IsEmpty() ? heldCallToken : currentCallToken;
	 PSafePtr<OpalCall> call = FindCallWithLock(token);
	 if (call == NULL)
	 {
		 //out <<"\nCalling:\t"<<args[0];
		 if (SetUpCall(srcEP, args[0], currentCallToken))
		 {
			 //out <<"\ncall "<<args[0]<<" ok\n";
		 }
	 }	 
	 else {
		 //out <<"\nCannot make call whilst call in progress.\n";
		 out <<"\ncall "<<args[0]<<" fail\n";
		 token.MakeEmpty();
	 }	 
	 out.flush();
}
void MyManager::HangupCurrentCall(PCLI::Arguments & args, INT)
{
	ostream & out = args.GetContext();
	PString & token = currentCallToken.IsEmpty() ? heldCallToken : currentCallToken;
	PSafePtr<OpalCall> call = FindCallWithLock(token);
	if (call == NULL)
		out <<"\nhang fail\n";/*out << "\nNo call to hang up!\n";*/
	else {
		//out <<"\nhang ok\n";//out << "\nClearing call " << call->GetPartyA()<< " || "<< call->GetPartyB()<< endl;
		call->Clear();
		token.MakeEmpty();
	}
	out.flush();
}
//dong change for h239
void MyManager::StartH239Connnection(PCLI::Arguments & args, INT)
{
	ostream & out = args.GetContext();
	if (currentCallToken.IsEmpty()) {
		out <<"\ndual fail\n";//out << "Cannot do start H239Connnection while no call in progress\n";
		return;
	}

	PSafePtr<OpalCall> call = FindCallWithLock(currentCallToken);
	if (call == NULL) {
		out <<"\ndual fail\n";//out << "Current call disappeared!\n";
		return;
	}

	PSafePtr<OpalConnection> conn = call->GetConnection(0);
	while (conn != NULL) {
		conn->startH239Conn();
		++conn;
	}
	out << "\ndual ok\n";
	out.flush();
}
//dong change for h239
void MyManager::StopH239Connnection(PCLI::Arguments & args, INT)
{
	ostream & out = args.GetContext();
	if (currentCallToken.IsEmpty()) {
		out <<"\nstop fail\n";//out << "Cannot do stop H239Connnection while no call in progress\n";
		return;
	}

	PSafePtr<OpalCall> call = FindCallWithLock(currentCallToken);
	if (call == NULL) {
		out <<"\nstop fail\n";//out << "Current call disappeared!\n";
		return;
	}

	PSafePtr<OpalConnection> conn = call->GetConnection(0);
	while (conn != NULL) {
		conn->stopH239Conn();
		++conn;
	}
	out <<"\nstop ok\n";//out << "\nstoping dual stream\n";
	out.flush();
}
void MyManager::UpdatePicture(PCLI::Arguments & args, INT)
{
	ostream & out = args.GetContext();
	if (args.GetCount() == 0) {
		args.WriteUsage();
		return;
	}

	PString & token = currentCallToken.IsEmpty() ? heldCallToken : currentCallToken;
	PSafePtr<OpalCall> call = FindCallWithLock(token);
	if (call == NULL)
	{
		out <<"\nno call in progress.\n";
	}	 
	else {
		PSafePtr<OpalConnection> conn = call->GetConnection(0);
		BOOL fromLocal = FALSE;
		unsigned short channelId=0;
		if (args[0] == "local")
			fromLocal = true;
		if (args[1] == "dual")
			channelId = 1;

		while (conn != NULL) {
			if (fromLocal)
			{
				conn->updateLocalPicture(channelId);
			} 
			else
			{
				conn->updateRemotePicture(channelId);
			}
			++conn;
		}
		out <<"\nupdate "<<args[0]<<" picture:\t"<<args[1]<<'\n';
	}	 
	out.flush();
}

void MyManager::SetBandWidth(PCLI::Arguments & args, INT)
{
	//[(total) or (main) or (dual)] [data] Kb \n(e.g. bandwidth total 8000)\n
	ostream & out = args.GetContext();
	if (args.GetCount() == 0) {
		args.WriteUsage();
		return;
	}
	unsigned int bandWidth=0;
	bandWidth =args[1].AsUnsigned();
	if ((bandWidth > 10000) || (bandWidth < 128))
	{
		out <<"\nbandwidth "<<args[0] <<" fail"<<'\n';
		//out <<"\nbandwidth ranges from 128 to 10000"<<'\n';
		out.flush();
		return;
	}

	//TODO just set it to kb, will change it in future
	bandWidth*=1000;
	if (args[0] == "main")
	{
		OpalMediaFormatList allMediaFormats;
		OpalMediaFormat::GetAllRegisteredMediaFormats(allMediaFormats);
		for (PINDEX i = 0; i < allMediaFormats.GetSize(); i++) {
			OpalMediaFormat mediaFormat = allMediaFormats[i];
			if (mediaFormat.GetName() == OPAL_H264)
			{
				mediaFormat.SetOptionInteger(OpalMediaFormat::MaxBitRateOption(), bandWidth);
				if (mediaFormat.GetOptionInteger(OpalMediaFormat::TargetBitRateOption()) > bandWidth)
					mediaFormat.SetOptionInteger(OpalMediaFormat::TargetBitRateOption(), bandWidth);
				OpalMediaFormat::SetRegisteredMediaFormat(mediaFormat);
				out <<"\nbandwidth "<<args[0] <<" ok"<<'\n';
				//out <<"\nset "<<args[0] <<" stream bandwidth:\t"<<bandWidth/1000<<" ok"<<'\n';
			}
		}
	}else if (args[0] == "dual")
	{
		OpalMediaFormatList allMediaFormats;
		OpalMediaFormat::GetAllRegisteredMediaFormats(allMediaFormats);
		for (PINDEX i = 0; i < allMediaFormats.GetSize(); i++) {
			OpalMediaFormat mediaFormat = allMediaFormats[i];
			if (mediaFormat.GetName() == OPAL_H264_H239)
			{
				mediaFormat.SetOptionInteger(OpalMediaFormat::MaxBitRateOption(), bandWidth);
				if (mediaFormat.GetOptionInteger(OpalMediaFormat::TargetBitRateOption()) > bandWidth)
					mediaFormat.SetOptionInteger(OpalMediaFormat::TargetBitRateOption(), bandWidth);
				OpalMediaFormat::SetRegisteredMediaFormat(mediaFormat);
				out <<"\nbandwidth "<<args[0] <<" ok"<<'\n';
				//out <<"\nset "<<args[0] <<" stream bandwidth:\t"<<bandWidth/1000<<" ok"<<'\n';
			}
		}
	} 
	else
	{
		//TODO just set it to adapter, will change it in future
		bandWidth/=100;
		h323EP->SetInitialBandwidth(bandWidth);
		out <<"\nbandwidth "<<args[0] <<" ok"<<'\n';
		//out <<"\nset "<<args[0] <<" stream bandwidth:\t"<<bandWidth/10<<" ok"<<'\n';
	}
	out.flush();	
}

void MyManager::GetBandWidth(PCLI::Arguments & args, INT)
{
	ostream & out = args.GetContext();
	unsigned totalBandwidth = h323EP->GetInitialBandwidth();
	unsigned bandwidth =0;
	unsigned mainMaxBandwidth =0;
	unsigned mainTargetBandwidth =0;

	OpalMediaFormatList allMediaFormats;
	OpalMediaFormat::GetAllRegisteredMediaFormats(allMediaFormats);
	unsigned* parm = new unsigned[2];
	for (PINDEX i = 0; i < allMediaFormats.GetSize(); i++) {
		OpalMediaFormat mediaFormat = allMediaFormats[i];
		if (mediaFormat.GetName() == OPAL_H264)
		{
			mainMaxBandwidth = mediaFormat.GetOptionInteger(OpalMediaFormat::MaxBitRateOption());
			//This is encoding bit rate
			mainTargetBandwidth = mediaFormat.GetOptionInteger(OpalMediaFormat::TargetBitRateOption());
			//I need to get the decoding bit rate from 8168 as below
			
			PString & token = currentCallToken.IsEmpty() ? heldCallToken : currentCallToken;
			PSafePtr<OpalCall> call = FindCallWithLock(token);
			if (call == NULL)
			{
				out <<"\nno call in progress.\n";
			}	 
			else {
				PSafePtr<OpalConnection> conn = call->GetConnection(0);
				unsigned short channelId=0;	
				while (conn != NULL) {
					PString connName(conn->GetClass());
					if (connName == "H323Connection")
					{//channelId is held for dual channel. dong
						bandwidth = (conn->GetBandwidthUsed() /20);
						conn->getBandwidth(parm, channelId );
						out <<"\nEncode bandwidth "<<parm[0]<<" Decode bandwidth "<<parm[1]<<"\n";
					}					
					++conn;
				}
			}		
		}
	}
	out.flush();
	delete parm;
}
void MyManager::SetLayout(PCLI::Arguments & args, INT)
{
	ostream & out = args.GetContext();
	if (args.GetCount() == 0) {
		args.WriteUsage();
		return;
	}
	LayoutType layoutType = static_cast<LayoutType>(args[0].AsUnsigned());
	SetLayoutInternal(layoutType);	
}

void MyManager::SetLayoutInternal(unsigned int num)
{
	/*	OpalPluginTranscoderFactory<OpalPluginVideoTranscoder>*/
	OpalTranscoderKey opalTransKey("DM8168","H.264");
	OpalTranscoder * ctrlMsg =OpalTranscoderFactory::CreateInstance(opalTransKey);
	RAVE_CT_CTRL_MSG* param =new RAVE_CT_CTRL_MSG;
	param->cmd=RAVE_CT_LAY_OUT_SET;
	param->input=	num;
	param->output=0;
	param->length=sizeof(RAVE_CT_CTRL_MSG)-4;
	OpalCtrlMsg opalCtrlMsg(param);
	ctrlMsg->ExecuteCommand(opalCtrlMsg);
	RAVE_CT_CTRL_MSG* retMsg =static_cast<RAVE_CT_CTRL_MSG*>(opalCtrlMsg.GetPlugInData());
	delete param;
}

void MyManager::SetGK(PCLI::Arguments & args, INT)
{
	ostream & out = args.GetContext();
	if (args.GetCount() == 0) {
		args.WriteUsage();
		return;
	}

	if (args[0] == "on")
	{
		if (e164.IsEmpty())
		{
			out << "\nplease set e164 first!\n";
			out.flush();
			return;
		}
		PString gkAddr ="";
		if (args.GetCount() >1)
		{
			gkAddr = args[1];
		}
		if (!h323EP->UseGatekeeper(gkAddr,PString::Empty(),PString("192.168.1.206")))
		{
			out << "\nset gk on fail\n";
		}		
	}else if (args[0] == "off")
	{
		if (!h323EP->RemoveGatekeeper())
		{
			out << "\nset gk off fail\n";
		}else
		{
			OnGKUnRegisterSuccess();
		}
	}else
	{
		out << "\nset gk unknown command\n";
	}
	out.flush();
}

void MyManager::SetE164(PCLI::Arguments & args, INT)
{
	ostream & out = args.GetContext();
	if (args.GetCount() == 0) {
		args.WriteUsage();
		return;
	}
	if (args[0].IsEmpty())
	{
		out << "\ne164 is empty, please set again!\n";
	}else
	{
		e164 = args[0];
		h323EP->SetLocalUserName(e164);
		if (!alias.IsEmpty())
		{
			h323EP->AddAliasName(alias);
		}
		SetDefaultUserName(e164, true);
		SetDefaultDisplayName(e164, true);
		out << "\nset e164: " << e164<<" ok \n";
		if (!gkIp.IsEmpty())
		{
			isGkUpdate =true;
			if (!h323EP->UseGatekeeper(gkIp,PString::Empty(),PString("192.168.1.206")))
			{
				out << "\nset gk on fail\n";
			}	
		}
	}
	out.flush();
}

void MyManager::SetAlias(PCLI::Arguments & args, INT)
{
	ostream & out = args.GetContext();
	if (args.GetCount() == 0) {
		args.WriteUsage();
		return;
	}
	if (args[0].IsEmpty())
	{
		out << "\nalias is empty, please set again!\n";
	}else
	{
		if (e164.IsEmpty())
		{
			out << "\nplease set e164 first!\n";
			out.flush();
			return;
		}else
		{
			alias = args[0];
			h323EP->SetLocalUserName(e164);
			h323EP->AddAliasName(alias);
		}

		out << "\nset alias: " << alias<<" ok \n";
		if (!gkIp.IsEmpty())
		{
			isGkUpdate =true;
			if (!h323EP->UseGatekeeper(gkIp,PString::Empty(),PString("192.168.1.206")))
			{
				out << "\nset gk on fail\n";
			}	
		}
	}
	out.flush();
}
void MyManager::GetStatistics(PCLI::Arguments & args, INT)
{
	ostream & out = args.GetContext();
	out<<"******************************************************************\n";
	out<<"remote_name:"<<remoteName<<"\n";
	out<<"call_bandwidth:"<<bandwidth<<"\n";
	out<<"******************************************************************\n";
	out <<"audio_encode_protocol:"<<statisticsMyAudioEncode.getProtocol()<<"\n";
	out <<"audio_encode_bitRate:"<<statisticsMyAudioEncode.getBitRate()<<"\n";
	out <<"audio_encode_frameRate:"<<statisticsMyAudioEncode.getFrameRate()<<"\n";
	out <<"audio_encode_packetsCount:"<<statisticsMyAudioEncode.getPacketCount()<<"\n";	
	out <<"audio_encode_lostCount:"<<statisticsMyAudioEncode.getPacketLost()<<"\n";
	out <<"audio_encode_lostRate:"<<statisticsMyAudioEncode.getPacketLostRate()<<"%\n";
	out<<"******************************************************************\n";
	out <<"audio_decode_protocol:"<<statisticsMyAudioDecode.getProtocol()<<"\n";
	out <<"audio_decode_bitRate:"<<statisticsMyAudioDecode.getBitRate()<<"\n";
	out <<"audio_decode_frameRate:"<<statisticsMyAudioDecode.getFrameRate()<<"\n";
	out <<"audio_decode_packetsCount:"<<statisticsMyAudioDecode.getPacketCount()<<"\n";	
	out <<"audio_decode_lostCount:"<<statisticsMyAudioDecode.getPacketLost()<<"\n";
	out <<"audio_decode_lostRate:"<<statisticsMyAudioDecode.getPacketLostRate()<<"%\n";
	out<<"******************************************************************\n";
	out <<"video_encode_protocol:"<<statisticsMyH264Encode.getProtocol()<<"\n";
	out <<"video_encode_resolution:"<<statisticsMyH264Encode.getResolution()<<"\n";
	out <<"video_encode_bitRate:"<<statisticsMyH264Encode.getBitRate()<<"\n";	
	out <<"video_encode_frameRate:"<<statisticsMyH264Encode.getFrameRate()<<"\n";
	out <<"video_encode_packetsCount:"<<statisticsMyH264Encode.getPacketCount()<<"\n";	
	out <<"video_encode_lostCount:"<<statisticsMyH264Encode.getPacketLost()<<"\n";
	out <<"video_encode_lostRate:"<<statisticsMyH264Encode.getPacketLostRate()<<"%\n";
	out<<"******************************************************************\n";
	out <<"video_decode_protocol:"<<statisticsMyH264Decode.getProtocol()<<"\n";
	out <<"video_decode_resolution:"<<statisticsMyH264Decode.getResolution()<<"\n";
	out <<"video_decode_bitRate:"<<statisticsMyH264Decode.getBitRate()<<"\n";
	out <<"video_decode_frameRate:"<<statisticsMyH264Decode.getFrameRate()<<"\n";
	out <<"video_decode_packetsCount:"<<statisticsMyH264Decode.getPacketCount()<<"\n";	
	out <<"video_decode_lostCount:"<<statisticsMyH264Decode.getPacketLost()<<"\n";
	out <<"video_decode_lostRate:"<<statisticsMyH264Decode.getPacketLostRate()<<"%\n";
	out<<"******************************************************************\n";
	out <<"dual_encode_protocol:"<<statisticsMyH239Encode.getProtocol()<<"\n";
	out <<"dual_encode_resolution:"<<statisticsMyH239Encode.getResolution()<<"\n";
	out <<"dual_encode_bitRate:"<<statisticsMyH239Encode.getBitRate()<<"\n";	
	out <<"dual_encode_frameRate:"<<statisticsMyH239Encode.getFrameRate()<<"\n";
	out <<"dual_encode_packetsCount:"<<statisticsMyH239Encode.getPacketCount()<<"\n";	
	out <<"dual_encode_lostCount:"<<statisticsMyH239Encode.getPacketLost()<<"\n";
	out <<"dual_encode_lostRate:"<<statisticsMyH239Encode.getPacketLostRate()<<"%\n";
	out<<"******************************************************************\n";
	out <<"dual_decode_protocol:"<<statisticsMyH239Decode.getProtocol()<<"\n";
	out <<"dual_decode_resolution:"<<statisticsMyH239Decode.getResolution()<<"\n";
	out <<"dual_decode_bitRate:"<<statisticsMyH239Decode.getBitRate()<<"\n";
	out <<"dual_decode_frameRate:"<<statisticsMyH239Decode.getFrameRate()<<"\n";
	out <<"dual_decode_packetsCount:"<<statisticsMyH239Decode.getPacketCount()<<"\n";	
	out <<"dual_decode_lostCount:"<<statisticsMyH239Decode.getPacketLost()<<"\n";
	out <<"dual_decode_lostRate:"<<statisticsMyH239Decode.getPacketLostRate()<<"%\n";
	out<<"******************************************************************\n";
	out.flush();
}

void MyManager::setMainStatistic(PTimer &, INT)
{
	PString & token = currentCallToken.IsEmpty() ? heldCallToken : currentCallToken;
	PSafePtr<OpalCall> call = FindCallWithLock(token);
	if (call == NULL)
	{
		/*out <<"\nno call in progress.\n"*/;
	}	 
	else {
		PSafePtr<OpalConnection> conn = call->GetConnection(0);
		while (conn != NULL) {
			PString connName(conn->GetClass());
			if (connName == "H323Connection")
			{
				statisticsAudioEncodeLast =statisticsAudioEncode;
				statisticsAudioDecodeLast =statisticsAudioDecode;
				statisticsH264EncodeLast =statisticsH264Encode;
				statisticsH264DecodeLast =statisticsH264Decode;

				conn->getStatistic(statisticsAudioEncode,1);
				conn->getStatistic(statisticsAudioDecode,2);
				conn->getStatistic(statisticsH264Encode,3);
				conn->getStatistic(statisticsH264Decode,4);
				//kbps
				statisticsMyAudioEncode.setBitRate(statisticsAudioEncode.m_totalBytes,statisticsAudioEncodeLast.m_totalBytes);
				statisticsMyAudioDecode.setBitRate(statisticsAudioDecode.m_totalBytes,statisticsAudioDecodeLast.m_totalBytes);
				statisticsMyH264Encode.setBitRate(statisticsH264Encode.m_totalBytes,statisticsH264EncodeLast.m_totalBytes);
				statisticsMyH264Decode.setBitRate(statisticsH264Decode.m_totalBytes,statisticsH264DecodeLast.m_totalBytes);
				
				//1080P use negotiation fixed value
				//frameRate
				statisticsMyAudioEncode.setFrameRate(statisticsAudioEncode.m_totalFrames,statisticsAudioEncodeLast.m_totalFrames);
				statisticsMyAudioDecode.setFrameRate(statisticsAudioDecode.m_totalFrames,statisticsAudioDecodeLast.m_totalFrames);
				statisticsMyH264Encode.setFrameRate(statisticsH264Encode.m_totalFrames,statisticsH264EncodeLast.m_totalFrames);
				statisticsMyH264Decode.setFrameRate(statisticsH264Decode.m_totalFrames,statisticsH264DecodeLast.m_totalFrames);
			
				//total packet
				statisticsMyAudioEncode.setPacketCount(statisticsAudioEncode.m_totalPackets);
				statisticsMyAudioDecode.setPacketCount(statisticsAudioDecode.m_totalPackets);
				statisticsMyH264Encode.setPacketCount(statisticsH264Encode.m_totalPackets);
				statisticsMyH264Decode.setPacketCount(statisticsH264Decode.m_totalPackets);

				//lost packet
				statisticsMyAudioEncode.setPacketLost(statisticsAudioEncode.m_packetsLost);
				statisticsMyAudioDecode.setPacketLost(statisticsAudioDecode.m_packetsLost);
				statisticsMyH264Encode.setPacketLost(statisticsH264Encode.m_packetsLost);
				statisticsMyH264Decode.setPacketLost(statisticsH264Decode.m_packetsLost);
				//packet lost rate.
				statisticsMyAudioEncode.setPacketSec(statisticsAudioEncode.m_totalPackets,statisticsAudioEncodeLast.m_totalPackets);
				statisticsMyAudioEncode.setPacketLostSec(statisticsAudioEncode.m_packetsLost,statisticsAudioEncodeLast.m_packetsLost);
				statisticsMyAudioDecode.setPacketSec(statisticsAudioDecode.m_totalPackets,statisticsAudioDecodeLast.m_totalPackets);
				statisticsMyAudioDecode.setPacketLostSec(statisticsAudioDecode.m_packetsLost,statisticsAudioDecodeLast.m_packetsLost);
				statisticsMyH264Encode.setPacketSec(statisticsH264Encode.m_totalPackets,statisticsH264EncodeLast.m_totalPackets);
				statisticsMyH264Encode.setPacketLostSec(statisticsH264Encode.m_packetsLost,statisticsH264EncodeLast.m_packetsLost);
				statisticsMyH264Decode.setPacketSec(statisticsH264Decode.m_totalPackets,statisticsH264DecodeLast.m_totalPackets);
				statisticsMyH264Decode.setPacketLostSec(statisticsH264Decode.m_packetsLost,statisticsH264DecodeLast.m_packetsLost);
			}
			++conn;
		}
	}	
}

void MyManager::setDualStatistic(PTimer &, INT)
{
	PString & token = currentCallToken.IsEmpty() ? heldCallToken : currentCallToken;
	PSafePtr<OpalCall> call = FindCallWithLock(token);
	if (call == NULL)
	{
		/*out <<"\nno call in progress.\n"*/;
	}	 
	else {
		PSafePtr<OpalConnection> conn = call->GetConnection(0);
		while (conn != NULL) {
			PString connName(conn->GetClass());
			if (connName == "H323Connection")
			{
				statisticsH239EncodeLast =statisticsH239Encode;
				statisticsH239DecodeLast =statisticsH239Decode;

				conn->getStatistic(statisticsH239Encode,5);
				conn->getStatistic(statisticsH239Decode,6);
				//kbps
				statisticsMyH239Encode.setBitRate(statisticsH239Encode.m_totalBytes,statisticsH239EncodeLast.m_totalBytes);
				statisticsMyH239Decode.setBitRate(statisticsH239Decode.m_totalBytes,statisticsH239DecodeLast.m_totalBytes);

				//1080P use negotiation fixed value
				//frameRate
				statisticsMyH239Encode.setFrameRate(statisticsH239Encode.m_totalFrames,statisticsH239EncodeLast.m_totalFrames);
				statisticsMyH239Decode.setFrameRate(statisticsH239Decode.m_totalFrames,statisticsH239DecodeLast.m_totalFrames);

				//total packet
				statisticsMyH239Encode.setPacketCount(statisticsH239Encode.m_totalPackets);
				statisticsMyH239Decode.setPacketCount(statisticsH239Decode.m_totalPackets);

				//lost packet
				statisticsMyH239Encode.setPacketLost(statisticsH239Encode.m_packetsLost);
				statisticsMyH239Decode.setPacketLost(statisticsH239Decode.m_packetsLost);
				//packet lost rate.
				statisticsMyH239Encode.setPacketSec(statisticsH239Encode.m_totalPackets,statisticsH239EncodeLast.m_totalPackets);
				statisticsMyH239Encode.setPacketLostSec(statisticsH239Encode.m_packetsLost,statisticsH239EncodeLast.m_packetsLost);
				statisticsMyH239Decode.setPacketSec(statisticsH239Decode.m_totalPackets,statisticsH239DecodeLast.m_totalPackets);
				statisticsMyH239Decode.setPacketLostSec(statisticsH239Decode.m_packetsLost,statisticsH239DecodeLast.m_packetsLost);
			}
			++conn;
		}
	}	
}


bool MyManager::setRemoteName()
{
	PString & token = currentCallToken.IsEmpty() ? heldCallToken : currentCallToken;
	PSafePtr<OpalCall> call = FindCallWithLock(token);
	if (call == NULL)
	{
		/*out <<"\nno call in progress.\n"*/;
	}	 
	else {
		PSafePtr<OpalConnection> conn = call->GetConnection(0);
		while (conn != NULL) {
			PString connName(conn->GetClass());
			if (connName == "H323Connection")
			{
				remoteName =conn->GetRemotePartyName();
			}
			++conn;
		}
	}	
	return true;
}

bool MyManager::setBandWidthInternal()
{
	PString & token = currentCallToken.IsEmpty() ? heldCallToken : currentCallToken;
	PSafePtr<OpalCall> call = FindCallWithLock(token);
	if (call == NULL)
	{
		//out <<"\nno call in progress.\n";
	}	 
	else {
		PSafePtr<OpalConnection> conn = call->GetConnection(0);
		while (conn != NULL) {
			PString connName(conn->GetClass());
			if (connName == "H323Connection")
			{
				bandwidth = (conn->GetBandwidthUsed() /20);
			}					
			++conn;
		}
	}
	return true;
}
void MyManager::setProtocolInternal(OpalMediaStream & stream)
{

	if (stream.IsSink())
	{
		if ( H323Capability::DefaultAudioSessionID == stream.GetSessionID())
		{
			statisticsMyAudioEncode.setProtocol(stream.GetMediaFormat().GetName());
		}
		else if (H323Capability::DefaultVideoSessionID == stream.GetSessionID())
		{
			statisticsMyH264Encode.setProtocol(stream.GetMediaFormat().GetName());
			statisticsMyH264Encode.setResolution(PString("1080P"));//TODO dong with negotiation
		} 
		else
		{
			statisticsMyH239Encode.setProtocol(stream.GetMediaFormat().GetName());
			statisticsMyH239Encode.setResolution(PString("1080P"));//TODO dong with negotiation
		}			
	}
	else
	{
		if ( H323Capability::DefaultAudioSessionID == stream.GetSessionID())
		{
			statisticsMyAudioDecode.setProtocol(stream.GetMediaFormat().GetName());
		}
		else if (H323Capability::DefaultVideoSessionID == stream.GetSessionID())
		{
			statisticsMyH264Decode.setProtocol(stream.GetMediaFormat().GetName());
			statisticsMyH264Decode.setResolution(PString("1080P"));//TODO dong with negotiation
		} 
		else
		{
			statisticsMyH239Decode.setProtocol(stream.GetMediaFormat().GetName());
			statisticsMyH239Decode.setResolution(PString("1080P"));//TODO dong with negotiation
		}
	}
}

// End of File ///////////////////////////////////////////////////////////////
