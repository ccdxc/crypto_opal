/*
 * rtpconn.h
 *
 * Connection abstraction
 *
 * Open Phone Abstraction Library (OPAL)
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
 * $Revision: 26728 $
 * $Author: rjongbloed $
 * $Date: 2011-12-01 22:59:14 -0600 (Thu, 01 Dec 2011) $
 */

#ifndef OPAL_OPAL_RTPCONN_H
#define OPAL_OPAL_RTPCONN_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <opal/buildopts.h>

#include <opal/connection.h>
#include <opal/mediatype.h>

#ifdef OPAL_ZRTP

class OpalZRTPStreamInfo {
  public:
    virtual bool Open() = 0;
    virtual RTP_UDP * CreateRTPSession(OpalConnection & conn, unsigned sessionId, bool remoteIsNat) = 0;
};

class OpalZRTPConnectionInfo {
  public:
    virtual bool Open() = 0;
    virtual RTP_UDP * CreateRTPSession(OpalConnection & conn, unsigned sessionId, bool remoteIsNat) = 0;

    PMutex mutex;
};

#endif // OPAL_ZRTP


class OpalRTPEndPoint;

//#ifdef HAS_LIBZRTP
//#ifndef __ZRTP_TYPES_H__
//struct zrtp_conn_ctx_t;
//#endif
//#endif

/** Class for carrying media session information
  */
class OpalMediaSession : public PObject
{
    PCLASSINFO(OpalMediaSession, PObject);
  public:
    OpalMediaSession(OpalConnection & conn, const OpalMediaType & _mediaType, unsigned sessionId);
    OpalMediaSession(const OpalMediaSession & _obj);

    virtual void Close() = 0;

    virtual PObject * Clone() const = 0;

    virtual bool IsActive() const = 0;

    virtual bool IsRTP() const = 0;

    virtual bool HasFailed() const = 0;

    virtual OpalTransportAddress GetLocalMediaAddress() const = 0;

    virtual void SetRemoteMediaAddress(const OpalTransportAddress &, const OpalMediaFormatList & ) { }

#if OPAL_SIP
    virtual SDPMediaDescription * CreateSDPMediaDescription(
      const OpalTransportAddress & localAddress
    ) = 0;
#endif

    virtual OpalMediaStream * CreateMediaStream(
      const OpalMediaFormat & mediaFormat, 
      unsigned sessionID, 
      PBoolean isSource
    ) = 0;

    OpalConnection & connection;
    OpalMediaType mediaType;     // media type for session
    unsigned sessionId;          // unique session ID
};


/** Class for carrying RTP session information
  */
class OpalRTPMediaSession : public OpalMediaSession
{
  PCLASSINFO(OpalRTPMediaSession, OpalMediaSession);
  public:
    OpalRTPMediaSession(
      OpalConnection & conn,
      const OpalMediaType & mediaType,
      unsigned sessionId
    );
    OpalRTPMediaSession(const OpalRTPMediaSession & obj);
    ~OpalRTPMediaSession();

    PObject * Clone() const { return new OpalRTPMediaSession(*this); }

    void Attach(RTP_Session * rtpSession);

    virtual void Close();

    virtual bool IsActive() const { return rtpSession != NULL; }

    virtual bool IsRTP() const { return true; }

    virtual bool HasFailed() const { return (rtpSession != NULL) && (rtpSession->HasFailed() || (rtpSession->GetPacketsReceived() == 0)); }

    virtual OpalTransportAddress GetLocalMediaAddress() const;

#if OPAL_SIP
    virtual SDPMediaDescription * CreateSDPMediaDescription(
      const OpalTransportAddress & localAddress
    );
#endif

    virtual OpalMediaStream * CreateMediaStream(
      const OpalMediaFormat & mediaFormat, 
      unsigned sessionID, 
      PBoolean isSource
    );

    RTP_Session * GetSession() const { return rtpSession; }

  protected:
    RTP_Session * rtpSession;    // RTP session
};

/**This class manages the RTP sessions for an OpalRTPConnection
 */
