/*
 * peclient.cxx
 *
 * H.323 Annex G Peer Element client protocol handler
 *
 * Open H323 Library
 *
 * Copyright (c) 2003 Equivalence Pty. Ltd.
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
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 21293 $
 * $Author: rjongbloed $
 * $Date: 2008-10-12 18:24:41 -0500 (Sun, 12 Oct 2008) $
 */

#include <ptlib.h>

#include <opal/buildopts.h>

#if OPAL_H501

#ifdef __GNUC__
#pragma implementation "peclient.h"
#endif

#include <h323/peclient.h>

#include <h323/h323ep.h>
#include <h323/h323annexg.h>
#include <h323/h323pdu.h>


#define new PNEW


const unsigned ServiceRequestRetryTime       = 60;
const unsigned ServiceRequestGracePeriod     = 10;
const unsigned ServiceRelationshipTimeToLive = 60;


////////////////////////////////////////////////////////////////

H501Transaction::H501Transaction(H323PeerElement & pe, const H501PDU & pdu, PBoolean hasReject)
: H323Transaction(pe, pdu, new H501PDU, hasReject ? new H501PDU : NULL),
    requestCommon(((H501PDU &)request->GetPDU()).m_common),
    confirmCommon(((H501PDU &)confirm->GetPDU()).m_common),
    peerElement(pe)
{
}


H323TransactionPDU * H501Transaction::CreateRIP(unsigned sequenceNumber,
                                                unsigned delay) const
{
  H501PDU * rip = new H501PDU;
  rip->BuildRequestInProgress(sequenceNumber, delay);
  return rip;
}


H235Authenticator::ValidationResult H501Transaction::ValidatePDU() const
{
  return request->Validate(requestCommon.m_tokens, H501_MessageCommonInfo::e_tokens,
                           requestCommon.m_cryptoTokens, H501_MessageCommonInfo::e_cryptoTokens);
}


////////////////////////////////////////////////////////////////

H501ServiceRequest::H501ServiceRequest(H323PeerElement & pe,
                                       const H501PDU & pdu)
  : H501Transaction(pe, pdu, PTrue),
    srq((H501_ServiceRequest &)request->GetChoice().GetObject()),
    scf(((H501PDU &)confirm->GetPDU()).BuildServiceConfirmation(pdu.m_common.m_sequenceNumber)),
    srj(((H501PDU &)reject->GetPDU()).BuildServiceRejection(pdu.m_common.m_sequenceNumber,
                                                            H501_ServiceRejectionReason::e_undefined))
{
}


#if PTRACING
const char * H501ServiceRequest::GetName() const
{
  return "ServiceRequest";
}
#endif


void H501ServiceRequest::SetRejectReason(unsigned reasonCode)
{
  srj.m_reason.SetTag(reasonCode);
}


H323Transaction::Response H501ServiceRequest::OnHandlePDU()
{
  return peerElement.OnServiceRequest(*this);
}


////////////////////////////////////////////////////////////////

H501DescriptorUpdate::H501DescriptorUpdate(H323PeerElement & pe,
                                           const H501PDU & pdu)
  : H501Transaction(pe, pdu, PFalse),
    du((H501_DescriptorUpdate &)request->GetChoice().GetObject()),
    ack(((H501PDU &)confirm->GetPDU()).BuildDescriptorUpdateAck(pdu.m_common.m_sequenceNumber))
{
}


#if PTRACING
const char * H501DescriptorUpdate::GetName() const
{
  return "DescriptorUpdate";
}
#endif


void H501DescriptorUpdate::SetRejectReason(unsigned /*reasonCode*/)
{
  // Not possible!
}


H323Transaction::Response H501DescriptorUpdate::OnHandlePDU()
{
  return peerElement.OnDescriptorUpdate(*this);
}


////////////////////////////////////////////////////////////////

H501AccessRequest::H501AccessRequest(H323PeerElement & pe,
                                     const H501PDU & pdu)
  : H501Transaction(pe, pdu, PTrue),
    arq((H501_AccessRequest &)request->GetChoice().GetObject()),
    acf(((H501PDU &)confirm->GetPDU()).BuildAccessConfirmation(pdu.m_common.m_sequenceNumber)),
    arj(((H501PDU &)reject->GetPDU()).BuildAccessRejection(pdu.m_common.m_sequenceNumber,
                                                           H501_AccessRejectionReason::e_undefined))
{
}


#if PTRACING
const char * H501AccessRequest::GetName() const
{
  return "AccessRequest";
}
#endif


void H501AccessRequest::SetRejectReason(unsigned reasonCode)
{
  arj.m_reason.SetTag(reasonCode);
}


H323Transaction::Response H501AccessRequest::OnHandlePDU()
{
  return peerElement.OnAccessRequest(*this);
}


////////////////////////////////////////////////////////////////

H323PeerElement::H323PeerElement(H323EndPoint & ep, H323Transport * trans)
  : H323_AnnexG(ep, trans),
    requestMutex(1, 1)
{
  Construct();
}

H323PeerElement::H323PeerElement(H323EndPoint & ep, const H323TransportAddress & addr)
  : H323_AnnexG(ep, addr),
    requestMutex(1, 1)
{
  Construct();
}

void H323PeerElement::Construct()
{
  if (transport != NULL)
    transport->SetPromiscuous(H323Transport::AcceptFromAny);

  monitorStop       = PFalse;
  localIdentifier   = endpoint.GetLocalUserName();
  basePeerOrdinal   = RemoteServiceRelationshipOrdinal;

  StartChannel();

  monitor = PThread::Create(PCREATE_NOTIFIER(MonitorMain), "PeerElementMonitor");
}

H323PeerElement::~H323PeerElement()
{
  if (monitor != NULL) {
    monitorStop = PTrue;
    monitorTickle.Signal();
    monitor->WaitForTermination();
    delete monitor;
  }

  StopChannel();
}


void H323PeerElement::SetLocalName(const PString & name)
{
  PWaitAndSignal m(localNameMutex);
  localIdentifier = name;
}

PString H323PeerElement::GetLocalName() const
{
  PWaitAndSignal m(localNameMutex);
  return localIdentifier;
}

void H323PeerElement::SetDomainName(const PString & name)
{
  PWaitAndSignal m(localNameMutex);
  domainName = name;
}

PString H323PeerElement::GetDomainName() const
{
  PWaitAndSignal m(localNameMutex);
  return domainName;
}


void H323PeerElement::PrintOn(ostream & strm) const
{
  if (!localIdentifier)
    strm << localIdentifier << '@';
  H323Transactor::PrintOn(strm);
}

