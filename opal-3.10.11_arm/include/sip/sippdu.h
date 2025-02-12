/*
 * sippdu.h
 *
 * Session Initiation Protocol PDU support.
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
 *
 * Copyright (c) 2002 Equivalence Pty. Ltd.
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
 * $Revision: 29598 $
 * $Author: ededu $
 * $Date: 2013-04-29 12:22:45 -0500 (Mon, 29 Apr 2013) $
 */

#ifndef OPAL_SIP_SIPPDU_H
#define OPAL_SIP_SIPPDU_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <opal/buildopts.h>

#if OPAL_SIP

#include <ptclib/mime.h>
#include <ptclib/url.h>
#include <ptclib/http.h>
#include <sip/sdp.h>
#include <opal/rtpconn.h>

 
class OpalTransport;
class OpalTransportAddress;
class OpalProductInfo;

class SIPEndPoint;
class SIPConnection;
class SIP_PDU;
class SIPSubscribeHandler;
class SIPDialogContext;
class SIPMIMEInfo;


/////////////////////////////////////////////////////////////////////////
// SIPURL

/** This class extends PURL to include displayname, optional "<>" delimiters
	and extended parameters - like tag.
	It may be used for From:, To: and Contact: lines.
 */

class SIPURL : public PURL
{
  PCLASSINFO(SIPURL, PURL);
  public:
    SIPURL();

    SIPURL(
      const PURL & url
    ) : PURL(url) { }
    SIPURL & operator=(
      const PURL & url
    ) { PURL::operator=(url); return *this; }

    /** str goes straight to Parse()
      */
    SIPURL(
      const char * cstr,    ///<  C string representation of the URL.
      const char * defaultScheme = NULL ///<  Default scheme for URL
    );
    SIPURL & operator=(
      const char * cstr
    ) { Parse(cstr); return *this; }

    /** str goes straight to Parse()
      */
    SIPURL(
      const PString & str,  ///<  String representation of the URL.
      const char * defaultScheme = NULL ///<  Default scheme for URL
    );
    SIPURL & operator=(
      const PString & str
    ) { Parse(str); return *this; }

    /** If name does not start with 'sip' then construct URI in the form
        <pre><code>
          sip:name\@host:port;transport=transport
        </code></pre>
        where host comes from address,
        port is listenerPort or port from address if that was 0
        transport is udp unless address specified tcp
        Send name starting with 'sip' or constructed URI to Parse()
     */
    SIPURL(
      const PString & name,
      const OpalTransportAddress & address,
      WORD listenerPort = 0
    );

    SIPURL(
      const OpalTransportAddress & address, 
      WORD listenerPort = 0
    );
    SIPURL & operator=(
      const OpalTransportAddress & address
    );

    SIPURL(
      const SIPMIMEInfo & mime,
      const char * name
    );

    /**Compare the two SIPURLs and return their relative rank.
       Note that does an intelligent comparison according to the rules
       in RFC3261 Section 19.1.4.

     @return
       <code>LessThan</code>, <code>EqualTo</code> or <code>GreaterThan</code>
       according to the relative rank of the objects.
     */
    virtual Comparison Compare(
      const PObject & obj   ///< Object to compare against.
    ) const;

    /** Returns complete SIPURL as one string, including displayname (in
        quotes) and address in angle brackets.
      */
    PString AsQuotedString() const;

    /** Returns display name only
      */
    PString GetDisplayName(PBoolean useDefault = true) const;
    
    void SetDisplayName(const PString & str) 
    {
      m_displayName = str;
    }

    /// Return string options in field parameters
    const PStringOptions & GetFieldParameters() const { return m_fieldParameters; }
          PStringOptions & GetFieldParameters()       { return m_fieldParameters; }

    /**Get the host and port as a transport address.
      */
    OpalTransportAddress GetHostAddress() const;

    /**Set the host and port as a transport address.
      */
    void SetHostAddress(const OpalTransportAddress & addr);

    enum UsageContext {
      ExternalURI,   ///< URI used anywhere outside of protocol
      RequestURI,    ///< Request-URI (after the INVITE)
      ToURI,         ///< To header field
      FromURI,       ///< From header field
      RouteURI,      ///< Record-Route header field
      RedirectURI,   ///< Redirect Contact header field
      ContactURI,    ///< General Contact header field
      RegContactURI, ///< Registration Contact header field
      RegisterURI    ///< URI on REGISTER request line.
    };

    /** Removes tag parm & query vars and recalculates urlString
        (scheme, user, password, host, port & URI parms (like transport))
        which are not allowed in the context specified, e.g. Request-URI etc
        According to RFC3261, 19.1.1 Table 1
      */
    void Sanitise(
      UsageContext context  ///< Context for URI
    );

    /** This will adjust the current URL according to RFC3263, using DNS SRV records.

        @return FALSE if DNS is available but entry is larger than last SRV record entry,
                TRUE if DNS lookup fails or no DNS is available
      */
    PBoolean AdjustToDNS(
      PINDEX entry = 0  /// Entry in the SRV record to adjust to
    );

    /// Generate a unique string suitable as a dialog tag
    static PString GenerateTag();

    /// Set a tag with a new unique ID.
    void SetTag(
      const PString & tag = PString::Empty(),
      bool force = false
    );

  protected:
    void ParseAsAddress(const PString & name, const OpalTransportAddress & _address, WORD listenerPort = 0);

