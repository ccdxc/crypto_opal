/*
 * ivr.h
 *
 * Interactive Voice Response support.
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
 *
 * Copyright (c) 2001 Equivalence Pty. Ltd.
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
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 27149 $
 * $Author: rjongbloed $
 * $Date: 2012-03-07 18:32:36 -0600 (Wed, 07 Mar 2012) $
 */

#ifndef OPAL_OPAL_IVR_H
#define OPAL_OPAL_IVR_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <opal/buildopts.h>

#if OPAL_IVR

#include <opal/opalvxml.h>
#include <opal/localep.h>

class OpalIVRConnection;


/**Interactive Voice Response endpoint.
 */
class OpalIVREndPoint : public OpalLocalEndPoint
{
    PCLASSINFO(OpalIVREndPoint, OpalLocalEndPoint);
  public:
  /**@name Construction */
  //@{
    /**Create a new endpoint.
     */
    OpalIVREndPoint(
      OpalManager & manager,  ///<  Manager of all endpoints.
      const char * prefix = "ivr" ///<  Prefix for URL style address strings
    );

    /**Destroy endpoint.
     */
    ~OpalIVREndPoint();
  //@}

  /**@name Overrides from OpalEndPoint */
  //@{
    /**Set up a connection to a remote party.
       This is called from the OpalManager::MakeConnection() function once
       it has determined that this is the endpoint for the protocol.

       The general form for this party parameter is:

            [proto:][alias@][transport$]address[:port]

       where the various fields will have meanings specific to the endpoint
       type. For example, with H.323 it could be "h323:Fred@site.com" which
       indicates a user Fred at gatekeeper size.com. Whereas for the PSTN
       endpoint it could be "pstn:5551234" which is to call 5551234 on the
       first available PSTN line.

       The proto field is optional when passed to a specific endpoint. If it
       is present, however, it must agree with the endpoints protocol name or
       false is returned.

       This function usually returns almost immediately with the connection
       continuing to occur in a new background thread.

       If false is returned then the connection could not be established. For
       example if a PSTN endpoint is used and the assiciated line is engaged
       then it may return immediately. Returning a non-NULL value does not
       mean that the connection will succeed, only that an attempt is being
       made.

       The default behaviour is pure.
     */
    virtual PSafePtr<OpalConnection> MakeConnection(
      OpalCall & call,          ///<  Owner of connection
      const PString & party,    ///<  Remote party to call
      void * userData = NULL,   ///<  Arbitrary data to pass to connection
      unsigned int options = 0, ///<  options to pass to conneciton
      OpalConnection::StringOptions * stringOptions = NULL

    );

    /**Get the data formats this endpoint is capable of operating.
       This provides a list of media data format names that may be used by an
       OpalMediaStream may be created by a connection from this endpoint.

       Note that a specific connection may not actually support all of the
       media formats returned here, but should return no more.

       The default behaviour is pure.
      */
    virtual OpalMediaFormatList GetMediaFormats() const;
  //@}

  /**@name Customisation call backs */
  //@{
    /**Create a connection for the PCSS endpoint.
       The default implementation is to create a OpalPCSSConnection.
      */
    virtual OpalIVRConnection * CreateConnection(
      OpalCall & call,        ///<  Owner of connection
      void * userData,        ///<  Arbitrary data to pass to connection
      const PString & vxml,   ///<  vxml to execute
      unsigned int options,
      OpalConnection::StringOptions * stringOptions = NULL
    );
  //@}

  /**@name Options and configuration */
  //@{
    /**Find a connection that uses the specified token.
       This searches the endpoint for the connection that contains the token
       as provided by functions such as MakeConnection().
      */
    PSafePtr<OpalIVRConnection> GetIVRConnectionWithLock(
      const PString & token,     ///<  Token to identify connection
      PSafetyMode mode = PSafeReadWrite
    ) { return PSafePtrCast<OpalConnection, OpalIVRConnection>(GetConnectionWithLock(token, mode)); }

    /**Get the default VXML to use.
      */
    const PString & GetDefaultVXML() const { return defaultVXML; }

    /** Set the default VXML to use.
      */
    void SetDefaultVXML(
      const PString & vxml
    );

    /**Set the default media formats for all connections using VXML.
      */
    void SetDefaultMediaFormats(
      const OpalMediaFormatList & formats
    );

    /** Called when VXML ends.
        Gives an opputunity for an application to do somthing when script ends,
        start a new script for instance.

        Default action does nothing, session will end and connection will be
        released.
      */
    virtual void OnEndDialog(
      OpalIVRConnection & connection  ///< Connection with ending dialog
    );

    /** Set/get the default text to speech engine used by the IVR  
      */
    void SetDefaultTextToSpeech(const PString & tts)
    { defaultTts = tts; }

    PString GetDefaultTextToSpeech() const
    { return defaultTts; }

  //@}

  protected:
    PString             defaultVXML;
    OpalMediaFormatList defaultMediaFormats;
    PString             defaultTts;

  private:
    P_REMOVE_VIRTUAL(OpalIVRConnection *, CreateConnection(OpalCall &,const PString &,void *,const PString &,OpalConnection::StringOptions *),0);
};


/**Interactive Voice Response connection.
 */
class OpalIVRConnection : public OpalLocalConnection
{
    PCLASSINFO(OpalIVRConnection, OpalLocalConnection);
  public:
  /**@name Construction */
  //@{
    /**Create a new endpoint.
     */
    OpalIVRConnection(
      OpalCall & call,            ///<  Owner calll for connection
      OpalIVREndPoint & endpoint, ///<  Owner endpoint for connection
      void * userData,            ///<  Arbitrary data to pass to connection
      const PString & vxml,       ///<  vxml to execute
      unsigned int options,
      OpalConnection::StringOptions * stringOptions = NULL
    );