void H323PeerElement::MonitorMain(PThread &, INT)
{
  PTRACE(4, "PeerElement\tBackground thread started");

  for (;;) {

    // refresh and retry remote service relationships by sending new ServiceRequests
    PTime now;
    PTime nextExpireTime = now + ServiceRequestRetryTime*1000;
    {
      for (PSafePtr<H323PeerElementServiceRelationship> sr = GetFirstRemoteServiceRelationship(PSafeReadOnly); sr != NULL; sr++) {

        if (now >= sr->expireTime) {
          PTRACE(3, "PeerElement\tRenewing service relationship " << sr->serviceID << "before expiry");
          ServiceRequestByID(sr->serviceID);
        }

        // get minimum sleep time for next refresh or retry
        if (sr->expireTime < nextExpireTime)
          nextExpireTime = sr->expireTime;
      }
    }

    // expire local service relationships we have not received ServiceRequests for
    {
      for (PSafePtr<H323PeerElementServiceRelationship> sr = GetFirstLocalServiceRelationship(PSafeReadOnly); sr != NULL; sr++) {

        // check to see if expired or needs refresh scheduled
        PTime expireTime = sr->expireTime + 1000 * ServiceRequestGracePeriod;
        if (now >= expireTime) {
          PTRACE(2, "PeerElement\tService relationship " << sr->serviceID << "expired");
          localServiceRelationships.Remove(sr);
          {
            PWaitAndSignal m(localPeerListMutex);
            localServiceOrdinals -= sr->ordinal;
          }
        }
        else if (expireTime < nextExpireTime)
          nextExpireTime = sr->expireTime;
      }
    }

    // if any descriptor needs updating, then spawn a thread to do it
    {
      for (PSafePtr<H323PeerElementDescriptor> descriptor = GetFirstDescriptor(PSafeReadOnly); descriptor != NULL; descriptor++) {
        PWaitAndSignal m(localPeerListMutex);
        if (
            (descriptor->state != H323PeerElementDescriptor::Clean) || 
            (
             (descriptor->creator >= RemoteServiceRelationshipOrdinal) && 
              !localServiceOrdinals.Contains(descriptor->creator)
             )
            ) {
          PThread::Create(PCREATE_NOTIFIER(UpdateAllDescriptors), 0, PThread::AutoDeleteThread, PThread::NormalPriority, "UpdateDescriptors");
          break;
        }
      }
    }

    // wait until just before the next expire time;
    PTimeInterval timeToWait = nextExpireTime - PTime();
    if (timeToWait > 60*1000)
      timeToWait = 60*1000;
    monitorTickle.Wait(timeToWait);

    if (monitorStop)
      break;
  }

  PTRACE(4, "PeerElement\tBackground thread ended");
}

void H323PeerElement::UpdateAllDescriptors(PThread &, INT)
{
  PTRACE(4, "PeerElement\tDescriptor update thread started");

  for (PSafePtr<H323PeerElementDescriptor> descriptor = GetFirstDescriptor(PSafeReadWrite); descriptor != NULL; descriptor++) {
    PWaitAndSignal m(localPeerListMutex);

    // delete any descriptors which belong to service relationships that are now gone
    if (
        (descriptor->state != H323PeerElementDescriptor::Deleted) &&
        (descriptor->creator >= RemoteServiceRelationshipOrdinal) && 
        !localServiceOrdinals.Contains(descriptor->creator)
       )
      descriptor->state = H323PeerElementDescriptor::Deleted;

    UpdateDescriptor(descriptor);
  }

  monitorTickle.Signal();

  PTRACE(4, "PeerElement\tDescriptor update thread ended");
}

void H323PeerElement::TickleMonitor(PTimer &, INT)
{
  monitorTickle.Signal();
}

///////////////////////////////////////////////////////////
//
// service relationship functions
//

H323PeerElementServiceRelationship * H323PeerElement::CreateServiceRelationship()
{
  return new H323PeerElementServiceRelationship();
}

PBoolean H323PeerElement::SetOnlyServiceRelationship(const PString & peer, PBoolean keepTrying)
{
  if (peer.IsEmpty()) {
    RemoveAllServiceRelationships();
    return PTrue;
  }

  for (PSafePtr<H323PeerElementServiceRelationship> sr = GetFirstRemoteServiceRelationship(PSafeReadOnly); sr != NULL; sr++)
    if (sr->peer != peer)
      RemoveServiceRelationship(sr->peer);

  return AddServiceRelationship(peer, keepTrying);
}

PBoolean H323PeerElement::AddServiceRelationship(const H323TransportAddress & addr, PBoolean keepTrying)
{
  OpalGloballyUniqueID serviceID;
  return AddServiceRelationship(addr, serviceID, keepTrying);
}

PBoolean H323PeerElement::AddServiceRelationship(const H323TransportAddress & addr, OpalGloballyUniqueID & serviceID, PBoolean keepTrying)

{
  switch (ServiceRequestByAddr(addr, serviceID)) {
    case Confirmed:
    case ServiceRelationshipReestablished:
      return PTrue;

    case NoResponse:
      if (!keepTrying)
        return PFalse;
      break;    
    
    case Rejected:
    case NoServiceRelationship:
    default:
      return PFalse;
  }

  PTRACE(2, "PeerElement\tRetrying ServiceRequest to " << addr << " in " << ServiceRequestRetryTime);

  // this will cause the polling routines to keep trying to establish a new service relationship
  H323PeerElementServiceRelationship * sr = CreateServiceRelationship();
  sr->peer = addr;
  sr->expireTime = PTime() + (ServiceRequestRetryTime * 1000);
  {
    PWaitAndSignal m(basePeerOrdinalMutex);
    sr->ordinal = basePeerOrdinal++;
  }
  {
    PWaitAndSignal m(remotePeerListMutex);
    remotePeerAddrToServiceID.SetAt(addr, sr->serviceID.AsString());
    remotePeerAddrToOrdinalKey.SetAt(addr, new POrdinalKey(sr->ordinal));
  }
  remoteServiceRelationships.Append(sr);

  monitorTickle.Signal();

  return PTrue;
}

PBoolean H323PeerElement::RemoveServiceRelationship(const OpalGloballyUniqueID & serviceID, int reason)
{
  {
    PWaitAndSignal m(remotePeerListMutex);

    // if no service relationship exists for this peer, then nothing to do
    PSafePtr<H323PeerElementServiceRelationship> sr = remoteServiceRelationships.FindWithLock(H323PeerElementServiceRelationship(serviceID), PSafeReadOnly);
    if (sr == NULL) {
      return PFalse;
    }
  }

  return ServiceRelease(serviceID, reason);
}


PBoolean H323PeerElement::RemoveServiceRelationship(const H323TransportAddress & peer, int reason)
{
  OpalGloballyUniqueID serviceID;

  // if no service relationship exists for this peer, then nothing to do
  {
    PWaitAndSignal m(remotePeerListMutex);
    if (!remotePeerAddrToServiceID.Contains(peer))
      return PFalse;
    serviceID = remotePeerAddrToServiceID[peer];
  }

  return ServiceRelease(serviceID, reason);
}

PBoolean H323PeerElement::RemoveAllServiceRelationships()
{
  // if a service relationship exists for this peer, then reconfirm it
  for (PSafePtr<H323PeerElementServiceRelationship> sr = GetFirstRemoteServiceRelationship(PSafeReadOnly); sr != NULL; sr++)
    RemoveServiceRelationship(sr->peer);

  return PTrue;
}