    // Override from PURL()
    virtual PBoolean InternalParse(
      const char * cstr,
      const char * defaultScheme
    ) { return ReallyInternalParse(false, cstr, defaultScheme); }

    bool ReallyInternalParse(
      bool fromField,
      const char * cstr,
      const char * defaultScheme
    );

    PString        m_displayName;
    PStringOptions m_fieldParameters;
};


class SIPURLList : public std::list<SIPURL>
{
  public:
    bool FromString(
      const PString & str,
      SIPURL::UsageContext context = SIPURL::RouteURI,
      bool reversed = false
    );
    PString ToString() const;
    friend ostream & operator<<(ostream & strm, const SIPURLList & urls);
};



/////////////////////////////////////////////////////////////////////////
// SIPMIMEInfo

/** Session Initiation Protocol MIME info container
   This is a string dictionary: for each item mime header is key, value
   is value.
   Headers may be full ("From") or compact ("f"). Colons not included.
   PMIMEInfo::ReadFrom (>>) parses from stream. That adds a header-value
   element for each mime line. If a mime header is duplicated in the
   stream then the additional value is appended to the existing, 
   separated by "/n".
   PMIMEInfo::ReadFrom supports multi-line values if the next line starts
   with a space - it just appends the next line to the existing string
   with the separating space.
   There is no checking of header names or values.
   compactForm decides whether 'Set' methods store full or compact headers.
   'Set' methods replace values, there is no method for appending except
   ReadFrom.
   'Get' methods work whether stored headers are full or compact.

   to do to satisfy RFC3261 (mandatory(*) & should):
    Accept
    Accept-Encoding
    Accept-Language
   *Allow
   *Max-Forwards
   *Min-Expires
   *Proxy-Authenticate
    Supported
   *Unsupported
   *WWW-Authenticate
 */

class SIPMIMEInfo : public PMIMEInfo
{
  PCLASSINFO(SIPMIMEInfo, PMIMEInfo);
  public:
    SIPMIMEInfo(bool compactForm = false);

    virtual void PrintOn(ostream & strm) const;
    virtual bool InternalAddMIME(const PString & fieldName, const PString & fieldValue);

    void SetCompactForm(bool form) { compactForm = form; }

    PCaselessString GetContentType(bool includeParameters = false) const;
    void SetContentType(const PString & v);

    PCaselessString GetContentEncoding() const;
    void SetContentEncoding(const PString & v);

    SIPURL GetFrom() const;
    void SetFrom(const PString & v);

    SIPURL GetPAssertedIdentity() const;
    void SetPAssertedIdentity(const PString & v);

    SIPURL GetPPreferredIdentity() const;
    void SetPPreferredIdentity(const PString & v);

    PString GetAccept() const;
    void SetAccept(const PString & v);

    PString GetAcceptEncoding() const;
    void SetAcceptEncoding(const PString & v);

    PString GetAcceptLanguage() const;
    void SetAcceptLanguage(const PString & v);

    PString GetAllow() const;
    unsigned GetAllowBitMask() const;
    void SetAllow(const PString & v);

    PString GetCallID() const;
    void SetCallID(const PString & v);

    SIPURL GetContact() const;
    bool GetContacts(SIPURLList & contacts) const;
    void SetContact(const PString & v);

    PString GetSubject() const;
    void SetSubject(const PString & v);

    SIPURL GetTo() const;
    void SetTo(const PString & v);

    PString GetVia() const;
    void SetVia(const PString & v);

    bool GetViaList(PStringList & v) const;
    void SetViaList(const PStringList & v);

    PString GetFirstVia() const;
    OpalTransportAddress GetViaReceivedAddress() const;

    SIPURL GetReferTo() const;
    void SetReferTo(const PString & r);

    SIPURL GetReferredBy() const;
    void SetReferredBy(const PString & r);

    PINDEX  GetContentLength() const;
    void SetContentLength(PINDEX v);
    PBoolean IsContentLengthPresent() const;

    PString GetCSeq() const;
    void SetCSeq(const PString & v);

    PString GetDate() const;
    void SetDate(const PString & v);
    void SetDate(const PTime & t);
    void SetDate(void); // set to current date

    unsigned GetExpires(unsigned dflt = UINT_MAX) const;// returns default value if not found
    void SetExpires(unsigned v);

    PINDEX GetMaxForwards() const;
    void SetMaxForwards(PINDEX v);

    PINDEX GetMinExpires() const;
    void SetMinExpires(PINDEX v);

    PString GetProxyAuthenticate() const;
    void SetProxyAuthenticate(const PString & v);

    PString GetRoute() const;
    bool GetRoute(SIPURLList & proxies) const;
    void SetRoute(const PString & v);
    void SetRoute(const SIPURLList & proxies);

    PString GetRecordRoute() const;
    bool GetRecordRoute(SIPURLList & proxies, bool reversed) const;
    void SetRecordRoute(const PString & v);
    void SetRecordRoute(const SIPURLList & proxies);

    unsigned GetCSeqIndex() const { return GetCSeq().AsUnsigned(); }

    PStringSet GetRequire() const;
    void SetRequire(const PStringSet & v);
    void AddRequire(const PString & v);

    PStringSet GetSupported() const;
    void SetSupported(const PStringSet & v);
    void AddSupported(const PString & v);