class OpalRTPSessionManager : public PObject
{
    PCLASSINFO(OpalRTPSessionManager , PObject);
  public:
  /**@name Construction */
  //@{
    /**Construct new session manager database.
      */
    OpalRTPSessionManager(
      OpalRTPConnection & connection ///< Owner connection for RTP sessions
    );

    /// Destroy sessions, unlinking from connection
    ~OpalRTPSessionManager();

    ///Copy constructor - maintain list in OpalRTPConnection
    OpalRTPSessionManager(const OpalRTPSessionManager & other);

    /// Assignment of RTP session managers.
    void operator=(const OpalRTPSessionManager & other) { sessions = other.sessions; }
  //@}

  /**@name Operations */
  //@{
    /**Get next available session ID for the media type.
      */
    unsigned GetNextSessionID();

    /**Add an RTP session for the specified ID.

       This function MUST be called only after the UseSession() function has
       returned NULL. The mutex flag is left locked in that case. This
       function expects the mutex to be locked and unlocks it automatically.
      */
    void AddSession(
      RTP_Session * session,          ///<  Session to add.
      const OpalMediaType & mediaType ///< initial media type for this session
    );
    void AddMediaSession(
      OpalMediaSession * session,          ///<  Session to add.
      const OpalMediaType & mediaType ///< initial media type for this session
    );

    /**Release the session.
     */
    virtual void CloseSession(
      unsigned sessionID     ///<  Session ID to release, 0 indicates all
    );

    /**Get a session for the specified ID.
     */
    RTP_Session * GetSession(
      unsigned sessionID    ///<  Session ID to get.
    ) const;
    OpalMediaSession * GetMediaSession(
      unsigned sessionID
    ) const;

    /**Change the sessionID for an existing session.
       This will adjust the RTP session and media streams.

       Return false if no such session exists.
      */
    bool ChangeSessionID(
      unsigned fromSessionID,   ///< Session ID to search for
      unsigned toSessionID      ///< Session ID to change to
    );
  //@}

    PMutex & GetMutex() { return m_mutex; }

    virtual bool AllSessionsFailing();

  protected:
    OpalRTPConnection & m_connection;
    PMutex m_mutex;

    PDICTIONARY(SessionDict, POrdinalKey, OpalMediaSession);
    SessionDict sessions;
};


typedef OpalRTPSessionManager RTP_SessionManager;


/**This is the base class for OpalConnections that use RTP sessions, 
   such as H.323 and SIPconnections to an endpoint.
 */
class OpalRTPConnection : public OpalConnection
{
    PCLASSINFO(OpalRTPConnection, OpalConnection);
  public:
  /**@name Construction */
  //@{
    /**Create a new connection.
     */
    OpalRTPConnection(
      OpalCall & call,                         ///<  Owner calll for connection
      OpalRTPEndPoint & endpoint,              ///<  Owner endpoint for connection
      const PString & token,                   ///<  Token to identify the connection
      unsigned options = 0,                    ///<  Connection options
      OpalConnection::StringOptions * stringOptions = NULL     ///< more complex options
    );  

    /**Destroy connection.
     */
    ~OpalRTPConnection();

    /**Clean up the termination of the connection.
       This function can do any internal cleaning up and waiting on background
       threads that may be using the connection object.

       Note that there is not a one to one relationship with the
       OnEstablishedConnection() function. This function may be called without
       that function being called. For example if SetUpConnection() was used
       but the call never completed.

       Classes that override this function should make sure they call the
       ancestor version for correct operation.

       An application will not typically call this function as it is used by
       the OpalManager during a release of the connection.

       The default behaviour calls the OpalEndPoint function of the same name.
      */
    virtual void OnReleased();
  //@}


  /**@name RTP Session Management */
  //@{
    /**Get next available session ID for the media type.
      */
    virtual unsigned GetNextSessionID(
      const OpalMediaType & mediaType,   ///< Media type of stream being opened
      bool isSource                      ///< Stream is a source/sink
    );

    /**Get an RTP session for the specified ID.
       If there is no session of the specified ID, NULL is returned.
      */
    virtual RTP_Session * GetSession(
      unsigned sessionID    ///<  RTP session number
    ) const;
    virtual OpalMediaSession * GetMediaSession(
      unsigned sessionID    ///<  RTP session number
    ) const;