H323PeerElement::Error H323PeerElement::ServiceRequestByAddr(const H323TransportAddress & peer, OpalGloballyUniqueID & serviceID)
{
  if (PAssertNULL(transport) == NULL)
    return NoResponse;

  // if a service relationship exists for this peer, then reconfirm it
  remotePeerListMutex.Wait();
  if (remotePeerAddrToServiceID.Contains(peer)) {
    serviceID = remotePeerAddrToServiceID[peer];
    remotePeerListMutex.Signal();
    return ServiceRequestByID(serviceID);
  }
  remotePeerListMutex.Signal();

  // create a new service relationship
  H323PeerElementServiceRelationship * sr = CreateServiceRelationship();

  // build the service request
  H501PDU pdu;
  H323TransportAddressArray interfaces = GetInterfaceAddresses();
  H501_ServiceRequest & body = pdu.BuildServiceRequest(GetNextSequenceNumber(), interfaces);

  // include the element indentifier
  body.IncludeOptionalField(H501_ServiceRequest::e_elementIdentifier);
  body.m_elementIdentifier = localIdentifier;

  // send the request
  Request request(pdu.GetSequenceNumber(), pdu, peer);
  H501PDU reply;
  request.responseInfo = &reply;
  if (!MakeRequest(request))  {
    delete sr;
    switch (request.responseResult) {
      case Request::NoResponseReceived :
        PTRACE(2, "PeerElement\tServiceRequest to " << peer << " failed due to no response");
        return NoResponse;

      case Request::RejectReceived:
        PTRACE(2, "PeerElement\tServiceRequest to " << peer << " rejected for reason " << request.rejectReason);
        break;

      default:
        PTRACE(2, "PeerElement\tServiceRequest to " << peer << " refused with unknown response " << (int)request.responseResult);
        break;
    }
    return Rejected;
  }

  // reply must contain a service ID
  if (!reply.m_common.HasOptionalField(H501_MessageCommonInfo::e_serviceID)) {
    PTRACE(1, "PeerElement\tServiceConfirmation contains no serviceID");
    delete sr;
    return Rejected;
  }

  // create the service relationship
  H501_ServiceConfirmation & replyBody = reply.m_body;
  sr->peer = peer;
  sr->serviceID = reply.m_common.m_serviceID;
  sr->expireTime = PTime() + 1000 * ((replyBody.m_timeToLive < ServiceRequestRetryTime) ? (int)replyBody.m_timeToLive : ServiceRequestRetryTime);
  sr->lastUpdateTime = PTime();
  serviceID = sr->serviceID;

  {
    if (sr->ordinal == LocalServiceRelationshipOrdinal) {
      {
        PWaitAndSignal m(basePeerOrdinalMutex);
        sr->ordinal = basePeerOrdinal++;
      }
      {
        PWaitAndSignal m(remotePeerListMutex);
        remotePeerAddrToServiceID.SetAt(peer, sr->serviceID.AsString());
        remotePeerAddrToOrdinalKey.SetAt(peer, new POrdinalKey(sr->ordinal));
      }
    }
  }

  remoteServiceRelationships.Append(sr);

  PTRACE(3, "PeerElement\tNew service relationship established with " << peer << " - next update in " << replyBody.m_timeToLive);
  OnAddServiceRelationship(peer);

  // mark all descriptors as needing an update
  for (PSafePtr<H323PeerElementDescriptor> descriptor = GetFirstDescriptor(PSafeReadWrite); descriptor != NULL; descriptor++) {
    if (descriptor->state == H323PeerElementDescriptor::Clean)
      descriptor->state = H323PeerElementDescriptor::Dirty;
  }

  monitorTickle.Signal();
  return Confirmed;
}


H323PeerElement::Error H323PeerElement::ServiceRequestByID(OpalGloballyUniqueID & serviceID)
{
  if (PAssertNULL(transport) == NULL)
    return NoResponse;

  // build the service request
  H501PDU pdu;
  H501_ServiceRequest & body = pdu.BuildServiceRequest(GetNextSequenceNumber(), transport->GetLastReceivedAddress());

  // include the element indentifier
  body.IncludeOptionalField(H501_ServiceRequest::e_elementIdentifier);
  body.m_elementIdentifier = localIdentifier;

  // check to see if we have a service relationship with the peer already
  PSafePtr<H323PeerElementServiceRelationship> sr = remoteServiceRelationships.FindWithLock(H323PeerElementServiceRelationship(serviceID), PSafeReadWrite);
  if (sr == NULL)
    return NoServiceRelationship;

  // setup to update the old service relationship
  pdu.m_common.IncludeOptionalField(H501_MessageCommonInfo::e_serviceID);
  pdu.m_common.m_serviceID = sr->serviceID;
  Request request(pdu.GetSequenceNumber(), pdu, sr->peer);
  H501PDU reply;
  request.responseInfo = &reply;

  if (MakeRequest(request)) {
    H501_ServiceConfirmation & replyBody = reply.m_body;
    sr->expireTime = PTime() + 1000 * ((replyBody.m_timeToLive < ServiceRequestRetryTime) ? (int)replyBody.m_timeToLive : ServiceRequestRetryTime);
    sr->lastUpdateTime = PTime();
    PTRACE(3, "PeerElement\tConfirmed service relationship with " << sr->peer << " - next update in " << replyBody.m_timeToLive);
    return Confirmed;
  }

  // if cannot update, then try again after 60 seconds
  switch (request.responseResult) {
    case Request::NoResponseReceived :
      PTRACE(2, "PeerElement\tNo response to ServiceRequest - trying again in " << ServiceRequestRetryTime);
      sr->expireTime = PTime() + (ServiceRequestRetryTime * 1000);
      monitorTickle.Signal();
      return NoResponse;

    case Request::RejectReceived:
      switch (request.rejectReason) {
        case H501_ServiceRejectionReason::e_unknownServiceID:
          if (OnRemoteServiceRelationshipDisappeared(serviceID, sr->peer))
            return Confirmed;
          break;

        default:
          PTRACE(2, "PeerElement\tServiceRequest to " << sr->peer << " rejected with unknown reason " << request.rejectReason);
          break;
      }
      break;

    default:
      PTRACE(2, "PeerElement\tServiceRequest to " << sr->peer << " failed with unknown response " << (int)request.responseResult);
      break;
  }

  return Rejected;
}

H323Transaction::Response H323PeerElement::OnServiceRequest(H501ServiceRequest & info)
{
  info.SetRejectReason(H501_ServiceRejectionReason::e_serviceUnavailable);
  return H323Transaction::Reject;
}

H323Transaction::Response H323PeerElement::HandleServiceRequest(H501ServiceRequest & info)
{
  // if a serviceID is specified, this is should be an existing service relationship
  if (info.requestCommon.HasOptionalField(H501_MessageCommonInfo::e_serviceID)) {

    // check to see if we have a service relationship with the peer already
    OpalGloballyUniqueID serviceID(info.requestCommon.m_serviceID);
    PSafePtr<H323PeerElementServiceRelationship> sr = localServiceRelationships.FindWithLock(H323PeerElementServiceRelationship(serviceID), PSafeReadWrite);
    if (sr == NULL) {
      PTRACE(2, "PeerElement\tRejecting unknown service ID " << serviceID << " received from peer " << info.GetReplyAddress());
      info.SetRejectReason(H501_ServiceRejectionReason::e_unknownServiceID);
      return H323Transaction::Reject;
    }

    // include service ID, local and domain identifiers
    info.confirmCommon.IncludeOptionalField(H501_MessageCommonInfo::e_serviceID);
    info.confirmCommon.m_serviceID = sr->serviceID;
    info.scf.m_elementIdentifier = GetLocalName();
    H323SetAliasAddress(GetDomainName(), info.scf.m_domainIdentifier);

    // include time to live
    info.scf.IncludeOptionalField(H501_ServiceConfirmation::e_timeToLive);
    info.scf.m_timeToLive = ServiceRelationshipTimeToLive;
    sr->lastUpdateTime = PTime();
    sr->expireTime = PTime() + (info.scf.m_timeToLive * 1000);

    PTRACE(2, "PeerElement\tService relationship with " << sr->name << " at " << info.GetReplyAddress() << " updated - next update in " << info.scf.m_timeToLive);
    return H323Transaction::Confirm;
  }

  H323PeerElementServiceRelationship * sr = CreateServiceRelationship();

  // get the name of the remote element
  if (info.srq.HasOptionalField(H501_ServiceRequest::e_elementIdentifier))
    sr->name = info.srq.m_elementIdentifier;

  // include service ID, local and domain identifiers
  info.confirmCommon.IncludeOptionalField(H501_MessageCommonInfo::e_serviceID);
  info.confirmCommon.m_serviceID = sr->serviceID;
  info.scf.m_elementIdentifier = GetLocalName();
  H323SetAliasAddress(GetDomainName(), info.scf.m_domainIdentifier);

  // include time to live
  info.scf.IncludeOptionalField(H501_ServiceConfirmation::e_timeToLive);
  info.scf.m_timeToLive = ServiceRelationshipTimeToLive;
  if (info.requestCommon.HasOptionalField(H501_MessageCommonInfo::e_replyAddress) && info.requestCommon.m_replyAddress.GetSize() > 0)
    sr->peer = info.requestCommon.m_replyAddress[0];
  else
    sr->peer = transport->GetLastReceivedAddress();
  sr->lastUpdateTime = PTime();
  sr->expireTime = PTime() + (info.scf.m_timeToLive * 1000);
  {
    H323TransportAddress addr = transport->GetLastReceivedAddress();
    {
      PWaitAndSignal m(basePeerOrdinalMutex);
      sr->ordinal = basePeerOrdinal++;
    }
    {
      PWaitAndSignal m(localPeerListMutex);
      localServiceOrdinals += sr->ordinal;
    }
  }

  // add to the list of known relationships
  localServiceRelationships.Append(sr);
  monitorTickle.Signal();

  // send the response
  PTRACE(3, "PeerElement\tNew service relationship with " << sr->name << " at " << info.GetReplyAddress() << " created - next update in " << info.scf.m_timeToLive);
  return H323Transaction::Confirm;
}