    PStringSet GetUnsupported() const;
    void SetUnsupported(const PStringSet & v);
    void AddUnsupported(const PString & v);
    
    PString GetEvent() const;
    void SetEvent(const PString & v);
    
    PCaselessString GetSubscriptionState(PStringToString & info) const;
    void SetSubscriptionState(const PString & v);
    
    PString GetUserAgent() const;
    void SetUserAgent(const PString & v);

    PString GetOrganization() const;
    void SetOrganization(const PString & v);

    void GetProductInfo(OpalProductInfo & info) const;
    void SetProductInfo(const PString & ua, const OpalProductInfo & info);

    PString GetWWWAuthenticate() const;
    void SetWWWAuthenticate(const PString & v);

    PString GetSIPIfMatch() const;
    void SetSIPIfMatch(const PString & v);

    PString GetSIPETag() const;
    void SetSIPETag(const PString & v);

    void GetAlertInfo(PString & info, int & appearance);
    void SetAlertInfo(const PString & info, int appearance);

    PString GetCallInfo() const;

    PString GetAllowEvents() const;
    void SetAllowEvents(const PString & v);

    /** return the value of a header field parameter, empty if none
     */
    PString GetFieldParameter(
      const PString & fieldName,    ///< Field name in dictionary
      const PString & paramName,    ///< Field parameter name
      const PString & defaultValue = PString::Empty()  ///< Default value for parameter
    ) const { return ExtractFieldParameter((*this)(fieldName), paramName, defaultValue); }

    /** set the value for a header field parameter, replace the
     *  current value, or add the parameter and its
     *  value if not already present.
     */
    void SetFieldParameter(
      const PString & fieldName,    ///< Field name in dictionary
      const PString & paramName,    ///< Field parameter name
      const PString & newValue      ///< New value for parameter
    ) { SetAt(fieldName, InsertFieldParameter((*this)(fieldName), paramName, newValue)); }

    /** return the value of a header field parameter, empty if none
     */
    static PString ExtractFieldParameter(
      const PString & fieldValue,   ///< Value of field string
      const PString & paramName,    ///< Field parameter name
      const PString & defaultValue = PString::Empty()  ///< Default value for parameter
    );

    /** set the value for a header field parameter, replace the
     *  current value, or add the parameter and its
     *  value if not already present.
     */
    static PString InsertFieldParameter(
      const PString & fieldValue,   ///< Value of field string
      const PString & paramName,    ///< Field parameter name
      const PString & newValue      ///< New value for parameter
    );

  protected:
    PStringSet GetTokenSet(const char * field) const;
    void AddTokenSet(const char * field, const PString & token);
    void SetTokenSet(const char * field, const PStringSet & tokens);

    /// Encode using compact form
    bool compactForm;
};


/////////////////////////////////////////////////////////////////////////
// SIPAuthentication

typedef PHTTPClientAuthentication SIPAuthentication;

class SIPAuthenticator : public PHTTPClientAuthentication::AuthObject
{
  public:
    SIPAuthenticator(SIP_PDU & pdu);
    virtual PMIMEInfo & GetMIME();
    virtual PString GetURI();
    virtual PString GetEntityBody();
    virtual PString GetMethod();

  protected:  
    SIP_PDU & m_pdu;
};



/////////////////////////////////////////////////////////////////////////
// SIP_PDU

/** Session Initiation Protocol message.
	Each message contains a header, MIME lines and possibly SDP.
	Class provides methods for reading from and writing to transport.
 */

class SIP_PDU : public PSafeObject
{
  PCLASSINFO(SIP_PDU, PSafeObject);
  public:
    enum Methods {
      Method_INVITE,
      Method_ACK,
      Method_OPTIONS,
      Method_BYE,
      Method_CANCEL,
      Method_REGISTER,
      Method_SUBSCRIBE,
      Method_NOTIFY,
      Method_REFER,
      Method_MESSAGE,
      Method_INFO,
      Method_PING,
      Method_PUBLISH,
      Method_PRACK,
      NumMethods
    };

    enum StatusCodes {
      IllegalStatusCode,
      Local_TransportError,
      Local_BadTransportAddress,
      Local_Timeout,

      Information_Trying                  = 100,
      Information_Ringing                 = 180,
      Information_CallForwarded           = 181,
      Information_Queued                  = 182,
      Information_Session_Progress        = 183,

      Successful_OK                       = 200,
      Successful_Accepted		          = 202,

      Redirection_MultipleChoices         = 300,
      Redirection_MovedPermanently        = 301,
      Redirection_MovedTemporarily        = 302,
      Redirection_UseProxy                = 305,
      Redirection_AlternativeService      = 380,