    /**Destroy endpoint.
     */
    ~OpalIVRConnection();
  //@}

  /**@name Overrides from OpalConnection */
  //@{
    /**Get indication of connection being to a "network".
       This indicates the if the connection may be regarded as a "network"
       connection. The distinction is about if there is a concept of a "remote"
       party being connected to and is best described by example: sip, h323,
       iax and pstn are all "network" connections as they connect to something
       "remote". While pc, pots and ivr are not as the entity being connected
       to is intrinsically local.
      */
    virtual bool IsNetworkConnection() const { return false; }

    /**Get the local name/alias.
      */
    virtual PString GetLocalPartyURL() const;

    /**A call back function whenever a connection is "established".
       This indicates that a connection to an endpoint was established. This
       usually occurs after OnConnected() and indicates that the connection
       is both connected and has media flowing.
     */
    void OnEstablished();
    
    /**A call back function to monitor the progress of a transfer.

       The default behaviour calls the OpalEndPoint function of the same name.
       The default action of that function is to return false, thereby
       releasing the connection if the info["result"] == "success".
     */
    virtual bool OnTransferNotify(
      const PStringToString & info, ///< State of the transfer
      const OpalConnection * transferringConnection
                                    ///< Connection performing transfer, maybe "this"
    );

    /**Initiate the transfer of an existing call (connection) to a new remote 
       party.

       If remoteParty is a valid call token, then the remote party is transferred
       to that party (consultation transfer) and both calls are cleared.
     */
    virtual bool TransferConnection(
      const PString & remoteParty   ///<  Remote party to transfer the existing call to
    );

    /**Get the data formats this connection is capable of operating.
       This provides a list of media data format names that an
       OpalMediaStream may be created in within this connection.

       The default behaviour returns the formats the PSoundChannel can do,
       typically only PCM-16.
      */
    virtual OpalMediaFormatList GetMediaFormats() const;

    /**Open a new media stream.
       This will create a media stream of an appropriate subclass as required
       by the underlying connection protocol. For instance H.323 would create
       an OpalRTPStream.

       The sessionID parameter may not be needed by a particular media stream
       and may be ignored. In the case of an OpalRTPStream it us used.

       Note that media streams may be created internally to the underlying
       protocol. This function is not the only way a stream can come into
       existance.

       The default behaviour is pure.
     */
    virtual OpalMediaStream * CreateMediaStream(
      const OpalMediaFormat & mediaFormat, ///<  Media format for stream
      unsigned sessionID,                  ///<  Session number for stream
      PBoolean isSource                        ///<  Is a source stream
    );

    /**Send a user input indication to the remote endpoint.
       This sends an arbitrary string as a user indication. If DTMF tones in
       particular are required to be sent then the SendIndicationTone()
       function should be used.

       The default behaviour plays the DTMF tones on the line.
      */
    virtual PBoolean SendUserInputString(
      const PString & value                   ///<  String value of indication
    );
  //@}

    /** Called when VXML ends.
        Default action calls equivalent function in OpalIVREndPoint.
      */
    virtual void OnEndDialog();

    const PString & GetVXML() const { return m_vxmlScript; }
    const OpalVXMLSession & GetVXMLSession() const { return m_vxmlSession; }
    OpalVXMLSession & GetVXMLSession() { return m_vxmlSession; }

    PTextToSpeech * GetTextToSpeech() const { return m_vxmlSession.GetTextToSpeech(); }
    PTextToSpeech * SetTextToSpeech(const PString & ttsName) { return m_vxmlSession.SetTextToSpeech(ttsName); }
    PTextToSpeech * SetTextToSpeech(PTextToSpeech * tts, PBoolean autoDelete = false) { return m_vxmlSession.SetTextToSpeech(tts, autoDelete); }

  protected:
    virtual bool StartVXML(const PString & vxml);
    virtual bool StartScript(const PString & script);

    OpalIVREndPoint   & endpoint;
    PString             m_vxmlScript;
    OpalMediaFormatList m_vxmlMediaFormats;
    OpalVXMLSession     m_vxmlSession;
};


/**This class describes a media stream that transfers data to/from an IVR
   vxml session.
  */
class OpalIVRMediaStream : public OpalRawMediaStream
{
    PCLASSINFO(OpalIVRMediaStream, OpalRawMediaStream);
  public:
  /**@name Construction */
  //@{
    /**Construct a new media stream for IVR session.
      */
    OpalIVRMediaStream(
      OpalIVRConnection & conn,
      const OpalMediaFormat & mediaFormat, ///<  Media format for stream
      unsigned sessionID,                  ///<  Session number for stream
      PBoolean isSource,                       ///<  Is a source stream
      PVXMLSession & vxml                  ///<  vxml session to use
    );
  //@}

  /**@name Overrides of OpalMediaStream class */
  //@{
    /**Open the media stream using the media format.

       The default behaviour simply sets the member variable "mediaFormat"
       and "defaultDataSize".
      */
    virtual PBoolean Open();

    /**Indicate if the media stream is synchronous.
       Returns false for IVR streams.
      */
    virtual PBoolean IsSynchronous() const;
  //@}

  protected:
    virtual void InternalClose();

    PVXMLSession & m_vxmlSession;
};


#endif // OPAL_IVR

#endif // OPAL_OPAL_IVR_H


// End of File ///////////////////////////////////////////////////////////////