PBoolean H323PeerElement::OnReceiveServiceRequest(const H501PDU & pdu, const H501_ServiceRequest & /*pduBody*/)
{
  H501ServiceRequest * info = new H501ServiceRequest(*this, pdu);
  if (!info->HandlePDU())
    delete info;

  return PFalse;
}

PBoolean H323PeerElement::OnReceiveServiceConfirmation(const H501PDU & pdu, const H501_ServiceConfirmation & pduBody)
{
  if (!H323_AnnexG::OnReceiveServiceConfirmation(pdu, pduBody))
    return PFalse;

  if (lastRequest->responseInfo != NULL)
    *(H501PDU *)lastRequest->responseInfo = pdu;

  return PTrue;
}

PBoolean H323PeerElement::ServiceRelease(const OpalGloballyUniqueID & serviceID, unsigned reason)
{
  // remove any previous check to see if we have a service relationship with the peer already
  PSafePtr<H323PeerElementServiceRelationship> sr = remoteServiceRelationships.FindWithLock(H323PeerElementServiceRelationship(serviceID), PSafeReadWrite);
  if (sr == NULL)
    return PFalse;

  // send the request - no response
  H501PDU pdu;
  H501_ServiceRelease & body = pdu.BuildServiceRelease(GetNextSequenceNumber());
  pdu.m_common.m_serviceID = sr->serviceID;
  body.m_reason = reason;
  WriteTo(pdu, sr->peer);

  OnRemoveServiceRelationship(sr->peer);
  InternalRemoveServiceRelationship(sr->peer);
  remoteServiceRelationships.Remove(sr);

  return PTrue;
}

PBoolean H323PeerElement::OnRemoteServiceRelationshipDisappeared(OpalGloballyUniqueID & serviceID, const H323TransportAddress & peer)
{
  OpalGloballyUniqueID oldServiceID = serviceID;

  // the service ID specified is now gone
  PSafePtr<H323PeerElementServiceRelationship> sr = remoteServiceRelationships.FindWithLock(H323PeerElementServiceRelationship(serviceID), PSafeReadOnly);
  if (sr != NULL)
    remoteServiceRelationships.Remove(sr);
  InternalRemoveServiceRelationship(peer);

  // attempt to create a new service relationship
  if (ServiceRequestByAddr(peer, serviceID) != Confirmed) { 
    PTRACE(2, "PeerElement\tService relationship with " << peer << " disappeared and refused new relationship");
    OnRemoveServiceRelationship(peer);
    return PFalse;
  }

  // we have a new service ID
  PTRACE(2, "PeerElement\tService relationship with " << peer << " disappeared and new relationship established");
  serviceID = remotePeerAddrToServiceID(peer);

  return PTrue;
}

void H323PeerElement::InternalRemoveServiceRelationship(const H323TransportAddress & peer)
{
  {
    PWaitAndSignal m(remotePeerListMutex);
    remotePeerAddrToServiceID.RemoveAt(peer);
    remotePeerAddrToOrdinalKey.RemoveAt(peer);
  }
  monitorTickle.Signal();
}



///////////////////////////////////////////////////////////
//
// descriptor table functions
//

H323PeerElementDescriptor * H323PeerElement::CreateDescriptor(const OpalGloballyUniqueID & descriptorID)
{
  return new H323PeerElementDescriptor(descriptorID);
}

PBoolean H323PeerElement::AddDescriptor(const OpalGloballyUniqueID & descriptorID,
                                            const PStringArray & aliasStrings, 
                               const H323TransportAddressArray & transportAddresses, 
                                                        unsigned options, 
                                                            PBoolean now)
{
  // convert transport addresses to aliases
  H225_ArrayOf_AliasAddress aliases;
  H323SetAliasAddresses(aliasStrings, aliases);
  return AddDescriptor(descriptorID,
                       aliases, transportAddresses, options, now);
}


PBoolean H323PeerElement::AddDescriptor(const OpalGloballyUniqueID & descriptorID,
                               const H225_ArrayOf_AliasAddress & aliases, 
                               const H323TransportAddressArray & transportAddresses, 
                                                        unsigned options, 
                                                            PBoolean now)
{
  H225_ArrayOf_AliasAddress addresses;
  H323SetAliasAddresses(transportAddresses, addresses);
  return AddDescriptor(descriptorID,
                       LocalServiceRelationshipOrdinal,
                       aliases, addresses, options, now);
}


PBoolean H323PeerElement::AddDescriptor(const OpalGloballyUniqueID & descriptorID,
                               const H225_ArrayOf_AliasAddress & aliases, 
                               const H225_ArrayOf_AliasAddress & transportAddress, 
                                                        unsigned options, 
                                                            PBoolean now)
{
  // create a new descriptor
  return AddDescriptor(descriptorID,
                       LocalServiceRelationshipOrdinal,
                       aliases, transportAddress, options, now);
}

PBoolean H323PeerElement::AddDescriptor(const OpalGloballyUniqueID & descriptorID,
                                             const POrdinalKey & creator,
                               const H225_ArrayOf_AliasAddress & aliases, 
                               const H225_ArrayOf_AliasAddress & transportAddresses, 
                                                        unsigned options, 
                                                            PBoolean now)
{
  // create an const H501_ArrayOf_AddressTemplate with the template information
  H501_ArrayOf_AddressTemplate addressTemplates;

  // copy data into the descriptor
  addressTemplates.SetSize(1);
  H225_EndpointType epType;
  endpoint.SetEndpointTypeInfo(epType);
  H323PeerElementDescriptor::CopyToAddressTemplate(addressTemplates[0], epType, aliases, transportAddresses, options);

  return AddDescriptor(descriptorID, creator, addressTemplates, now);
}