      Failure_BadRequest                  = 400,
      Failure_UnAuthorised                = 401,
      Failure_PaymentRequired             = 402,
      Failure_Forbidden                   = 403,
      Failure_NotFound                    = 404,
      Failure_MethodNotAllowed            = 405,
      Failure_NotAcceptable               = 406,
      Failure_ProxyAuthenticationRequired = 407,
      Failure_RequestTimeout              = 408,
      Failure_Conflict                    = 409,
      Failure_Gone                        = 410,
      Failure_LengthRequired              = 411,
      Failure_RequestEntityTooLarge       = 413,
      Failure_RequestURITooLong           = 414,
      Failure_UnsupportedMediaType        = 415,
      Failure_UnsupportedURIScheme        = 416,
      Failure_BadExtension                = 420,
      Failure_ExtensionRequired           = 421,
      Failure_IntervalTooBrief            = 423,
      Failure_TemporarilyUnavailable      = 480,
      Failure_TransactionDoesNotExist     = 481,
      Failure_LoopDetected                = 482,
      Failure_TooManyHops                 = 483,
      Failure_AddressIncomplete           = 484,
      Failure_Ambiguous                   = 485,
      Failure_BusyHere                    = 486,
      Failure_RequestTerminated           = 487,
      Failure_NotAcceptableHere           = 488,
      Failure_BadEvent                    = 489,
      Failure_RequestPending              = 491,
      Failure_Undecipherable              = 493,

      Failure_InternalServerError         = 500,
      Failure_NotImplemented              = 501,
      Failure_BadGateway                  = 502,
      Failure_ServiceUnavailable          = 503,
      Failure_ServerTimeout               = 504,
      Failure_SIPVersionNotSupported      = 505,
      Failure_MessageTooLarge             = 513,

      GlobalFailure_BusyEverywhere        = 600,
      GlobalFailure_Decline               = 603,
      GlobalFailure_DoesNotExistAnywhere  = 604,
      GlobalFailure_NotAcceptable         = 606,

      MaxStatusCode                       = 699
    };

    static const char * GetStatusCodeDescription(int code);
    friend ostream & operator<<(ostream & strm, StatusCodes status);

    SIP_PDU(
      Methods method = SIP_PDU::NumMethods
    );

    /** Construct a Response message
        extra is passed as message body
     */
    SIP_PDU(
      const SIP_PDU & request,
      StatusCodes code,
      const SDPSessionDescription * sdp = NULL
    );

    SIP_PDU(const SIP_PDU &);
    SIP_PDU & operator=(const SIP_PDU &);
    ~SIP_PDU();

    void PrintOn(
      ostream & strm
    ) const;

    void InitialiseHeaders(
      const SIPURL & dest,
      const SIPURL & to,
      const SIPURL & from,
      const PString & callID,
      unsigned cseq,
      const PString & via
    );
    void InitialiseHeaders(
      SIPDialogContext & dialog,
      const PString & via = PString::Empty(),
      unsigned cseq = 0
    );
    void InitialiseHeaders(
      SIPConnection & connection,
      const OpalTransport & transport,
      unsigned cseq = 0
    );
    void InitialiseHeaders(
      const SIP_PDU & request
    );

    /**Add and populate Route header following the given routeSet.
      If first route is strict, exchange with URI.
      Returns true if routeSet.
      */
    bool SetRoute(const SIPURLList & routeSet);
    bool SetRoute(const SIPURL & proxy);

    /**Set mime allow field to all supported methods.
      */
    void SetAllow(unsigned bitmask);

    /**Update the VIA field following RFC3261, 18.2.1 and RFC3581.
      */
    void AdjustVia(OpalTransport & transport);

    PString CreateVia(
      SIPEndPoint & endpoint,
      const OpalTransport & transport,
      SIPConnection * connection = NULL
    );

    /**Read PDU from the specified transport.
      */
    SIP_PDU::StatusCodes Read(
      OpalTransport & transport
    );

    /**Write the PDU to the transport.
      */
    PBoolean Write(
      OpalTransport & transport,
      const OpalTransportAddress & remoteAddress = OpalTransportAddress(),
      const PString & localInterface = PString::Empty()
    );

    /**Write PDU as a response to a request.
    */
    bool SendResponse(
      OpalTransport & transport,
      StatusCodes code,
      SIPEndPoint * endpoint = NULL
    ) const;
    bool SendResponse(
      OpalTransport & transport,
      SIP_PDU & response,
      SIPEndPoint * endpoint = NULL
    ) const;

    /** Construct the PDU string to output.
        Returns the total length of the PDU.
      */
    PString Build();

    PString GetTransactionID() const;

    Methods GetMethod() const                { return m_method; }
    StatusCodes GetStatusCode () const       { return m_statusCode; }
    void SetStatusCode (StatusCodes c)       { m_statusCode = c; }
    const SIPURL & GetURI() const            { return m_uri; }
    void SetURI(const SIPURL & newuri)       { m_uri = newuri; }
    unsigned GetVersionMajor() const         { return m_versionMajor; }
    unsigned GetVersionMinor() const         { return m_versionMinor; }
    void SetCSeq(unsigned cseq);
    const PString & GetEntityBody() const    { return m_entityBody; }
    void SetEntityBody(const PString & body) { m_entityBody = body; }
    void SetEntityBody();
    const PString & GetInfo() const          { return m_info; }
    void SetInfo(const PString & info)       { m_info = info; }
    const SIPMIMEInfo & GetMIME() const      { return m_mime; }
          SIPMIMEInfo & GetMIME()            { return m_mime; }
    SDPSessionDescription * GetSDP(const OpalMediaFormatList & masterList);
    void SetSDP(SDPSessionDescription * sdp);

  protected:
    Methods     m_method;                 // Request type, ==NumMethods for Response
    StatusCodes m_statusCode;
    SIPURL      m_uri;                    // display name & URI, no tag
    unsigned    m_versionMajor;
    unsigned    m_versionMinor;
    PString     m_info;
    SIPMIMEInfo m_mime;
    PString     m_entityBody;