    /**Use an RTP session for the specified ID.
       This will find a session of the specified ID and uses it if available.

       If there is no session of the specified ID one is created.

       The type of RTP session that is created will be compatible with the
       transport. At this time only IP (RTP over UDP) is supported.
      */
    virtual RTP_Session * UseSession(
      const OpalTransport & transport,  ///<  Transport of signalling
      unsigned sessionID,               ///<  RTP session number
      const OpalMediaType & mediatype,  ///<  media type
      RTP_QOS * rtpqos = NULL           ///<  Quiality of Service information
    );

    /**Release the session.
     */
    virtual void CloseSession(
      unsigned sessionID     ///<  RTP session number, 0 indicates all
    );

    /**Create and open a new RTP session.
       The type of RTP session that is created will be compatible with the
       transport. At this time only IP (RTP over UDP) is supported.
      */
    virtual RTP_Session * CreateSession(
      const OpalTransport & transport,
      unsigned sessionID,
      const OpalMediaType & mediaType,
      RTP_QOS * rtpqos
    );

    /** Create a new underlying RTP session instance.
      */
    virtual RTP_UDP * CreateRTPSession(
      unsigned sessionId,
      const OpalMediaType & mediaType,
      bool remoteIsNat
    );

    /**Change the sessionID for an existing session.
       This will adjust the RTP session and media streams.

       Return false if no such session exists.
      */
    virtual bool ChangeSessionID(
      unsigned fromSessionID,   ///< Session ID to search for
      unsigned toSessionID      ///< Session ID to change to
    );
  //@}

  /**@name NAT Management */
  //@{
    /** Return true if the remote appears to be behind a NAT firewall
    */
    virtual PBoolean RemoteIsNAT() const
    { return remoteIsNAT; }

    /**Determine if the RTP session needs to accommodate a NAT router.
       For endpoints that do not use STUN or something similar to set up all the
       correct protocol embeddded addresses correctly when a NAT router is between
       the endpoints, it is possible to still accommodate the call, with some
       restrictions. This function determines if the RTP can proceed with special
       NAT allowances.

       The special allowance is that the RTP code will ignore whatever the remote
       indicates in the protocol for the address to send RTP data and wait for
       the first packet to arrive from the remote and will then proceed to send
       all RTP data back to that address AND port.

       The default behaviour checks the values of the physical link
       (localAddr/peerAddr) against the signaling address the remote indicated in
       the protocol, eg H.323 SETUP sourceCallSignalAddress or SIP "To" or
       "Contact" fields, and makes a guess that the remote is behind a NAT router.
     */
    virtual PBoolean IsRTPNATEnabled(
      const PIPSocket::Address & localAddr,   ///< Local physical address of connection
      const PIPSocket::Address & peerAddr,    ///< Remote physical address of connection
      const PIPSocket::Address & signalAddr,  ///< Remotes signaling address as indicated by protocol of connection
      PBoolean incoming                       ///< Incoming/outgoing connection
    );
  //@}

    /**Attaches the RFC 2833 handler to the media patch
       This method may be called from subclasses, e.g. within
       OnPatchMediaStream()
      */
    virtual void AttachRFC2833HandlerToPatch(PBoolean isSource, OpalMediaPatch & patch);

    virtual PBoolean SendUserInputTone(
      char tone,        ///<  DTMF tone code
      unsigned duration = 0  ///<  Duration of tone in milliseconds
    );

    /**Meda information structure for GetMediaInformation() function.
      */
    struct MediaInformation {
      MediaInformation() { 
        rfc2833  = RTP_DataFrame::IllegalPayloadType; 
        ciscoNSE = RTP_DataFrame::IllegalPayloadType; 
      }

      OpalTransportAddress data;           ///<  Data channel address
      OpalTransportAddress control;        ///<  Control channel address
      RTP_DataFrame::PayloadTypes rfc2833; ///<  Payload type for RFC2833
      RTP_DataFrame::PayloadTypes ciscoNSE; ///<  Payload type for RFC2833
    };
  //@}