PBoolean H323PeerElement::AddDescriptor(const OpalGloballyUniqueID & descriptorID,
                                             const POrdinalKey & creator,
                            const H501_ArrayOf_AddressTemplate & addressTemplates,
                                                   const PTime & updateTime,
                                                            PBoolean now)
{
  // see if there is actually a descriptor with this ID
  PSafePtr<H323PeerElementDescriptor> descriptor = descriptors.FindWithLock(H323PeerElementDescriptor(descriptorID), PSafeReadWrite);
  H501_UpdateInformation_updateType::Choices updateType = H501_UpdateInformation_updateType::e_changed;
  PBoolean add = PFalse;
  {
    PWaitAndSignal m(aliasMutex);
    if (descriptor != NULL) {
      RemoveDescriptorInformation(descriptor->addressTemplates);

      // only update if the update time is later than what we already have
      if (updateTime < descriptor->lastChanged)
        return PTrue;

    } else {
      add = PTrue;
      descriptor                   = CreateDescriptor(descriptorID);
      descriptor->creator          = creator;
      descriptor->addressTemplates = addressTemplates;
      updateType                   = H501_UpdateInformation_updateType::e_added;
    }
    descriptor->lastChanged = PTime();

    // add all patterns and transport addresses to secondary lookup tables
    PINDEX i, j, k;
    for (i = 0; i < descriptor->addressTemplates.GetSize(); i++) {
      H501_AddressTemplate & addressTemplate = addressTemplates[i];

      // add patterns for this descriptor
      for (j = 0; j < addressTemplate.m_pattern.GetSize(); j++) {
        H501_Pattern & pattern = addressTemplate.m_pattern[j];
        switch (pattern.GetTag()) {
          case H501_Pattern::e_specific:
            specificAliasToDescriptorID.Append(CreateAliasKey((H225_AliasAddress &)pattern, descriptorID, i, PFalse));
            break;
          case H501_Pattern::e_wildcard:
            wildcardAliasToDescriptorID.Append(CreateAliasKey((H225_AliasAddress &)pattern, descriptorID, i, PTrue));
            break;
          case H501_Pattern::e_range:
            break;
        }
      }

      // add transport addresses for this descriptor
      H501_ArrayOf_RouteInformation & routeInfos = addressTemplate.m_routeInfo;
      for (j = 0; j < routeInfos.GetSize(); j++) {
        H501_ArrayOf_ContactInformation & contacts = routeInfos[j].m_contacts;
        for (k = 0; k < contacts.GetSize(); k++) {
          H501_ContactInformation & contact = contacts[k];
          H225_AliasAddress & transportAddress = contact.m_transportAddress;
          transportAddressToDescriptorID.Append(CreateAliasKey((H225_AliasAddress &)transportAddress, descriptorID, i));
        }
      }
    }
  }

  if (!add)
    OnUpdateDescriptor(*descriptor);
  else {
    descriptors.Append(descriptor);
    OnNewDescriptor(*descriptor);
  }

  // do the update now, or later
  if (now) {
    PTRACE(3, "PeerElement\tDescriptor " << descriptorID << " added/updated");
    UpdateDescriptor(descriptor, updateType);
  } else if (descriptor->state != H323PeerElementDescriptor::Deleted) {
    PTRACE(3, "PeerElement\tDescriptor " << descriptorID << " queued to be added");
    descriptor->state = H323PeerElementDescriptor::Dirty;
    monitorTickle.Signal();
  }

  return PTrue;
}
  
H323PeerElement::AliasKey * H323PeerElement::CreateAliasKey(const H225_AliasAddress & alias, const OpalGloballyUniqueID & id, PINDEX pos, PBoolean wild)
{
  return new AliasKey(alias, id, pos, wild);
}

void H323PeerElement::RemoveDescriptorInformation(const H501_ArrayOf_AddressTemplate & addressTemplates)
{
  PWaitAndSignal m(aliasMutex);
  PINDEX i, j, k, idx;

  // remove all patterns and transport addresses for this descriptor
  for (i = 0; i < addressTemplates.GetSize(); i++) {
    H501_AddressTemplate & addressTemplate = addressTemplates[i];

    // remove patterns for this descriptor
    for (j = 0; j < addressTemplate.m_pattern.GetSize(); j++) {
      H501_Pattern & pattern = addressTemplate.m_pattern[j];
      switch (pattern.GetTag()) {
        case H501_Pattern::e_specific:
          idx = specificAliasToDescriptorID.GetValuesIndex((H225_AliasAddress &)pattern);
          if (idx != P_MAX_INDEX)
            specificAliasToDescriptorID.RemoveAt(idx);
          break;
        case H501_Pattern::e_wildcard:
          idx = wildcardAliasToDescriptorID.GetValuesIndex((H225_AliasAddress &)pattern);
          if (idx != P_MAX_INDEX)
            wildcardAliasToDescriptorID.RemoveAt(idx);
          break;
        case H501_Pattern::e_range:
          break;
      }
    }

    // remove transport addresses for this descriptor
    H501_ArrayOf_RouteInformation & routeInfos = addressTemplate.m_routeInfo;
    for (j = 0; j < routeInfos.GetSize(); j++) {
      H501_ArrayOf_ContactInformation & contacts = routeInfos[i].m_contacts;
      for (k = 0; k < contacts.GetSize(); k++) {
        H501_ContactInformation & contact = contacts[k];
        H225_AliasAddress & transportAddress = contact.m_transportAddress;
        idx = transportAddressToDescriptorID.GetValuesIndex(transportAddress);
        if (idx != P_MAX_INDEX)
          transportAddressToDescriptorID.RemoveAt(idx);
      }
    }
  }
}

PBoolean H323PeerElement::DeleteDescriptor(const PString & str, PBoolean now)
{
  H225_AliasAddress alias;
  H323SetAliasAddress(str, alias);
  return DeleteDescriptor(alias, now);
}

PBoolean H323PeerElement::DeleteDescriptor(const H225_AliasAddress & alias, PBoolean now)
{
  OpalGloballyUniqueID descriptorID("");

  // find the descriptor ID for the descriptor
  {
    PWaitAndSignal m(aliasMutex);
    PINDEX idx = specificAliasToDescriptorID.GetValuesIndex(alias);
    if (idx == P_MAX_INDEX)
      return PFalse;
    descriptorID = ((AliasKey &)specificAliasToDescriptorID[idx]).id;
  }

  return DeleteDescriptor(descriptorID, now);
}

PBoolean H323PeerElement::DeleteDescriptor(const OpalGloballyUniqueID & descriptorID, PBoolean now)
{
  // see if there is a descriptor with this ID
  PSafePtr<H323PeerElementDescriptor> descriptor = descriptors.FindWithLock(H323PeerElementDescriptor(descriptorID), PSafeReadWrite);
  if (descriptor == NULL)
    return PFalse;

  OnRemoveDescriptor(*descriptor);

  RemoveDescriptorInformation(descriptor->addressTemplates);

  // delete the descriptor, or mark it as to be deleted
  if (now) {
    PTRACE(3, "PeerElement\tDescriptor " << descriptorID << " deleted");
    UpdateDescriptor(descriptor, H501_UpdateInformation_updateType::e_deleted);
  } else {
    PTRACE(3, "PeerElement\tDescriptor for " << descriptorID << " queued to be deleted");
    descriptor->state = H323PeerElementDescriptor::Deleted;
    monitorTickle.Signal();
  }

  return PTrue;
}