    SDPSessionDescription * m_SDP;

    mutable PString m_transactionID;
};


PQUEUE(SIP_PDU_Queue, SIP_PDU);


#if PTRACING
ostream & operator<<(ostream & strm, SIP_PDU::Methods method);
#endif


/////////////////////////////////////////////////////////////////////////
// SIPDialogContext

/** Session Initiation Protocol dialog context information.
  */
class SIPDialogContext
{
  public:
    SIPDialogContext();
    SIPDialogContext(const SIPMIMEInfo & mime);

    PString AsString() const;
    bool FromString(
      const PString & str
    );

    const PString & GetCallID() const { return m_callId; }
    void SetCallID(const PString & id) { m_callId = id; }

    const SIPURL & GetRequestURI() const { return m_requestURI; }
    void SetRequestURI(const SIPURL & url) { m_requestURI = url; }

    const PString & GetLocalTag() const { return m_localTag; }
    void SetLocalTag(const PString & tag) { m_localTag = tag; }

    const SIPURL & GetLocalURI() const { return m_localURI; }
    void SetLocalURI(const SIPURL & url);

    const PString & GetRemoteTag() const { return m_remoteTag; }
    void SetRemoteTag(const PString & tag) { m_remoteTag = tag; }

    const SIPURL & GetRemoteURI() const { return m_remoteURI; }
    void SetRemoteURI(const SIPURL & url);

    const SIPURLList & GetRouteSet() const { return m_routeSet; }
    void SetRouteSet(const PString & str) { m_routeSet.FromString(str); }

    const SIPURL & GetProxy() const { return m_proxy; }
    void SetProxy(const SIPURL & proxy, bool addToRouteSet);

    void Update(OpalTransport & transport, const SIP_PDU & response);

    unsigned GetNextCSeq();
    void IncrementCSeq(unsigned inc) { m_lastSentCSeq += inc; }

    bool IsDuplicateCSeq(unsigned sequenceNumber);

    bool IsEstablished() const
    {
      return !m_callId.IsEmpty() &&
             !m_requestURI.IsEmpty() &&
             !m_localTag.IsEmpty() &&
             !m_remoteTag.IsEmpty();
    }

    OpalTransportAddress GetRemoteTransportAddress() const;

    void SetForking(bool f) { m_forking = f; }

  protected:
    PString     m_callId;
    SIPURL      m_requestURI;
    SIPURL      m_localURI;
    PString     m_localTag;
    SIPURL      m_remoteURI;
    PString     m_remoteTag;
    SIPURLList  m_routeSet;
    unsigned    m_lastSentCSeq;
    unsigned    m_lastReceivedCSeq;
    OpalTransportAddress m_externalTransportAddress;
    bool        m_forking;
    SIPURL      m_proxy;
};


/////////////////////////////////////////////////////////////////////////

struct SIPParameters
{
  SIPParameters(
    const PString & aor = PString::Empty(),
    const PString & remote = PString::Empty()
  );

  void Normalise(
    const PString & defaultUser,
    const PTimeInterval & defaultExpire
  );

  PCaselessString m_remoteAddress;
  PCaselessString m_localAddress;
  PCaselessString m_proxyAddress;
  PCaselessString m_addressOfRecord;
  PCaselessString m_contactAddress;
  PCaselessString m_interface;
  SIPMIMEInfo     m_mime;
  PString         m_authID;
  PString         m_password;
  PString         m_realm;
  unsigned        m_expire;
  unsigned        m_restoreTime;
  PTimeInterval   m_minRetryTime;
  PTimeInterval   m_maxRetryTime;
  void          * m_userData;
};


#if PTRACING
ostream & operator<<(ostream & strm, const SIPParameters & params);
#endif


/////////////////////////////////////////////////////////////////////////
// SIPTransaction

/** Session Initiation Protocol transaction.
    A transaction is a stateful independent entity that provides services to
    a connection (Transaction User). Transactions are contained within 
    connections.
    A client transaction handles sending a request and receiving its
    responses.
    A server transaction handles sending responses to a received request.
    In either case the SIP_PDU ancestor is the sent or received request.
 */

class SIPTransaction : public SIP_PDU
{
    PCLASSINFO(SIPTransaction, SIP_PDU);
  public:
    SIPTransaction(
      Methods method,
      SIPEndPoint   & endpoint,
      OpalTransport & transport
    );
    /** Construct a transaction for requests in a dialog.
     *  The transport is used to determine the local address
     */
    SIPTransaction(
      Methods method,
      SIPConnection & connection
    );
    ~SIPTransaction();

    /* Under some circumstances a new transaction with all the same parameters
       but different ID needs to be created, e.g. when get authentication error. */
    virtual SIPTransaction * CreateDuplicate() const = 0;

    PBoolean Start();
    bool IsTrying()     const { return m_state == Trying; }
    bool IsProceeding() const { return m_state == Proceeding; }
    bool IsInProgress() const { return m_state == Trying || m_state == Proceeding; }
    bool IsFailed()     const { return m_state > Terminated_Success; }
    bool IsCompleted()  const { return m_state >= Completed; }
    bool IsCanceled()   const { return m_state == Cancelling || m_state == Terminated_Cancelled || m_state == Terminated_Aborted; }
    bool IsTerminated() const { return m_state >= Terminated_Success; }