  /**@name Overrides from OpalConnection */
  //@{
    /**Get information on the media channel for the connection.
       The default behaviour checked the mediaTransportAddresses dictionary
       for the session ID and returns information based on that. It also uses
       the rfc2833Handler variable for that part of the info.

       It is up to the descendant class to assure that the mediaTransportAddresses
       dictionary is set correctly before OnIncomingCall() is executed.
     */
    virtual PBoolean GetMediaInformation(
      unsigned sessionID,     ///<  Session ID for media channel
      MediaInformation & info ///<  Information on media channel
    ) const;

    /**See if the media can bypass the local host.

       The default behaviour returns true if the session is audio or video.
     */
    virtual PBoolean IsMediaBypassPossible(
      unsigned sessionID                  ///<  Session ID for media channel
    ) const;

    /**Create a new media stream.
       This will create a media stream of an appropriate subclass as required
       by the underlying connection protocol. For instance H.323 would create
       an OpalRTPStream.

       The sessionID parameter may not be needed by a particular media stream
       and may be ignored. In the case of an OpalRTPStream it us used.

       Note that media streams may be created internally to the underlying
       protocol. This function is not the only way a stream can come into
       existance.
     */
    virtual OpalMediaStream * CreateMediaStream(
      const OpalMediaFormat & mediaFormat, ///<  Media format for stream
      unsigned sessionID,                  ///<  Session number for stream
      PBoolean isSource                        ///<  Is a source stream
    );

    /**Adjust media formats available on a connection.
       This is called by a connection after it has called
       OpalCall::GetMediaFormats() to get all media formats that it can use so
       that an application may remove or reorder the media formats before they
       are used to open media streams.

       This function may also be executed by other connections in the call. If
       this happens then the "otherConnection" parameter will be non-NULL. The
       "local" parameter sense is relative to the "otherConnection" parameter,
       if NULL then it is relative to "this".

       The default behaviour calls the OpalEndPoint function of the same name.
      */
    virtual void AdjustMediaFormats(
      bool local,                             ///<  Media formats a local ones to be presented to remote
      const OpalConnection * otherConnection, ///<  Other connection we are adjusting media for
      OpalMediaFormatList & mediaFormats      ///<  Media formats to use
    ) const;

    /**Call back when patching a media stream.
       This function is called when a connection has created a new media
       patch between two streams. This is usually called twice per media patch,
       once for the source stream and once for the sink stream.

       Note this is not called within the context of the patch thread and is
       called before that thread has started.
      */
    virtual void OnPatchMediaStream(
      PBoolean isSource,        ///< Is source/sink call
      OpalMediaPatch & patch    ///<  New patch
    );

    /** Callback for media commands.
        Calls the SendIntraFrameRequest on the rtp session

       @returns true if command is handled.
      */
    virtual bool OnMediaCommand(
      OpalMediaStream & stream,         ///< Stream command executed on
      const OpalMediaCommand & command  ///< Media command being executed
    );
  //@}

    virtual void SessionFailing(RTP_Session & session);

  protected:
    PDECLARE_NOTIFIER(OpalRFC2833Info, OpalRTPConnection, OnUserInputInlineRFC2833);
    PDECLARE_NOTIFIER(OpalRFC2833Info, OpalRTPConnection, OnUserInputInlineCiscoNSE);

    OpalRTPSessionManager m_rtpSessions;
    OpalRFC2833Proto * rfc2833Handler;
#if OPAL_T38_CAPABILITY
    OpalRFC2833Proto * ciscoNSEHandler;
#endif

    PBoolean remoteIsNAT;
    PBoolean useRTPAggregation;

#ifdef OPAL_ZRTP
    bool zrtpEnabled;
    PMutex zrtpConnInfoMutex;
    OpalZRTPConnectionInfo * zrtpConnInfo;
#endif
};


class RTP_UDP;

class OpalSecurityMode : public PObject
{
  PCLASSINFO(OpalSecurityMode, PObject);
  public:
    virtual RTP_UDP * CreateRTPSession(
      OpalRTPConnection & connection,     ///< Connection creating session (may be needed by secure connections)
      const RTP_Session::Params & options ///< Parameters to construct with session.
    ) = 0;
    virtual PBoolean Open() = 0;
};

#endif // OPAL_OPAL_RTPCONN_H