PBoolean H323PeerElement::UpdateDescriptor(H323PeerElementDescriptor * descriptor)
{
  H501_UpdateInformation_updateType::Choices updateType = H501_UpdateInformation_updateType::e_changed;
  switch (descriptor->state) {
    case H323PeerElementDescriptor::Clean:
      return PTrue;
    
    case H323PeerElementDescriptor::Dirty:
      break;

    case H323PeerElementDescriptor::Deleted:
      updateType = H501_UpdateInformation_updateType::e_deleted;
      break;
  }

  return UpdateDescriptor(descriptor, updateType);
}

PBoolean H323PeerElement::UpdateDescriptor(H323PeerElementDescriptor * descriptor, H501_UpdateInformation_updateType::Choices updateType)
{
  if (updateType == H501_UpdateInformation_updateType::e_deleted)
    descriptor->state = H323PeerElementDescriptor::Deleted;
  else if (descriptor->state == H323PeerElementDescriptor::Deleted)
    updateType = H501_UpdateInformation_updateType::e_deleted;
  else if (descriptor->state == H323PeerElementDescriptor::Clean)
    return PTrue;
  else
    descriptor->state = H323PeerElementDescriptor::Clean;

  for (PSafePtr<H323PeerElementServiceRelationship> sr = GetFirstRemoteServiceRelationship(PSafeReadOnly); sr != NULL; sr++) {
    SendUpdateDescriptorByID(sr->serviceID, descriptor, updateType);
  }

  if (descriptor->state == H323PeerElementDescriptor::Deleted)
    descriptors.Remove(descriptor);

  return PTrue;
}

///////////////////////////////////////////////////////////
//
// descriptor peer element functions
//

H323PeerElement::Error H323PeerElement::SendUpdateDescriptorByID(const OpalGloballyUniqueID & serviceID, 
                                                              H323PeerElementDescriptor * descriptor, 
                                               H501_UpdateInformation_updateType::Choices updateType)
{
  if (PAssertNULL(transport) == NULL)
    return NoResponse;

  H501PDU pdu;
  pdu.BuildDescriptorUpdate(GetNextSequenceNumber(), transport->GetLastReceivedAddress());
  H323TransportAddress peer;

  // put correct service descriptor into the common data
  {
    // check to see if we have a service relationship with the peer already
    PSafePtr<H323PeerElementServiceRelationship> sr = remoteServiceRelationships.FindWithLock(H323PeerElementServiceRelationship(serviceID), PSafeReadOnly);

    // if there is no service relationship, then nothing to do
    if (sr == NULL)
      return NoServiceRelationship;

    pdu.m_common.IncludeOptionalField(H501_MessageCommonInfo::e_serviceID);
    pdu.m_common.m_serviceID = sr->serviceID;
    peer = sr->peer;
  }

  return SendUpdateDescriptor(pdu, peer, descriptor, updateType);
}

H323PeerElement::Error H323PeerElement::SendUpdateDescriptorByAddr(const H323TransportAddress & peer, 
                                                              H323PeerElementDescriptor * descriptor, 
                                               H501_UpdateInformation_updateType::Choices updateType)
{
  if (PAssertNULL(transport) == NULL)
    return NoResponse;

  H501PDU pdu;
  pdu.BuildDescriptorUpdate(GetNextSequenceNumber(), transport->GetLastReceivedAddress());
  return SendUpdateDescriptor(pdu, peer, descriptor, updateType);
}

H323PeerElement::Error H323PeerElement::SendUpdateDescriptor(H501PDU & pdu, 
                                          const H323TransportAddress & peer, 
                                           H323PeerElementDescriptor * descriptor,
                            H501_UpdateInformation_updateType::Choices updateType)
{
  if (PAssertNULL(transport) == NULL)
    return NoResponse;

  H501_DescriptorUpdate & body = pdu.m_body;

  // put in sender address
  H323TransportAddressArray addrs = GetInterfaceAddresses();
  PAssert(addrs.GetSize() > 0, "No interface addresses");
  H323SetAliasAddress(addrs[0], body.m_sender, H225_AliasAddress::e_transportID);

  // add information
  body.m_updateInfo.SetSize(1);
  H501_UpdateInformation & info = body.m_updateInfo[0];
  info.m_descriptorInfo.SetTag(H501_UpdateInformation_descriptorInfo::e_descriptor);
  info.m_updateType.SetTag(updateType);
  descriptor->CopyTo(info.m_descriptorInfo);

  // make the request
  Request request(pdu.GetSequenceNumber(), pdu, peer);
  if (MakeRequest(request))
    return Confirmed;

  // if error was no service relationship, then establish relationship and try again
  switch (request.responseResult) {
    case Request::NoResponseReceived :
      PTRACE(2, "PeerElement\tUpdateDescriptor to " << peer << " failed due to no response");
      break;

    default:
      PTRACE(2, "PeerElement\tUpdateDescriptor to " << peer << " refused with unknown response " << (int)request.responseResult);
      return Rejected;
  }

  return Rejected;
}

H323Transaction::Response H323PeerElement::OnDescriptorUpdate(H501DescriptorUpdate & /*info*/)
{
  return H323Transaction::Ignore;
}

PBoolean H323PeerElement::OnReceiveDescriptorUpdate(const H501PDU & pdu, const H501_DescriptorUpdate & /*pduBody*/)
{
  H501DescriptorUpdate * info = new H501DescriptorUpdate(*this, pdu);
  if (!info->HandlePDU())
    delete info;

  return PFalse;
}

PBoolean H323PeerElement::OnReceiveDescriptorUpdateACK(const H501PDU & pdu, const H501_DescriptorUpdateAck & pduBody)
{
  if (!H323_AnnexG::OnReceiveDescriptorUpdateACK(pdu, pduBody))
    return PFalse;

  if (lastRequest->responseInfo != NULL)
    *(H501_MessageCommonInfo *)lastRequest->responseInfo = pdu.m_common;

  return PTrue;
}

///////////////////////////////////////////////////////////
//
// access request functions
//

PBoolean H323PeerElement::AccessRequest(const PString & searchAlias,
                                     PStringArray & destAliases,
                            H323TransportAddress & transportAddress,
                                          unsigned options)
{
  H225_AliasAddress h225searchAlias;
  H323SetAliasAddress(searchAlias, h225searchAlias);

  H225_ArrayOf_AliasAddress h225destAliases;
  if (!AccessRequest(h225searchAlias, h225destAliases, transportAddress, options))
    return PFalse;

  destAliases = H323GetAliasAddressStrings(h225destAliases);
  return PTrue;
}


PBoolean H323PeerElement::AccessRequest(const PString & searchAlias, 
                        H225_ArrayOf_AliasAddress & destAliases,
                             H323TransportAddress & transportAddress, 
                                           unsigned options)
{
  H225_AliasAddress h225searchAlias;
  H323SetAliasAddress(searchAlias, h225searchAlias);
  return AccessRequest(h225searchAlias, destAliases, transportAddress, options);
}

PBoolean H323PeerElement::AccessRequest(const H225_AliasAddress & searchAlias, 
                                  H225_ArrayOf_AliasAddress & destAliases,
                                       H323TransportAddress & transportAddress, 
                                                     unsigned options)
{
  H225_AliasAddress h225Address;
  if (!AccessRequest(searchAlias, destAliases, h225Address, options))
    return PFalse;

  transportAddress = H323GetAliasAddressString(h225Address);
  return PTrue;
}