    void WaitForCompletion();
    PBoolean Cancel();
    void Abort();

    virtual PBoolean OnReceivedResponse(SIP_PDU & response);
    virtual PBoolean OnCompleted(SIP_PDU & response);

    OpalTransport & GetTransport()  const { return m_transport; }
    SIPConnection * GetConnection() const { return m_connection; }
    PString         GetInterface()  const { return m_localInterface; }
    void            SetInterface(const PString & localIf)  { m_localInterface = localIf; }

    static PString GenerateCallID();

  protected:
    bool SendPDU(SIP_PDU & pdu);
    bool ResendCANCEL();
    void SetParameters(const SIPParameters & params);

    PDECLARE_NOTIFIER(PTimer, SIPTransaction, OnRetry);
    PDECLARE_NOTIFIER(PTimer, SIPTransaction, OnTimeout);

    enum States {
      NotStarted,
      Trying,
      Proceeding,
      Cancelling,
      Completed,
      Terminated_Success,
      Terminated_Timeout,
      Terminated_RetriesExceeded,
      Terminated_TransportError,
      Terminated_Cancelled,
      Terminated_Aborted,
      NumStates
    };
    virtual void SetTerminated(States newState);

    SIPEndPoint           & m_endpoint;
    OpalTransport         & m_transport;
    PSafePtr<SIPConnection> m_connection;
    PTimeInterval           m_retryTimeoutMin; 
    PTimeInterval           m_retryTimeoutMax; 

    States     m_state;
    unsigned   m_retry;
    PTimer     m_retryTimer;
    PTimer     m_completionTimer;
    PSyncPoint m_completed;

    PString              m_localInterface;
    OpalTransportAddress m_remoteAddress;
};


#define OPAL_PROXY_PARAM     "OPAL-proxy"
#define OPAL_LOCAL_ID_PARAM  "OPAL-local-id"
#define OPAL_INTERFACE_PARAM "OPAL-interface"


/////////////////////////////////////////////////////////////////////////
// SIPResponse

/** When we receive a command, we need a transaction to send repeated responses.
 */
class SIPResponse : public SIPTransaction
{
    PCLASSINFO(SIPResponse, SIPTransaction);
  public:
    SIPResponse(
      SIPEndPoint   & endpoint,
      StatusCodes code
    );

    virtual SIPTransaction * CreateDuplicate() const;

    bool Send(OpalTransport & transport, const SIP_PDU & command);
};


/////////////////////////////////////////////////////////////////////////
// SIPInvite

/** Session Initiation Protocol transaction for INVITE
    INVITE implements a three-way handshake to handle the human input and 
    extended duration of the transaction.
 */

class SIPInvite : public SIPTransaction
{
    PCLASSINFO(SIPInvite, SIPTransaction);
  public:
    SIPInvite(
      SIPConnection & connection,
      const OpalRTPSessionManager & sm
    );

    virtual SIPTransaction * CreateDuplicate() const;

    virtual PBoolean OnReceivedResponse(SIP_PDU & response);

    const OpalRTPSessionManager & GetSessionManager() const { return m_rtpSessions; }
          OpalRTPSessionManager & GetSessionManager()       { return m_rtpSessions; }

  protected:
    OpalRTPSessionManager m_rtpSessions;
};


/////////////////////////////////////////////////////////////////////////

/* This is the ACK request sent when receiving a response to an outgoing
 * INVITE.
 */
class SIPAck : public SIP_PDU
{
    PCLASSINFO(SIPAck, SIP_PDU);
  public:
    SIPAck(
      SIPTransaction & invite,
      SIP_PDU & response
    );

    virtual SIPTransaction * CreateDuplicate() const;
};


/////////////////////////////////////////////////////////////////////////

/* This is a BYE request
 */
class SIPBye : public SIPTransaction
{
    PCLASSINFO(SIPBye, SIPTransaction);
    
  public:
    SIPBye(
      SIPEndPoint & ep,
      OpalTransport & trans,
      SIPDialogContext dialog
    );
    SIPBye(
      SIPConnection & conn
    );

    virtual SIPTransaction * CreateDuplicate() const;
};


/////////////////////////////////////////////////////////////////////////

class SIPRegister : public SIPTransaction
{
    PCLASSINFO(SIPRegister, SIPTransaction);
  public:
    enum CompatibilityModes {
      e_FullyCompliant,                 /**< Registrar is fully compliant, we will register
                                             all listeners of all types (e.g. sip, sips etc)
                                             in the Contact field. */
      e_CannotRegisterMultipleContacts, /**< Registrar refuses to register more than one
                                             contact field. Correct behaviour is to return
                                             a contact with the fields it can accept in
                                             the 200 OK */
      e_CannotRegisterPrivateContacts,  /**< Registrar refuses to register any RFC
                                             contact field. Correct behaviour is to return
                                             a contact with the fields it can accept in
                                             the 200 OK */
      e_HasApplicationLayerGateway      /**< Router has Application Layer Gateway code that
                                             is doing address transations, so we do not try
                                             to do it ourselves as well or it goes horribly
                                             wrong. */
    };

    /// Registrar parameters
    struct Params : public SIPParameters {
      Params()
        : m_registrarAddress(m_remoteAddress)
        , m_compatibility(SIPRegister::e_FullyCompliant)
      { }

