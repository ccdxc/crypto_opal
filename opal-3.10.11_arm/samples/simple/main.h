/*
 * main.h
 *
 * A simple OPAL "net telephone" application.
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
 * $Revision: 24299 $
 * $Author: rjongbloed $
 * $Date: 2010-04-28 08:52:07 -0500 (Wed, 28 Apr 2010) $
 */

#ifndef _SimpleOpal_MAIN_H
#define _SimpleOpal_MAIN_H

#include <ptclib/ipacl.h>
#include <opal/manager.h>
#include <opal/pcss.h>
#include <opal/ivr.h>
#include "MyStatistic.h"//dong change for statistic

#ifndef OPAL_PTLIB_AUDIO
#error Cannot compile without PTLib sound channel support!
#endif


class MyManager;
class SIPEndPoint;
class H323EndPoint;
class H323SEndPoint;
class IAX2EndPoint;
class OpalCapiEndPoint;


class MyPCSSEndPoint : public OpalPCSSEndPoint
{
  PCLASSINFO(MyPCSSEndPoint, OpalPCSSEndPoint)

  public:
    MyPCSSEndPoint(MyManager & manager);

    virtual PBoolean OnShowIncoming(const OpalPCSSConnection & connection);
    virtual PBoolean OnShowOutgoing(const OpalPCSSConnection & connection);

    PBoolean SetSoundDevice(PArgList & args, const char * optionName, PSoundChannel::Directions dir);

    PString incomingConnectionToken;
    bool    autoAnswer;
};


class MyManager : public OpalManager
{
  PCLASSINFO(MyManager, OpalManager)

  public:
    MyManager();
    ~MyManager();

    PBoolean Initialise(PArgList & args);
    void Main(PArgList & args);

    virtual void OnEstablishedCall(
      OpalCall & call   /// Call that was completed
    );
	//dong add for dual notify
	virtual void OnDualEstablished(OpalConnection & connection);
	virtual void OnClosedDualMediaStream( const OpalMediaStream & stream );
	//dong add for gk
	virtual void OnGKRegisterSuccess(PString gkIp);
	virtual void OnGKRegisterFail();
	void OnGKUnRegisterSuccess();

    virtual void OnClearedCall(
      OpalCall & call   /// Connection that was established
    );
    virtual PBoolean OnOpenMediaStream(
      OpalConnection & connection,  /// Connection that owns the media stream
      OpalMediaStream & stream    /// New media stream being opened
    );
    virtual void OnUserInputString(
      OpalConnection & connection,  /// Connection input has come from
      const PString & value         /// String value of indication
    );

  protected:
    bool InitialiseH323EP(PArgList & args, PBoolean secure, H323EndPoint * h323EP);

    PString currentCallToken;
    PString heldCallToken;

#if OPAL_LID
    OpalLineEndPoint * potsEP;
#endif
    MyPCSSEndPoint   * pcssEP;
#if OPAL_H323
    H323EndPoint     * h323EP;
#endif
#if OPAL_SIP
    SIPEndPoint      * sipEP;
#endif
#if OPAL_IAX2
    IAX2EndPoint     * iax2EP;
#endif
#if OPAL_CAPI
    OpalCapiEndPoint * capiEP;
#endif
#if OPAL_IVR
    OpalIVREndPoint  * ivrEP;
#endif
#if OPAL_FAX
    OpalFaxEndPoint  * faxEP;
#endif

    bool    pauseBeforeDialing;
    PString srcEP;
	PString gkIp;
	PString e164;
	PString alias;
	bool isGkUpdate;
	PTimer statisticsMainTimer;
	PTimer statisticsDualTimer;

	OpalMediaStatistics statisticsAudioEncode;//1
	OpalMediaStatistics statisticsAudioDecode;//2
	OpalMediaStatistics statisticsH264Encode;//3
	OpalMediaStatistics statisticsH264Decode;//4
	OpalMediaStatistics statisticsH239Encode;//5
	OpalMediaStatistics statisticsH239Decode;//6

	OpalMediaStatistics statisticsAudioEncodeLast;//1
	OpalMediaStatistics statisticsAudioDecodeLast;//2
	OpalMediaStatistics statisticsH264EncodeLast;//3
	OpalMediaStatistics statisticsH264DecodeLast;//4
	OpalMediaStatistics statisticsH239EncodeLast;//5
	OpalMediaStatistics statisticsH239DecodeLast;//6

	MyStatistic statisticsMyAudioEncode;//1
	MyStatistic statisticsMyAudioDecode;//2
	MyStatistic statisticsMyH264Encode;//3
	MyStatistic statisticsMyH264Decode;//4
	MyStatistic statisticsMyH239Encode;//5
	MyStatistic statisticsMyH239Decode;//6

	PString remoteName;
	unsigned bandwidth;


    void HangupCurrentCall();
    void StartCall(const PString & ostr);
    void HoldRetrieveCall();
    void TransferCall(const PString & dest);
#if OPAL_PTLIB_CONFIG_FILE
    void NewSpeedDial(const PString & ostr);
    void ListSpeedDials();
#endif // OPAL_PTLIB_CONFIG_FILE
    void SendMessageToRemoteNode(const PString & ostr);
    void SendTone(const char tone);
	void SetLayoutInternal(unsigned int num);
	bool setRemoteName();
	bool setBandWidthInternal();
	void setProtocolInternal(OpalMediaStream & stream);
//dong motify for tcp/telnet control//dong change for h239
	PCLI * cli;
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, HeartBeat);
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, CallRemote);
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, HangupCurrentCall);
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, StartH239Connnection);
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, StopH239Connnection);
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, UpdatePicture);
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, SetBandWidth);
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, GetBandWidth);
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, SetLayout);
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, SetGK);
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, SetE164);
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, SetAlias);
	PDECLARE_NOTIFIER(PCLI::Arguments, MyManager, GetStatistics);

	PDECLARE_NOTIFIER(PTimer, MyManager, setMainStatistic);
	PDECLARE_NOTIFIER(PTimer, MyManager, setDualStatistic);
	
};


class SimpleOpalProcess : public PProcess
{
  PCLASSINFO(SimpleOpalProcess, PProcess)

  public:
    SimpleOpalProcess();

    void Main();

  protected:
    MyManager * opal;
};


#endif  // _SimpleOpal_MAIN_H


// End of File ///////////////////////////////////////////////////////////////