PBoolean H323PeerElement::AccessRequest(const H225_AliasAddress & searchAlias, 
                                  H225_ArrayOf_AliasAddress & destAliases,
                                          H225_AliasAddress & transportAddress, 
                                                     unsigned options)
{
  // try each service relationship in turn
  POrdinalSet peersTried;

  for (PSafePtr<H323PeerElementServiceRelationship> sr = GetFirstRemoteServiceRelationship(PSafeReadOnly); sr != NULL; sr++) {

    // create the request
    H501PDU request;
    H501_AccessRequest & requestBody = request.BuildAccessRequest(GetNextSequenceNumber(), transport->GetLastReceivedAddress());

    // set dest information
    H501_PartyInformation & destInfo = requestBody.m_destinationInfo;
    destInfo.m_logicalAddresses.SetSize(1);
    destInfo.m_logicalAddresses[0] = searchAlias;

    // set protocols
    requestBody.IncludeOptionalField(H501_AccessRequest::e_desiredProtocols);
    H323PeerElementDescriptor::SetProtocolList(requestBody.m_desiredProtocols, options);

    // make the request
    H501PDU reply;
    H323PeerElement::Error error = SendAccessRequestByID(sr->serviceID, request, reply);
    H323TransportAddress peerAddr = sr->peer;

    while (error == Confirmed) {

      // make sure we got at least one template
      H501_AccessConfirmation & confirm = reply.m_body;
      H501_ArrayOf_AddressTemplate & addressTemplates = confirm.m_templates;
      if (addressTemplates.GetSize() == 0) {
        PTRACE(2, "Main\tAccessRequest for " << searchAlias << " from " << peerAddr << " contains no templates");
        break;
      }
      H501_AddressTemplate & addressTemplate = addressTemplates[0];

      // make sure patterns are returned
      H501_ArrayOf_Pattern & patterns = addressTemplate.m_pattern;
      if (patterns.GetSize() == 0) {
        PTRACE(2, "Main\tAccessRequest for " << searchAlias << " from " << peerAddr << " contains no patterns");
        break;
      }

      // make sure routes are returned
      H501_ArrayOf_RouteInformation & routeInfos = addressTemplate.m_routeInfo;
      if (routeInfos.GetSize() == 0) {
        PTRACE(2, "Main\tAccessRequest for " << searchAlias << " from " << peerAddr << " contains no routes");
        break;
      }
      H501_RouteInformation & routeInfo = addressTemplate.m_routeInfo[0];

      // make sure routes contain contacts
      H501_ArrayOf_ContactInformation & contacts = routeInfo.m_contacts;
      if (contacts.GetSize() == 0) {
        PTRACE(2, "Main\tAccessRequest for " << searchAlias << " from " << peerAddr << " contains no contacts");
        break;
      }
      H501_ContactInformation & contact = routeInfo.m_contacts[0];

      // get the address
      H225_AliasAddress contactAddress = contact.m_transportAddress;
      int tag = routeInfo.m_messageType.GetTag();
      if (tag == H501_RouteInformation_messageType::e_sendAccessRequest) {
        PTRACE(2, "Main\tAccessRequest for " << searchAlias << " redirected from " << peerAddr << " to " << contactAddress);
        peerAddr = H323GetAliasAddressString(contactAddress);
      }
      else if (tag == H501_RouteInformation_messageType::e_sendSetup) {

        // get the dest aliases
        destAliases.SetSize(addressTemplate.m_pattern.GetSize());
        PINDEX count = 0;
        PINDEX i;
        for (i = 0; i < addressTemplate.m_pattern.GetSize(); i++) {  
          if (addressTemplate.m_pattern[i].GetTag() == H501_Pattern::e_specific) {  
             H225_AliasAddress & alias = addressTemplate.m_pattern[i];  
             destAliases[count++] = alias;  
          }  
        }  
        destAliases.SetSize(count);  

        transportAddress = contactAddress;
        PTRACE(3, "Main\tAccessRequest for " << searchAlias << " returned " << transportAddress << " from " << peerAddr);
        return PTrue;
      }
      else { // H501_RouteInformation_messageType::e_nonExistent
        PTRACE(3, "Main\tAccessRequest for " << searchAlias << " from " << peerAddr << " returned nonExistent");
        break;
      }

      // this is the address to send the new request to
      H323TransportAddress addr = peerAddr;

      // create the request
      H501_AccessRequest & requestBody = request.BuildAccessRequest(GetNextSequenceNumber(), transport->GetLastReceivedAddress());

      // set dest information
      H501_PartyInformation & destInfo = requestBody.m_destinationInfo;
      destInfo.m_logicalAddresses.SetSize(1);
      destInfo.m_logicalAddresses[0] = searchAlias;

      // set protocols
      requestBody.IncludeOptionalField(H501_AccessRequest::e_desiredProtocols);
      H323PeerElementDescriptor::SetProtocolList(requestBody.m_desiredProtocols, options);

      // make the request
      error = SendAccessRequestByAddr(addr, request, reply);
    }
  }

  return PFalse;
}

///////////////////////////////////////////////////////////
//
// access request functions
//

H323PeerElement::Error H323PeerElement::SendAccessRequestByID(const OpalGloballyUniqueID & origServiceID, 
                                                                                 H501PDU & pdu, 
                                                                                 H501PDU & confirmPDU)
{
  if (PAssertNULL(transport) == NULL)
    return NoResponse;

  OpalGloballyUniqueID serviceID = origServiceID;

  for (;;) {

    // get the peer address
    H323TransportAddress peer;
    { 
      PSafePtr<H323PeerElementServiceRelationship> sr = remoteServiceRelationships.FindWithLock(H323PeerElementServiceRelationship(serviceID), PSafeReadOnly);
      if (sr == NULL)
        return NoServiceRelationship;
      peer = sr->peer;
    }

    // set the service ID
    pdu.m_common.IncludeOptionalField(H501_MessageCommonInfo::e_serviceID);
    pdu.m_common.m_serviceID = serviceID;

    // make the request
    Request request(pdu.GetSequenceNumber(), pdu, peer);
    request.responseInfo = &confirmPDU;
    if (MakeRequest(request))
      return Confirmed;

    // if error was no service relationship, then establish relationship and try again
    switch (request.responseResult) {
      case Request::NoResponseReceived :
        PTRACE(2, "PeerElement\tAccessRequest to " << peer << " failed due to no response");
        return Rejected;

      case Request::RejectReceived:
        switch (request.rejectReason) {
          case H501_ServiceRejectionReason::e_unknownServiceID:
            if (!OnRemoteServiceRelationshipDisappeared(serviceID, peer))
              return Rejected;
            break;
          default:
            return Rejected;
        }
        break;

      default:
        PTRACE(2, "PeerElement\tAccessRequest to " << peer << " refused with unknown response " << (int)request.responseResult);
        return Rejected;
    }
  }

  return Rejected;
}

H323PeerElement::Error H323PeerElement::SendAccessRequestByAddr(const H323TransportAddress & peerAddr, 
                                                                                   H501PDU & pdu, 
                                                                                   H501PDU & confirmPDU)
{
  if (PAssertNULL(transport) == NULL)
    return NoResponse;

  pdu.m_common.RemoveOptionalField(H501_MessageCommonInfo::e_serviceID);

  // make the request
  Request request(pdu.GetSequenceNumber(), pdu, peerAddr);
  request.responseInfo = &confirmPDU;
  if (MakeRequest(request))
    return Confirmed;

  // if error was no service relationship, then establish relationship and try again
  switch (request.responseResult) {
    case Request::NoResponseReceived :
      PTRACE(2, "PeerElement\tAccessRequest to " << peerAddr << " failed due to no response");
      break;

    case Request::RejectReceived:
      PTRACE(2, "PeerElement\tAccessRequest failed due to " << request.rejectReason);
      break;

    default:
      PTRACE(2, "PeerElement\tAccessRequest to " << peerAddr << " refused with unknown response " << (int)request.responseResult);
      break;
  }

  return Rejected;
}