      Params(const Params & param)
        : SIPParameters(param)
        , m_registrarAddress(m_remoteAddress)
        , m_compatibility(param.m_compatibility)
      { }

      PCaselessString  & m_registrarAddress; // For backward compatibility
      CompatibilityModes m_compatibility;
    };

    SIPRegister(
      SIPEndPoint   & endpoint,
      OpalTransport & transport,
      const PString & callId,
      unsigned cseq,
      const Params & params
    );

    virtual SIPTransaction * CreateDuplicate() const;
};


#if PTRACING
ostream & operator<<(ostream & strm, SIPRegister::CompatibilityModes mode);
ostream & operator<<(ostream & strm, const SIPRegister::Params & params);
#endif


/////////////////////////////////////////////////////////////////////////

class SIPSubscribe : public SIPTransaction
{
    PCLASSINFO(SIPSubscribe, SIPTransaction);
  public:
    /** Valid types for an event package
     */
    enum PredefinedPackages {
      MessageSummary,
      Presence,
      Dialog,

      NumPredefinedPackages,

      Watcher = 0x8000,

      MessageSummaryWatcher = Watcher|MessageSummary,
      PresenceWatcher       = Watcher|Presence,
      DialogWatcher         = Watcher|Dialog,

      PackageMask = Watcher-1
    };
    friend PredefinedPackages operator|(PredefinedPackages p1, PredefinedPackages p2) { return (PredefinedPackages)((int)p1|(int)p2); }

    class EventPackage : public PCaselessString
    {
      PCLASSINFO(EventPackage, PCaselessString);
      public:
        EventPackage(PredefinedPackages = NumPredefinedPackages);
        explicit EventPackage(const PString & str) : PCaselessString(str) { }
        explicit EventPackage(const char   *  str) : PCaselessString(str) { }

        EventPackage & operator=(PredefinedPackages pkg);
        EventPackage & operator=(const PString & str) { PCaselessString::operator=(str); return *this; }
        EventPackage & operator=(const char   *  str) { PCaselessString::operator=(str); return *this; }

        bool operator==(PredefinedPackages pkg) const { return Compare(EventPackage(pkg)) == EqualTo; }
        bool operator==(const PString & str) const { return Compare(str) == EqualTo; }
        bool operator==(const char * cstr) const { return InternalCompare(0, P_MAX_INDEX, cstr) == EqualTo; }
        virtual Comparison InternalCompare(PINDEX offset, PINDEX length, const char * cstr) const;

        bool IsWatcher() const;
    };

    /** Information provided on the subscription status. */
    struct SubscriptionStatus {
      SIPSubscribeHandler * m_handler;           ///< Handler for subscription
      PString               m_addressofRecord;   ///< Address of record for registration
      bool                  m_wasSubscribing;    ///< Was registering or unregistering
      bool                  m_reSubscribing;     ///< Was a registration refresh
      SIP_PDU::StatusCodes  m_reason;            ///< Reason for status change
      OpalProductInfo       m_productInfo;       ///< Server product info from registrar if available.
      void                * m_userData;          ///< User data corresponding to this registration
    };

    struct NotifyCallbackInfo {
      NotifyCallbackInfo(
        SIPEndPoint & ep,
        OpalTransport & trans,
        SIP_PDU & notify,
        SIP_PDU & response
      );

      bool SendResponse(
        SIP_PDU::StatusCodes status,
        const char * extra = NULL
      );

      SIPEndPoint   & m_endpoint;
      OpalTransport & m_transport;
      SIP_PDU       & m_notify;
      SIP_PDU       & m_response;
      bool            m_sendResponse;
    };

    struct Params : public SIPParameters
    {
      Params(PredefinedPackages pkg = NumPredefinedPackages)
        : m_agentAddress(m_remoteAddress)
        , m_eventPackage(pkg)
        , m_eventList(false)
      { }

      Params(const Params & param)
        : SIPParameters(param)
        , m_agentAddress(m_remoteAddress)
        , m_eventPackage(param.m_eventPackage)
        , m_eventList(param.m_eventList)
        , m_contentType(param.m_contentType)
        , m_onSubcribeStatus(param.m_onSubcribeStatus)
        , m_onNotify(param.m_onNotify)
      { }

      PCaselessString & m_agentAddress; // For backward compatibility
      EventPackage      m_eventPackage;
      bool              m_eventList;    // Enable RFC4662
      PCaselessString   m_contentType;  // May be \n separated list of types

      PNotifierTemplate<const SubscriptionStatus &> m_onSubcribeStatus;
      PNotifierTemplate<NotifyCallbackInfo &> m_onNotify;
    };

    SIPSubscribe(
        SIPEndPoint & ep,
        OpalTransport & trans,
        SIPDialogContext & dialog,
        const Params & params
    );

    virtual SIPTransaction * CreateDuplicate() const;
};


#if PTRACING
ostream & operator<<(ostream & strm, const SIPSubscribe::Params & params);
#endif


typedef SIPSubscribe::EventPackage SIPEventPackage;


/////////////////////////////////////////////////////////////////////////

class SIPHandler;

class SIPEventPackageHandler
{
public:
  virtual ~SIPEventPackageHandler() { }
  virtual PCaselessString GetContentType() const = 0;
  virtual bool ValidateContentType(const PString & type, const SIPMIMEInfo & mime);
  virtual bool OnReceivedNOTIFY(SIPHandler & handler, SIP_PDU & request) = 0;
  virtual PString OnSendNOTIFY(SIPHandler & /*handler*/, const PObject * /*body*/) { return PString::Empty(); }
};