H323Transaction::Response H323PeerElement::OnAccessRequest(H501AccessRequest & info)
{
  info.SetRejectReason(H501_AccessRejectionReason::e_noServiceRelationship);
  return H323Transaction::Reject;
}

PBoolean H323PeerElement::OnReceiveAccessRequest(const H501PDU & pdu, const H501_AccessRequest & /*pduBody*/)
{
  H501AccessRequest * info = new H501AccessRequest(*this, pdu);
  if (!info->HandlePDU())
    delete info;

  return PFalse;
}

PBoolean H323PeerElement::OnReceiveAccessConfirmation(const H501PDU & pdu, const H501_AccessConfirmation & pduBody)
{
  if (!H323_AnnexG::OnReceiveAccessConfirmation(pdu, pduBody))
    return PFalse;

  if (lastRequest->responseInfo != NULL)
    *(H501PDU *)lastRequest->responseInfo = pdu;

  return PTrue;
}

PBoolean H323PeerElement::OnReceiveAccessRejection(const H501PDU & pdu, const H501_AccessRejection & pduBody)
{
  if (!H323_AnnexG::OnReceiveAccessRejection(pdu, pduBody))
    return PFalse;

  return PTrue;
}

PBoolean H323PeerElement::MakeRequest(Request & request)
{
  requestMutex.Wait();
  PBoolean stat = H323_AnnexG::MakeRequest(request);
  requestMutex.Signal();
  return stat;
}

//////////////////////////////////////////////////////////////////////////////

PObject::Comparison H323PeerElementDescriptor::Compare(const PObject & obj) const
{ 
  H323PeerElementDescriptor & other = (H323PeerElementDescriptor &)obj;
  return descriptorID.Compare(other.descriptorID); 
}

void H323PeerElementDescriptor::CopyTo(H501_Descriptor & descriptor)
{
  descriptor.m_descriptorInfo.m_descriptorID = descriptorID;
  descriptor.m_descriptorInfo.m_lastChanged  = lastChanged.AsString("yyyyMMddhhmmss", PTime::GMT);
  descriptor.m_templates                     = addressTemplates;

  if (!gatekeeperID.IsEmpty()) {
    descriptor.IncludeOptionalField(H501_Descriptor::e_gatekeeperID);
    descriptor.m_gatekeeperID = gatekeeperID;
  }
}


PBoolean H323PeerElementDescriptor::ContainsNonexistent()
{
  PBoolean blocked = PFalse;

  // look for any nonexistent routes, which means this descriptor does NOT match
  PINDEX k, j;
  for (k = 0; !blocked && (k < addressTemplates.GetSize()); k++) {
	  H501_ArrayOf_RouteInformation & routeInfo = addressTemplates[k].m_routeInfo;
    for (j = 0; !blocked && (j < routeInfo.GetSize()); j++) {
      if (routeInfo[j].m_messageType.GetTag() == H501_RouteInformation_messageType::e_nonExistent)
        blocked = PTrue;
    }
  }

  return blocked;
}


PBoolean H323PeerElementDescriptor::CopyToAddressTemplate(H501_AddressTemplate & addressTemplate,
                                                   const H225_EndpointType & epInfo,
                                           const H225_ArrayOf_AliasAddress & aliases, 
                                           const H225_ArrayOf_AliasAddress & transportAddresses, 
                                                                    unsigned options)
{
  // add patterns for this descriptor
  addressTemplate.m_pattern.SetSize(aliases.GetSize());
  PINDEX j;
  for (j = 0; j < aliases.GetSize(); j++) {
    H501_Pattern & pattern = addressTemplate.m_pattern[j];
    if ((options & Option_WildCard) != 0)
      pattern.SetTag(H501_Pattern::e_wildcard);
    else 
      pattern.SetTag(H501_Pattern::e_specific);
    (H225_AliasAddress &)pattern = aliases[j];
  }

  // add transport addresses for this descriptor
  H501_ArrayOf_RouteInformation & routeInfos = addressTemplate.m_routeInfo;
  routeInfos.SetSize(1);
  H501_RouteInformation & routeInfo = routeInfos[0];

  if ((options & Option_NotAvailable) != 0)
    routeInfo.m_messageType.SetTag(H501_RouteInformation_messageType::e_nonExistent);

  else if ((options & Option_SendAccessRequest) != 0)
    routeInfo.m_messageType.SetTag(H501_RouteInformation_messageType::e_sendAccessRequest);

  else {
    routeInfo.m_messageType.SetTag(H501_RouteInformation_messageType::e_sendSetup);
    routeInfo.m_callSpecific = PFalse;
    routeInfo.IncludeOptionalField(H501_RouteInformation::e_type);
    routeInfo.m_type = epInfo;
  }

  routeInfo.m_callSpecific = PFalse;
  H501_ArrayOf_ContactInformation & contacts = routeInfos[0].m_contacts;
  contacts.SetSize(transportAddresses.GetSize());
  PINDEX i;
  for (i = 0; i < transportAddresses.GetSize(); i++) {
    H501_ContactInformation & contact = contacts[i];
    contact.m_transportAddress = transportAddresses[i];
    contact.m_priority         = H323PeerElementDescriptor::GetPriorityOption(options);
  }

  // add protocols
  addressTemplate.IncludeOptionalField(H501_AddressTemplate::e_supportedProtocols);
  SetProtocolList(addressTemplate.m_supportedProtocols, options);

  return PTrue;
}

/*
PBoolean H323PeerElementDescriptor::CopyFrom(const H501_Descriptor & descriptor)
{
  descriptorID                           = descriptor.m_descriptorInfo.m_descriptorID;
  //lastChanged.AsString("yyyyMMddhhmmss") = descriptor.m_descriptorInfo.m_lastChanged;
  addressTemplates                       = descriptor.m_templates;

  if (descriptor.HasOptionalField(H501_Descriptor::e_gatekeeperID))
    gatekeeperID = descriptor.m_gatekeeperID;
  else
    gatekeeperID = PString::Empty();

  return PTrue;
}
*/

void H323PeerElementDescriptor::SetProtocolList(H501_ArrayOf_SupportedProtocols & h501Protocols, unsigned options)
{
  h501Protocols.SetSize(0);
  int mask =1;
  do {
    if (options & mask) {
      int pos = h501Protocols.GetSize();
      switch (mask) {
        case H323PeerElementDescriptor::Protocol_H323:
          h501Protocols.SetSize(pos+1);
          h501Protocols[pos].SetTag(H225_SupportedProtocols::e_h323);
          break;

        case H323PeerElementDescriptor::Protocol_Voice:
          h501Protocols.SetSize(pos+1);
          h501Protocols[pos].SetTag(H225_SupportedProtocols::e_voice);
          break;

        default:
          break;
      }
    }
    mask *= 2;
  } while (mask != Protocol_Max);
}

unsigned H323PeerElementDescriptor::GetProtocolList(const H501_ArrayOf_SupportedProtocols & h501Protocols)
{
  unsigned options = 0;
  PINDEX i;
  for (i = 0; i < h501Protocols.GetSize(); i++) {
    switch (h501Protocols[i].GetTag()) {
      case H225_SupportedProtocols::e_h323:
        options += Protocol_H323;
        break;

      case H225_SupportedProtocols::e_voice:
        options += Protocol_Voice;
        break;

      default:
        break;
    }
  }
  return options;
}


#endif // OPAL_H501

// End of file ////////////////////////////////////////////////////////////////