typedef PFactory<SIPEventPackageHandler, SIPEventPackage> SIPEventPackageFactory;


/////////////////////////////////////////////////////////////////////////

class SIPNotify : public SIPTransaction
{
    PCLASSINFO(SIPNotify, SIPTransaction);
  public:
    SIPNotify(
        SIPEndPoint & ep,
        OpalTransport & trans,
        SIPDialogContext & dialog,
        const SIPEventPackage & eventPackage,
        const PString & state,
        const PString & body
    );

    virtual SIPTransaction * CreateDuplicate() const;
};


/////////////////////////////////////////////////////////////////////////

class SIPPublish : public SIPTransaction
{
    PCLASSINFO(SIPPublish, SIPTransaction);
  public:
    SIPPublish(
      SIPEndPoint & ep,
      OpalTransport & trans,
      const PString & id,
      const PString & sipIfMatch,
      const SIPSubscribe::Params & params,
      const PString & body
    );

    virtual SIPTransaction * CreateDuplicate() const;
};


/////////////////////////////////////////////////////////////////////////

class SIPRefer : public SIPTransaction
{
  PCLASSINFO(SIPRefer, SIPTransaction);
  public:
    SIPRefer(
      SIPConnection & connection,
      const SIPURL & referTo,
      const SIPURL & referred_by,
      bool referSub
    );

    virtual SIPTransaction * CreateDuplicate() const;
};


/////////////////////////////////////////////////////////////////////////

/* This is not a generic NOTIFY PDU, but the minimal one
 * that gets sent when receiving a REFER
 */
class SIPReferNotify : public SIPTransaction
{
    PCLASSINFO(SIPReferNotify, SIPTransaction);
  public:
    SIPReferNotify(
      SIPConnection & connection,
      StatusCodes code
    );

    virtual SIPTransaction * CreateDuplicate() const;
};


/////////////////////////////////////////////////////////////////////////

/* This is a MESSAGE PDU, with a body
 */
class SIPMessage : public SIPTransaction
{
  PCLASSINFO(SIPMessage, SIPTransaction);
  public:
    struct Params : public SIPParameters
    {
      Params()
        : m_contentType("text/plain;charset=UTF-8")
      { 
        m_expire = 5000;
      }

      PCaselessString             m_contentType;
      PString                     m_id;
      PString                     m_body;
      PAtomicInteger::IntegerType m_messageId;
    };

    SIPMessage(
      SIPEndPoint & ep,
      OpalTransport & trans,
      const Params & params
    );
    SIPMessage(
      SIPConnection & connection,
      const Params & params
    );

    virtual SIPTransaction * CreateDuplicate() const;

    const SIPURL & GetLocalAddress() const { return m_localAddress; }

  private:
    void Construct(const Params & params);

    SIPURL m_localAddress;
};


/////////////////////////////////////////////////////////////////////////

/* This is an OPTIONS request
 */
class SIPOptions : public SIPTransaction
{
    PCLASSINFO(SIPOptions, SIPTransaction);
    
  public:
    struct Params : public SIPParameters
    {
      Params()
        : m_acceptContent("application/sdp, application/media_control+xml, application/dtmf, application/dtmf-relay")
      { 
      }

      PCaselessString m_acceptContent;
      PCaselessString m_contentType;
      PString         m_body;
    };

    SIPOptions(
      SIPEndPoint & ep,
      OpalTransport & trans,
      const PString & id,
      const Params & params
    );
    SIPOptions(
      SIPConnection & conn,
      const Params & params
    );

    virtual SIPTransaction * CreateDuplicate() const;

  protected:
    void Construct(const Params & params);
};


/////////////////////////////////////////////////////////////////////////

/* This is an INFO request
 */
class SIPInfo : public SIPTransaction
{
    PCLASSINFO(SIPInfo, SIPTransaction);
    
  public:
    struct Params
    {
      Params(const PString & contentType = PString::Empty(),
             const PString & body = PString::Empty())
        : m_contentType(contentType)
        , m_body(body)
      {
      }

      PCaselessString m_contentType;
      PString         m_body;
    };

    SIPInfo(
      SIPConnection & conn,
      const Params & params
    );

    virtual SIPTransaction * CreateDuplicate() const;
};


/////////////////////////////////////////////////////////////////////////

/* This is a PING PDU, with a body
 */
class SIPPing : public SIPTransaction
{
  PCLASSINFO(SIPPing, SIPTransaction);

  public:
    SIPPing(
      SIPEndPoint & ep,
      OpalTransport & trans,
      const SIPURL & address
    );

    virtual SIPTransaction * CreateDuplicate() const;
};


/////////////////////////////////////////////////////////////////////////

/* This is a PRACK PDU
 */
class SIPPrack : public SIPTransaction
{
  PCLASSINFO(SIPPrack, SIPTransaction);

  public:
    SIPPrack(
      SIPConnection & conn,
      const PString & rack
    );

    virtual SIPTransaction * CreateDuplicate() const;
};


#endif // OPAL_SIP

#endif // OPAL_SIP_SIPPDU_H


// End of File ///////////////////////////////////////////////////////////////
