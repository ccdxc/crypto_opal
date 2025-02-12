/*
 * mediastrm.cxx
 *
 * Media Stream classes
 *
 * Open H323 Library
 *
 * Copyright (c) 1998-2001 Equivalence Pty. Ltd.
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
 * Contributor(s): ________________________________________.
 *
 * $Revision: 28025 $
 * $Author: rjongbloed $
 * $Date: 2012-07-13 03:32:47 -0500 (Fri, 13 Jul 2012) $
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "mediastrm.h"
#endif

#include <opal/buildopts.h>

#include <opal/mediastrm.h>

#if OPAL_VIDEO
#include <ptlib/videoio.h>
#include <codec/vidcodec.h>
#endif

#include <ptlib/sound.h>
#include <opal/patch.h>
#include <lids/lid.h>
#include <rtp/rtp.h>
#include <opal/transports.h>
#include <opal/rtpconn.h>
#include <opal/rtpep.h>
#include <opal/call.h>

#define MAX_PAYLOAD_TYPE_MISMATCHES 10


#define new PNEW


///////////////////////////////////////////////////////////////////////////////

OpalMediaStream::OpalMediaStream(OpalConnection & conn, const OpalMediaFormat & fmt, unsigned _sessionID, PBoolean isSourceStream)
  : connection(conn)
  , sessionID(_sessionID)
  , identifier(conn.GetCall().GetToken() + psprintf("_%u", sessionID))
  , mediaFormat(fmt)
  , m_paused(false)
  , isSource(isSourceStream)
  , isOpen(false)
  , defaultDataSize(mediaFormat.GetFrameSize()*mediaFormat.GetOptionInteger(OpalAudioFormat::TxFramesPerPacketOption(), 1))
  , timestamp(0)
  , marker(true)
  , mismatchedPayloadTypes(0)
  , m_payloadType(mediaFormat.GetPayloadType())
  , m_frameTime(mediaFormat.GetFrameTime())
  , m_frameSize(mediaFormat.GetFrameSize())
{
  connection.SafeReference();
  PTRACE(5, "Media\tCreated " << (isSource ? "Source" : "Sink") << ' ' << this);
}


OpalMediaStream::~OpalMediaStream()
{
  Close();
  connection.SafeDereference();
  PTRACE(5, "Media\tDestroyed " << (isSource ? "Source" : "Sink") << ' ' << this);
}

//ys add for control test
void OpalMediaStream::ctrl(int input)
{
    cout<<"alsa OpalMediastream ctrl"<<endl;
}

void OpalMediaStream::PrintOn(ostream & strm) const
{
  strm << GetClass() << '[' << this << "]-"
       << (isSource ? "Source" : "Sink")
       << '-' << mediaFormat;
}


OpalMediaFormat OpalMediaStream::GetMediaFormat() const
{
  return mediaFormat;
}


bool OpalMediaStream::UpdateMediaFormat(const OpalMediaFormat & newMediaFormat)
{
  // We make referenced copy of pointer so can't be deleted out from under us
  PatchPtr mediaPatch = m_mediaPatch;

  return mediaPatch != NULL ? mediaPatch->UpdateMediaFormat(newMediaFormat)
                            : InternalUpdateMediaFormat(newMediaFormat);
}


bool OpalMediaStream::InternalUpdateMediaFormat(const OpalMediaFormat & newMediaFormat)
{
  if (!mediaFormat.Update(newMediaFormat))
    return false;

  PTRACE(4, "Media\tMedia format updated on " << *this);
  m_payloadType = mediaFormat.GetPayloadType();
  m_frameTime = mediaFormat.GetFrameTime();
  m_frameSize = mediaFormat.GetFrameSize();
  return true;
}


PBoolean OpalMediaStream::ExecuteCommand(const OpalMediaCommand & command)
{
  // We make referenced copy of pointer so can't be deleted out from under us
  PatchPtr mediaPatch = m_mediaPatch;

  if (mediaPatch == NULL)
    return false;

  PTRACE(4, "Media\tExecute command \"" << command << "\" on " << *this << " for " << connection);

  if (mediaPatch->ExecuteCommand(command, IsSink()))
    return true;

  if (IsSink())
    return false;

  return connection.OnMediaCommand(*this, command);
}


PBoolean OpalMediaStream::Open()
{
  isOpen = true;
  return true;
}


PBoolean OpalMediaStream::Start()
{
  if (!Open())
    return false;

  // We make referenced copy of pointer so can't be deleted out from under us
  PatchPtr mediaPatch = m_mediaPatch;

  if (mediaPatch == NULL)
    return false;

  if (IsPaused()) {
    PTRACE(4, "Media\tStarting (paused) stream " << *this);
    return false;
  }

  PTRACE(4, "Media\tStarting stream " << *this);
  mediaPatch->Start();
  return true;
}


PBoolean OpalMediaStream::Close()
{
  if (!isOpen)
    return false;

  PTRACE(4, "Media\tClosing stream " << *this);

  if (!LockReadWrite())
    return false;

  // Allow for race condition where it is closed in another thread during the above wait
  if (!isOpen) {
    PTRACE(4, "Media\tAlready closed stream " << *this);
    UnlockReadWrite();
    return false;
  }

  isOpen = false;

  InternalClose();

  UnlockReadWrite();

  connection.OnClosedMediaStream(*this);
  SetPatch(NULL);
  connection.RemoveMediaStream(*this);

  PTRACE(5, "Media\tClosed stream " << *this);
  return true;
}


PBoolean OpalMediaStream::WritePackets(RTP_DataFrameList & packets)
{
  for (RTP_DataFrameList::iterator packet = packets.begin(); packet != packets.end(); ++packet) {
    if (!WritePacket(*packet))
      return false;
  }

  return true;
}


void OpalMediaStream::IncrementTimestamp(PINDEX size)
{
  timestamp += m_frameTime * (m_frameSize != 0 ? ((size + m_frameSize - 1) / m_frameSize) : 1);
}


PBoolean OpalMediaStream::ReadPacket(RTP_DataFrame & packet)
{
  if (!isOpen)
    return false;

  unsigned oldTimestamp = timestamp;

  if (defaultDataSize < (packet.GetSize() - RTP_DataFrame::MinHeaderSize)) {
    stringstream str;
    str << "Media stream buffer " << (packet.GetSize() - RTP_DataFrame::MinHeaderSize) << " too small for media packet " << defaultDataSize;
    PAssertAlways(str.str().c_str());
  }

  PINDEX lastReadCount;
  if (!ReadData(packet.GetPayloadPtr(), defaultDataSize, lastReadCount))
    return false;

  // If the ReadData() function did not change the timestamp then use the default
  // method or fixed frame times and sizes.
  if (oldTimestamp == timestamp)
    IncrementTimestamp(lastReadCount);

  packet.SetPayloadType(m_payloadType);
  packet.SetPayloadSize(lastReadCount);
  packet.SetTimestamp(oldTimestamp); // Beginning of frame
  packet.SetMarker(marker);
  marker = false;

  return true;
}


PBoolean OpalMediaStream::WritePacket(RTP_DataFrame & packet)
{
 //PTRACE(1, "WritePacket\tsend RTP frame");
  if (!isOpen)
    return false;

  timestamp = packet.GetTimestamp();

  int size = packet.GetPayloadSize();
  if (size > 0 && m_payloadType < RTP_DataFrame::MaxPayloadType) {
    if (packet.GetPayloadType() == m_payloadType) {
      PTRACE_IF(2, mismatchedPayloadTypes > 0,
                "H323RTP\tPayload type matched again " << m_payloadType);
      mismatchedPayloadTypes = 0;
    }
    else {
      mismatchedPayloadTypes++;
      if (mismatchedPayloadTypes < MAX_PAYLOAD_TYPE_MISMATCHES) {
        PTRACE(2, "Media\tRTP data with mismatched payload type,"
                  " is " << packet.GetPayloadType() << 
                  " expected " << m_payloadType <<
                  ", ignoring packet.");
        size = 0;
      }
      else {
        PTRACE_IF(2, mismatchedPayloadTypes == MAX_PAYLOAD_TYPE_MISMATCHES,
                  "Media\tRTP data with consecutive mismatched payload types,"
                  " is " << packet.GetPayloadType() << 
                  " expected " << m_payloadType <<
                  ", ignoring payload type from now on.");
      }
    }
  }

  if (size == 0) {
    PINDEX dummy;
    if (!InternalWriteData(NULL, 0, dummy))
      return false;
  }
  else {
    marker = packet.GetMarker();
    const BYTE * ptr = packet.GetPayloadPtr();

    while (size > 0) {
      PINDEX written;
      if (!InternalWriteData(ptr, size, written))
        return false;
      size -= written;
      ptr += written;
    }

    PTRACE_IF(1, size < 0, "Media\tRTP payload size too small, short " << -size << " bytes.");
  }

  packet.SetTimestamp(timestamp);

  return true;
}


bool OpalMediaStream::InternalWriteData(const BYTE * data, PINDEX length, PINDEX & written)
{
  unsigned oldTimestamp = timestamp;

  if (!WriteData(data, length, written) || (length > 0 && written == 0)) {
    PTRACE(2, "Media\tWriteData failed, written=" << written);
    return false;
  }

  // If the Write() function did not change the timestamp then use the default
  // method of fixed frame times and sizes.
  if (oldTimestamp == timestamp)
    IncrementTimestamp(written);

  return true;
}


PBoolean OpalMediaStream::ReadData(BYTE * buffer, PINDEX size, PINDEX & length)
{
  if (!isOpen) {
    length = 0;
    return false;
  }

  RTP_DataFrame packet(size);
  if (!ReadPacket(packet)) {
    length = 0;
    return false;
  }

  length = packet.GetPayloadSize();
  if (length > size)
    length = size;
  memcpy(buffer, packet.GetPayloadPtr(), length);
  timestamp = packet.GetTimestamp();
  marker = packet.GetMarker();
  return true;
}


PBoolean OpalMediaStream::WriteData(const BYTE * buffer, PINDEX length, PINDEX & written)
{
  if (!isOpen) {
    written = 0;
    return false;
  }

  written = length;
  RTP_DataFrame packet(length);
  memcpy(packet.GetPayloadPtr(), buffer, length);
  packet.SetPayloadType(m_payloadType);
  packet.SetTimestamp(timestamp);
  packet.SetMarker(marker);
  return WritePacket(packet);
}


PBoolean OpalMediaStream::PushPacket(RTP_DataFrame & packet)
{
  // We make referenced copy of pointer so can't be deleted out from under us
  PatchPtr mediaPatch = m_mediaPatch;

  // OpalMediaPatch::PushFrame() might block, do outside of mutex
  return mediaPatch != NULL && mediaPatch->PushFrame(packet);
}


PBoolean OpalMediaStream::SetDataSize(PINDEX dataSize, PINDEX /*frameTime*/)
{
  if (dataSize <= 0)
    return false;

  PTRACE_IF(4, defaultDataSize != dataSize, "Media\tSet data size from " << defaultDataSize << " to " << dataSize);
  defaultDataSize = dataSize;
  return true;
}


PBoolean OpalMediaStream::RequiresPatchThread(OpalMediaStream * /*sinkStream*/) const
{
  return RequiresPatchThread();
}


PBoolean OpalMediaStream::RequiresPatchThread() const
{
  return true;
}


bool OpalMediaStream::EnableJitterBuffer(bool) const
{
  return false;
}


bool OpalMediaStream::SetPaused(bool pause, bool fromPatch)
{
  // We make referenced copy of pointer so can't be deleted out from under us
  PatchPtr mediaPatch = m_mediaPatch;

  // If we are source, then update the sink side, and vice versa
  if (!fromPatch && mediaPatch != NULL)
    return mediaPatch->SetPaused(pause);

  PSafeLockReadWrite mutex(*this);
  if (!mutex.IsLocked())
    return false;

  if (m_paused == pause)
    return false;

  PTRACE(3, "Media\t" << (pause ? "Paused" : "Resumed") << " stream " << *this);
  m_paused = pause;

  connection.OnPauseMediaStream(*this, pause);
  return true;
}


PBoolean OpalMediaStream::SetPatch(OpalMediaPatch * patch)
{
  PatchPtr mediaPatch = m_mediaPatch.Set(patch);

#if PTRACING
  if (PTrace::CanTrace(4) && (patch != NULL || mediaPatch != NULL)) {
    ostream & trace = PTrace::Begin(4, __FILE__, __LINE__);
    if (patch == NULL)
      trace << "Removing patch " << *mediaPatch;
    else if (mediaPatch == NULL)
      trace << "Adding patch " << *patch;
    else
      trace << "Overwriting patch " << *mediaPatch << " with " << *patch;
    trace << " on stream " << *this << PTrace::End;
  }
#endif

  if (mediaPatch != NULL) {
    if (IsSink())
      mediaPatch->RemoveSink(this);
    else
      mediaPatch->Close();
  }

  return true;
}


void OpalMediaStream::AddFilter(const PNotifier & filter, const OpalMediaFormat & stage) const
{
  // We make referenced copy of pointer so can't be deleted out from under us
  PatchPtr mediaPatch = m_mediaPatch;

  if (mediaPatch != NULL)
    mediaPatch->AddFilter(filter, stage);
}


PBoolean OpalMediaStream::RemoveFilter(const PNotifier & filter, const OpalMediaFormat & stage) const
{
  // We make referenced copy of pointer so can't be deleted out from under us
  PatchPtr mediaPatch = m_mediaPatch;

  return mediaPatch != NULL && mediaPatch->RemoveFilter(filter, stage);
}


#if OPAL_STATISTICS
void OpalMediaStream::GetStatistics(OpalMediaStatistics & statistics, bool fromPatch) const
{
  // We make referenced copy of pointer so can't be deleted out from under us
  PatchPtr mediaPatch = m_mediaPatch;

  if (mediaPatch != NULL && !fromPatch)
    mediaPatch->GetStatistics(statistics, IsSink());
}
#endif


void OpalMediaStream::OnStartMediaPatch() 
{ 
  // We make referenced copy of pointer so can't be deleted out from under us
  PatchPtr mediaPatch = m_mediaPatch;

  if (mediaPatch != NULL)
    connection.OnStartMediaPatch(*mediaPatch);
}


void OpalMediaStream::OnStopMediaPatch(OpalMediaPatch & patch)
{ 
  connection.OnStopMediaPatch(patch);
}


///////////////////////////////////////////////////////////////////////////////

OpalMediaStreamPacing::OpalMediaStreamPacing(const OpalMediaFormat & mediaFormat)
  : m_isAudio(mediaFormat.GetMediaType() == OpalMediaType::Audio())
  , m_frameTime(mediaFormat.GetFrameTime())
  , m_frameSize(mediaFormat.GetFrameSize())
  , m_timeUnits(mediaFormat.GetTimeUnits())
  /* No more than 1 second of "slip", reset timing if thread does not execute
     the timeout for a whole second. Prevents too much bunching up of data
     in "bad" conditions, that really should not happen. */
  , m_delay(1000)
{
  PAssert(!(m_isAudio && m_frameSize == 0), PInvalidParameter);
}


void OpalMediaStreamPacing::Pace(bool reading, PINDEX bytes, bool & marker)
{
  unsigned timeToWait = m_frameTime;

  if (m_isAudio)
    timeToWait *= (bytes + m_frameSize - 1) / m_frameSize;
  else {
    if (reading)
      marker = true;
    else if (!marker)
      return;
  }

  m_delay.Delay(timeToWait/m_timeUnits);
}


///////////////////////////////////////////////////////////////////////////////

OpalNullMediaStream::OpalNullMediaStream(OpalConnection & conn,
                                         const OpalMediaFormat & mediaFormat,
                                         unsigned sessionID,
                                         bool isSource,
                                         bool isSyncronous)
  : OpalMediaStream(conn, mediaFormat, sessionID, isSource)
  , OpalMediaStreamPacing(mediaFormat)
  , m_isSynchronous(isSyncronous)
  , m_requiresPatchThread(isSyncronous)
{
}


OpalNullMediaStream::OpalNullMediaStream(OpalConnection & conn,
                                         const OpalMediaFormat & mediaFormat,
                                         unsigned sessionID,
                                         bool isSource,
                                         bool usePacingDelay,
                                         bool requiresPatchThread)
  : OpalMediaStream(conn, mediaFormat, sessionID, isSource)
  , OpalMediaStreamPacing(mediaFormat)
  , m_isSynchronous(usePacingDelay)
  , m_requiresPatchThread(requiresPatchThread)
{
}


PBoolean OpalNullMediaStream::ReadData(BYTE * buffer, PINDEX size, PINDEX & length)
{
  if (!isOpen)
    return false;

  memset(buffer, 0, size);
  length = size;

  if (m_isSynchronous)
    Pace(true, size, marker);
  return true;
}


PBoolean OpalNullMediaStream::WriteData(const BYTE * /*buffer*/, PINDEX length, PINDEX & written)
{
  if (!isOpen)
    return false;

  written = length != 0 ? length : defaultDataSize;

  if (m_isSynchronous)
    Pace(false, written, marker);
  return true;
}


bool OpalNullMediaStream::SetPaused(bool pause, bool fromPatch)
{
  if (!OpalMediaStream::SetPaused(pause, fromPatch))
    return false;

  // If coming out of pause, restart pacing delay
  if (!pause)
    m_delay.Restart();

  return true;
}


PBoolean OpalNullMediaStream::RequiresPatchThread() const
{
  return m_requiresPatchThread;
}


PBoolean OpalNullMediaStream::IsSynchronous() const
{
  return m_isSynchronous;
}


///////////////////////////////////////////////////////////////////////////////

OpalRTPMediaStream::OpalRTPMediaStream(OpalRTPConnection & conn,
                                   const OpalMediaFormat & mediaFormat,
                                                  PBoolean isSource,
                                             RTP_Session & rtp,
                                                  unsigned minJitter,
                                                  unsigned maxJitter)
  : OpalMediaStream(conn, mediaFormat, rtp.GetSessionID(), isSource),
    rtpSession(rtp),
    minAudioJitterDelay(minJitter),
    maxAudioJitterDelay(maxJitter)
{
  if (!mediaFormat.NeedsJitterBuffer())
    minAudioJitterDelay = maxAudioJitterDelay = 0;

  /* If we are a source then we should set our buffer size to the max
     practical UDP packet size. This means we have a buffer that can accept
     whatever the RTP sender throws at us. For sink, we set it to the
     maximum size based on MTU (or other criteria). */
  defaultDataSize = isSource ? conn.GetEndPoint().GetManager().GetMaxRtpPacketSize() : conn.GetMaxRtpPayloadSize();

  PTRACE(5, "Media\tCreated RTP media session, RTP=" << &rtp);
}


OpalRTPMediaStream::~OpalRTPMediaStream()
{
  Close();
}


PBoolean OpalRTPMediaStream::Open()
{
  if (isOpen)
    return true;

  rtpSession.SetEncoding(mediaFormat.GetMediaType().GetDefinition()->GetRTPEncoding());
  rtpSession.Reopen(IsSource());

  return OpalMediaStream::Open();
}


void OpalRTPMediaStream::InternalClose()
{
  // Break any I/O blocks and wait for the thread that uses this object to
  // terminate before we allow it to be deleted.
  rtpSession.Close(IsSource());
}


bool OpalRTPMediaStream::SetPaused(bool pause, bool fromPatch)
{
  if (!OpalMediaStream::SetPaused(pause, fromPatch))
    return false;

  // If coming out of pause, reopen the RTP session, even though it is probably
  // still open, to make sure any pending error/statistic conditions are reset.
  if (!pause)
    rtpSession.Reopen(IsSource());

  if (IsSource())
    EnableJitterBuffer(!pause);

  return true;
}


PBoolean OpalRTPMediaStream::ReadPacket(RTP_DataFrame & packet)
{
  if (!IsOpen())
    return false;

  if (IsSink()) {
    PTRACE(1, "Media\tTried to read from sink media stream");
    return false;
  }

  
  if (!rtpSession.ReadBufferedData(packet))
    return false;

  timestamp = packet.GetTimestamp();
  return true;
}


PBoolean OpalRTPMediaStream::WritePacket(RTP_DataFrame & packet)
{
  if (!IsOpen())
    return false;

  if (IsSource()) {
    PTRACE(1, "Media\tTried to write to source media stream");
    return false;
  }

  timestamp = packet.GetTimestamp();

  if (packet.GetPayloadSize() == 0)
    return true;

  packet.SetPayloadType(m_payloadType);
  return rtpSession.WriteData(packet);
}


PBoolean OpalRTPMediaStream::SetDataSize(PINDEX PTRACE_PARAM(dataSize), PINDEX /*frameTime*/)
{
  PTRACE(3, "Media\tRTP data size cannot be changed to " << dataSize << ", fixed at " << defaultDataSize);
  return true;
}


PBoolean OpalRTPMediaStream::IsSynchronous() const
{
  // Sinks never block, and source with jitter buffer never blocks
  return IsSource() && maxAudioJitterDelay == 0;
}


PBoolean OpalRTPMediaStream::RequiresPatchThread() const
{
  return !dynamic_cast<OpalRTPEndPoint &>(connection.GetEndPoint()).CheckForLocalRTP(*this);
}


bool OpalRTPMediaStream::EnableJitterBuffer(bool enab) const
{
  if (!IsOpen() || IsSink() || !RequiresPatchThread())
    return false;

  unsigned minJitter, maxJitter;
  if (enab && mediaFormat.NeedsJitterBuffer()) {
    minJitter = minAudioJitterDelay*mediaFormat.GetTimeUnits();
    maxJitter = maxAudioJitterDelay*mediaFormat.GetTimeUnits();
  }
  else
    minJitter = maxJitter = 0;

  rtpSession.SetJitterBufferSize(minJitter, maxJitter,
                                 mediaFormat.GetTimeUnits(),
                                 connection.GetEndPoint().GetManager().GetMaxRtpPacketSize());
  return true;
}


PBoolean OpalRTPMediaStream::SetPatch(OpalMediaPatch * patch)
{
  if (!isOpen || IsSink())
    return OpalMediaStream::SetPatch(patch);

  rtpSession.Close(true);
  bool ok = OpalMediaStream::SetPatch(patch);
  rtpSession.Reopen(true);
  return ok;
}


#if OPAL_STATISTICS
void OpalRTPMediaStream::GetStatistics(OpalMediaStatistics & statistics, bool fromPatch) const
{
  rtpSession.GetStatistics(statistics, IsSource());
  OpalMediaStream::GetStatistics(statistics, fromPatch);
}
#endif

///////////////////////////////////////////////////////////////////////////////

OpalRawMediaStream::OpalRawMediaStream(OpalConnection & conn,
                                       const OpalMediaFormat & mediaFormat,
                                       unsigned sessionID,
                                       PBoolean isSource,
                                       PChannel * chan, PBoolean autoDelete)
  : OpalMediaStream(conn, mediaFormat, sessionID, isSource)
  , m_channel(chan)
  , m_autoDelete(autoDelete)
  , m_silence(160) // At least 10ms
  , m_averageSignalSum(0)
  , m_averageSignalSamples(0)
{//TODO
 //pf=fopen("recordMediaStream.pcm","wb+");
  int alsaInput =  mediaFormat.GetOptionInteger(OpalAudioFormat::AlsaInputOption());
 //cout<<"**************init OpalRawMediaStream control******************"<<alsaInput <<endl;
  ctrl(alsaInput);
  //dong linux set to alsa channel.
}


OpalRawMediaStream::~OpalRawMediaStream()
{
//fclose(pf);
  Close();

  if (m_autoDelete)
    delete m_channel;
  m_channel = NULL;
}

//ys add for control test
void OpalRawMediaStream::ctrl(int input)
{
    PINDEX i = input;
    PINDEX j = 0;
    ((PSoundChannel *)m_channel)->GetBuffers(i,j);
}


PBoolean OpalRawMediaStream::ReadData(BYTE * buffer, PINDEX size, PINDEX & length)
{
  if (!isOpen)
    return false;

  length = 0;

  if (IsSink()) {
    PTRACE(1, "Media\tTried to read from sink media stream");
    return false;
  }

  PWaitAndSignal mutex(m_channelMutex);

  if (!IsOpen() || m_channel == NULL)
    return false;

  if (buffer == NULL || size == 0)
    return m_channel->Read(buffer, size);

  unsigned consecutiveZeroReads = 0;
  while (size > 0) {
//fwrite(buffer,size,1,pf);
//fflush(pf);
    if (!m_channel->Read(buffer, size))
      return false;

    PINDEX lastReadCount = m_channel->GetLastReadCount();
    if (lastReadCount != 0)
      consecutiveZeroReads = 0;
    else if (++consecutiveZeroReads > 10) {
      PTRACE(1, "Media\tRaw channel returned success with zero data multiple consecutive times, aborting.");
      return false;
    }

    CollectAverage(buffer, lastReadCount);

    buffer += lastReadCount;
    length += lastReadCount;
    size -= lastReadCount;
  }


  return true;
}


PBoolean OpalRawMediaStream::WriteData(const BYTE * buffer, PINDEX length, PINDEX & written)
{
  if (!isOpen) {
    PTRACE(1, "Media\tTried to write to closed media stream");
    return false;
  }

  written = 0;

  if (IsSource()) {
    PTRACE(1, "Media\tTried to write to source media stream");
    return false;
  }
  
  PWaitAndSignal mutex(m_channelMutex);

  if (!IsOpen() || m_channel == NULL) {
    PTRACE(1, "Media\tTried to write to media stream with no channel");
    return false;
  }

  /* We make the assumption that the remote is sending the same sized packets
     all the time and does not suddenly switch from 30ms to 20ms, even though
     this is technically legal. We have never yet seen it, and even if it did
     it doesn't hurt the silence insertion algorithm very much.

     So, silence buffer is set to be the largest chunk of audio the remote has
     ever sent to us. Then when they stop sending, (we get length==0) we just
     keep outputting that number of bytes to the raw channel until the remote
     starts up again.
     */

  if (buffer != NULL && length != 0)
    m_silence.SetMinSize(length);
  else {
    length = m_silence.GetSize();
    buffer = m_silence;
  }

  if (!m_channel->Write(buffer, length))
    return false;

  written = m_channel->GetLastWriteCount();
  CollectAverage(buffer, written);
  return true;
}


bool OpalRawMediaStream::SetChannel(PChannel * chan, bool autoDelete)
{
  if (chan == NULL || !chan->IsOpen()) {
    if (autoDelete)
      delete chan;
    return false;
  }

  m_channelMutex.Wait();

  PChannel * channelToDelete = m_autoDelete ? m_channel : NULL;
  m_channel = chan;
  m_autoDelete = autoDelete;

  SetDataSize(GetDataSize(), 1);

  m_channelMutex.Signal();

  delete channelToDelete; // Outside mutex

  PTRACE(4, "Media\tSet raw media channel to \"" << m_channel->GetName() << '"');
  return true;
}


void OpalRawMediaStream::InternalClose()
{
  if (m_channel != NULL)
    m_channel->Close();
}


unsigned OpalRawMediaStream::GetAverageSignalLevel()
{
  PWaitAndSignal mutex(m_averagingMutex);

  if (m_averageSignalSamples == 0)
    return UINT_MAX;

  unsigned average = (unsigned)(m_averageSignalSum/m_averageSignalSamples);
  m_averageSignalSum = average;
  m_averageSignalSamples = 1;
  return average;
}


void OpalRawMediaStream::CollectAverage(const BYTE * buffer, PINDEX size)
{
  PWaitAndSignal mutex(m_averagingMutex);

  size = size/2;
  m_averageSignalSamples += size;
  const short * pcm = (const short *)buffer;
  while (size-- > 0) {
    m_averageSignalSum += PABS(*pcm);
    pcm++;
  }
}


///////////////////////////////////////////////////////////////////////////////

OpalFileMediaStream::OpalFileMediaStream(OpalConnection & conn,
                                         const OpalMediaFormat & mediaFormat,
                                         unsigned sessionID,
                                         PBoolean isSource,
                                         PFile * file,
                                         PBoolean autoDel)
  : OpalRawMediaStream(conn, mediaFormat, sessionID, isSource, file, autoDel)
  , OpalMediaStreamPacing(mediaFormat)
{
}


OpalFileMediaStream::OpalFileMediaStream(OpalConnection & conn,
                                         const OpalMediaFormat & mediaFormat,
                                         unsigned sessionID,
                                         PBoolean isSource,
                                         const PFilePath & path)
  : OpalRawMediaStream(conn, mediaFormat, sessionID, isSource,
                       new PFile(path, isSource ? PFile::ReadOnly : PFile::WriteOnly),
                       true)
  , OpalMediaStreamPacing(mediaFormat)
{
}


PBoolean OpalFileMediaStream::IsSynchronous() const
{
  return false;
}


PBoolean OpalFileMediaStream::ReadData(BYTE * data, PINDEX size, PINDEX & length)
{
  if (!OpalRawMediaStream::ReadData(data, size, length))
    return false;

  Pace(true, size, marker);
  return true;
}


/**Write raw media data to the sink media stream.
   The default behaviour writes to the PChannel object.
  */
PBoolean OpalFileMediaStream::WriteData(const BYTE * data, PINDEX length, PINDEX & written)
{
  if (!OpalRawMediaStream::WriteData(data, length, written))
    return false;

  Pace(false, written, marker);
  return true;
}


///////////////////////////////////////////////////////////////////////////////

#if OPAL_PTLIB_AUDIO

OpalAudioMediaStream::OpalAudioMediaStream(OpalConnection & conn,
                                           const OpalMediaFormat & mediaFormat,
                                           unsigned sessionID,
                                           PBoolean isSource,
                                           PINDEX buffers,
                                           unsigned bufferTime,
                                           PSoundChannel * channel,
                                           PBoolean autoDel)
  : OpalRawMediaStream(conn, mediaFormat, sessionID, isSource, channel, autoDel)
  , m_soundChannelBuffers(buffers)
  , m_soundChannelBufferTime(bufferTime)
{
}


OpalAudioMediaStream::OpalAudioMediaStream(OpalConnection & conn,
                                           const OpalMediaFormat & mediaFormat,
                                           unsigned sessionID,
                                           PBoolean isSource,
                                           PINDEX buffers,
                                           unsigned bufferTime,
                                           const PString & deviceName)
  : OpalRawMediaStream(conn, mediaFormat, sessionID, isSource,
                       PSoundChannel::CreateOpenedChannel(PString::Empty(),
                                                          deviceName,
                                                          isSource ? PSoundChannel::Recorder
                                                                   : PSoundChannel::Player,
                                                          1, mediaFormat.GetClockRate(), 16),
                       true)
  , m_soundChannelBuffers(buffers)
  , m_soundChannelBufferTime(bufferTime)
{
}


PBoolean OpalAudioMediaStream::SetDataSize(PINDEX dataSize, PINDEX frameTime)
{
  PINDEX frameSize = frameTime*sizeof(short);
  unsigned clockRate = mediaFormat.GetClockRate();
  unsigned frameMilliseconds = (frameTime*1000+clockRate-1)/clockRate;

  /* For efficiency reasons we will not accept a packet size that is too small.
     We move it up to the next even multiple of the minimum, which has a danger
     of the remote not sending an even number of our multiplier, but 10ms seems
     universally done by everyone out there. */
  const unsigned MinBufferTimeMilliseconds = 10;
  if (frameMilliseconds < MinBufferTimeMilliseconds) {
    PINDEX minFrameCount = (MinBufferTimeMilliseconds+frameMilliseconds-1)/frameMilliseconds;
    frameSize = minFrameCount*frameTime*sizeof(short);
    frameMilliseconds = (minFrameCount*frameTime*1000+clockRate-1)/clockRate;
  }

  // Quantise dataSize up to multiple of the frame time
  PINDEX frameCount = (dataSize+frameSize-1)/frameSize;
  dataSize = frameCount*frameSize;

  // Calculate number of sound buffers from global system settings
  PINDEX soundChannelBuffers = (m_soundChannelBufferTime+frameMilliseconds-1)/frameMilliseconds;
  if (soundChannelBuffers < m_soundChannelBuffers)
    soundChannelBuffers = m_soundChannelBuffers;

  // Increase sound buffers if negotiated maximum larger than global settings
  if (soundChannelBuffers < frameCount)
    soundChannelBuffers = frameCount;

  PTRACE(3, "Media\tAudio " << (IsSource() ? "source" : "sink") << " data size set to "
         << dataSize << ", buffer size set to " << frameSize << " and " << soundChannelBuffers << " buffers.");
  return OpalMediaStream::SetDataSize(dataSize, frameTime) &&
         ((PSoundChannel *)m_channel)->SetBuffers(frameSize, soundChannelBuffers);
}


PBoolean OpalAudioMediaStream::IsSynchronous() const
{
  return true;
}

#endif // OPAL_PTLIB_AUDIO


///////////////////////////////////////////////////////////////////////////////

#if OPAL_VIDEO

OpalVideoMediaStream::OpalVideoMediaStream(OpalConnection & conn,
                                          const OpalMediaFormat & mediaFormat,
                                           unsigned sessionID,
                                           PVideoInputDevice * in,
                                           PVideoOutputDevice * out,
                                           PBoolean delIn,
                                           PBoolean delOut)
  : OpalMediaStream(conn, mediaFormat, sessionID, in != NULL)
  , m_inputDevice(in)
  , m_outputDevice(out)
  , m_autoDeleteInput(delIn)
  , m_autoDeleteOutput(delOut)
{
  PAssert(in != NULL || out != NULL, PInvalidParameter);
}


OpalVideoMediaStream::~OpalVideoMediaStream()
{
  Close();

  if (m_autoDeleteInput)
    delete m_inputDevice;

  if (m_autoDeleteOutput)
    delete m_outputDevice;
}


PBoolean OpalVideoMediaStream::SetDataSize(PINDEX dataSize, PINDEX frameTime)
{
  if (m_inputDevice != NULL) {
    PINDEX minDataSize = m_inputDevice->GetMaxFrameBytes();
    if (dataSize < minDataSize)
      dataSize = minDataSize;
  }
  if (m_outputDevice != NULL) {
    PINDEX minDataSize = m_outputDevice->GetMaxFrameBytes();
    if (dataSize < minDataSize)
      dataSize = minDataSize;
  }

  return OpalMediaStream::SetDataSize(sizeof(PluginCodec_Video_FrameHeader) + dataSize, frameTime);
}


bool OpalVideoMediaStream::InternalUpdateMediaFormat(const OpalMediaFormat & newMediaFormat)
{
  if (!OpalMediaStream::InternalUpdateMediaFormat(newMediaFormat))
    return false;

  unsigned width = mediaFormat.GetOptionInteger(OpalVideoFormat::FrameWidthOption(), PVideoFrameInfo::QCIFWidth);
  unsigned height = mediaFormat.GetOptionInteger(OpalVideoFormat::FrameHeightOption(), PVideoFrameInfo::QCIFHeight);

  if (m_inputDevice != NULL) {
    if (!m_inputDevice->SetFrameSizeConverter(width, height)) {
      PTRACE(1, "Media\tCould not set frame size in grabber to " << width << 'x' << height << " in " << mediaFormat);
      return false;
    }
    if (!m_inputDevice->SetFrameRate(mediaFormat.GetClockRate()/mediaFormat.GetFrameTime())) {
      PTRACE(1, "Media\tCould not set frame rate in grabber to " << (mediaFormat.GetClockRate()/mediaFormat.GetFrameTime()));
      return false;
    }
  }

  if (m_outputDevice != NULL) {
    if (!m_outputDevice->SetFrameSizeConverter(width, height)) {
      PTRACE(1, "Media\tCould not set frame size in video display to " << width << 'x' << height << " in " << mediaFormat);
      return false;
    }
  }

  return true;
}


PBoolean OpalVideoMediaStream::Open()
{
  if (isOpen)
    return true;

  unsigned width = mediaFormat.GetOptionInteger(OpalVideoFormat::FrameWidthOption(), PVideoFrameInfo::QCIFWidth);
  unsigned height = mediaFormat.GetOptionInteger(OpalVideoFormat::FrameHeightOption(), PVideoFrameInfo::QCIFHeight);

  if (m_inputDevice != NULL) {
    if (!m_inputDevice->SetColourFormatConverter(mediaFormat)) {
      PTRACE(1, "Media\tCould not set colour format in grabber to " << mediaFormat);
      return false;
    }
    if (!m_inputDevice->SetFrameSizeConverter(width, height)) {
      PTRACE(1, "Media\tCould not set frame size in grabber to " << width << 'x' << height << " in " << mediaFormat);
      return false;
    }
    if (!m_inputDevice->SetFrameRate(mediaFormat.GetClockRate()/mediaFormat.GetFrameTime())) {
      PTRACE(1, "Media\tCould not set frame rate in grabber to " << (mediaFormat.GetClockRate()/mediaFormat.GetFrameTime()));
      return false;
    }
    if (!m_inputDevice->Start()) {
      PTRACE(1, "Media\tCould not start video grabber");
      return false;
    }
    m_lastGrabTime = PTimer::Tick();
  }

  if (m_outputDevice != NULL) {
    if (!m_outputDevice->SetColourFormatConverter(mediaFormat)) {
      PTRACE(1, "Media\tCould not set colour format in video display to " << mediaFormat);
      return false;
    }
    if (!m_outputDevice->SetFrameSizeConverter(width, height)) {
      PTRACE(1, "Media\tCould not set frame size in video display to " << width << 'x' << height << " in " << mediaFormat);
      return false;
    }
  }

  SetDataSize(1, 1); // Gets set to minimum of device buffer requirements

  return OpalMediaStream::Open();
}


void OpalVideoMediaStream::InternalClose()
{
  if (m_inputDevice != NULL) {
    if (m_autoDeleteInput)
      m_inputDevice->Close();
    else
      m_inputDevice->Stop();
  }

  if (m_outputDevice != NULL) {
    if (m_autoDeleteOutput)
      m_outputDevice->Close();
    else
      m_outputDevice->Stop();
  }
}


PBoolean OpalVideoMediaStream::ReadData(BYTE * data, PINDEX size, PINDEX & length)
{

	
   	length=128;
	  PTimeInterval currentGrabTime = PTimer::Tick();
	  timestamp += (int)((currentGrabTime - m_lastGrabTime).GetMilliSeconds()*OpalMediaFormat::VideoClockRate/1000);
	  m_lastGrabTime = currentGrabTime;
	  marker = true;
	//dong change for h239
	if (sessionID == H323Capability::DefaultVideoSessionID)
	{

	  PThread::Sleep(12);

	} 
	else
	{
		PThread::Sleep(25);
	}
	m_pacing.Delay(1000/m_inputDevice->frameRate);
	
	return true;
	 //PTRACE(1, "OpalVideoMediaStream::ReadData");
	/*unsigned short sleep_time = 16;
#if defined(P_LINUX) || defined(P_MACOSX)
		usleep(sleep_time * 1000);
#else
		PThread::Sleep(sleep_time);
#endif*/
	
	
	///change by ck///////////////
	/*::Sleep(15);
	length=152064;
	return true;*/
	///change by ck///////////////
  /*if (!isOpen)
    return false;

  if (IsSink()) {
    PTRACE(1, "Media\tTried to read from sink media stream");
    return false;
  }

  if (m_inputDevice == NULL) {
    PTRACE(1, "Media\tTried to read from video display device");
    return false;
  }

  if (size < m_inputDevice->GetMaxFrameBytes()) {
    PTRACE(1, "Media\tTried to read with insufficient buffer size - " << size << " < " << m_inputDevice->GetMaxFrameBytes());
    return false;
  }


  unsigned width, height;
  m_inputDevice->GetFrameSize(width, height);
  OpalVideoTranscoder::FrameHeader * frame = (OpalVideoTranscoder::FrameHeader *)PAssertNULL(data);
  frame->x = frame->y = 0;
  frame->width = width;
  frame->height = height;

    
  PINDEX bytesReturned = size - sizeof(OpalVideoTranscoder::FrameHeader);
  unsigned flags = 0;
   PTRACE(1, "OpalVideoMediaStream::ReadData1");
  if (!m_inputDevice->GetFrameData((BYTE *)OPAL_VIDEO_FRAME_DATA_PTR(frame), &bytesReturned, flags)) {
    PTRACE(2, "Media\tFailed to grab frame from " << m_inputDevice->GetDeviceName());
    return false;
  }
  PTRACE(1, "OpalVideoMediaStream::ReadData2");
  PTimeInterval currentGrabTime = PTimer::Tick();
  timestamp += (int)((currentGrabTime - m_lastGrabTime).GetMilliSeconds()*OpalMediaFormat::VideoClockRate/1000);
  m_lastGrabTime = currentGrabTime;

  //dong add for apply IFrame
  if ((flags & PluginCodec_ReturnCoderRequestIFrame) != 0)
    ExecuteCommand(OpalVideoUpdatePicture());
  marker = true;
  length = bytesReturned;
  if (length > 0)
    length += sizeof(PluginCodec_Video_FrameHeader);

  if (m_outputDevice == NULL)
    return true;

  if (m_outputDevice->Start())
    return m_outputDevice->SetFrameData(0, 0, width, height, OPAL_VIDEO_FRAME_DATA_PTR(frame), true, flags);

  PTRACE(1, "Media\tCould not start video display device");
  if (m_autoDeleteOutput)
    delete m_outputDevice;
  m_outputDevice = NULL;
  return true;*/
}


PBoolean OpalVideoMediaStream::WriteData(const BYTE * data, PINDEX length, PINDEX & written)
{

	////change by ck /////////////
	written = length;
	//PThread::Sleep(5);
	return true ;
	////change by ck /////////////
  if (!isOpen)
    return false;

  if (IsSource()) {
    PTRACE(1, "Media\tTried to write to source media stream");
    return false;
  }

  if (m_outputDevice == NULL) {
    PTRACE(1, "Media\tTried to write to video capture device");
    return false;
  }

  // Assume we are writing the exact amount (check?)
  written = length;

  // Check for missing packets, we do nothing at this level, just ignore it
  if (data == NULL)
    return true;

  const OpalVideoTranscoder::FrameHeader * frame = (const OpalVideoTranscoder::FrameHeader *)data;

  if (frame->width ==0 && frame->height ==0 )//dong add for 8168 fake yuv
	  return true;
  if (!m_outputDevice->SetFrameSize(frame->width, frame->height)) {
    PTRACE(1, "Media\tCould not resize video display device to " << frame->width << 'x' << frame->height);
    return false;
  }

  if (!m_outputDevice->Start()) {
    PTRACE(1, "Media\tCould not start video display device");
    return false;
  }

  return m_outputDevice->SetFrameData(frame->x, frame->y,
                                      frame->width, frame->height,
                                      OPAL_VIDEO_FRAME_DATA_PTR(frame), marker);
}


PBoolean OpalVideoMediaStream::IsSynchronous() const
{
  return IsSource();
}

#endif // OPAL_VIDEO


///////////////////////////////////////////////////////////////////////////////

OpalUDPMediaStream::OpalUDPMediaStream(OpalConnection & conn,
                                      const OpalMediaFormat & mediaFormat,
                                       unsigned sessionID,
                                       PBoolean isSource,
                                       OpalTransportUDP & transport)
  : OpalMediaStream(conn, mediaFormat, sessionID, isSource),
    udpTransport(transport)
{
}

OpalUDPMediaStream::~OpalUDPMediaStream()
{
  Close();
}


PBoolean OpalUDPMediaStream::ReadPacket(RTP_DataFrame & packet)
{
  packet.SetPayloadType(m_payloadType);
  packet.SetPayloadSize(0);

  if (IsSink()) {
    PTRACE(1, "Media\tTried to read from sink media stream");
    return false;
  }

  PBYTEArray rawData;
  if (!udpTransport.ReadPDU(rawData)) {
    PTRACE(2, "Media\tRead on UDP transport failed: "
       << udpTransport.GetErrorText() << " transport: " << udpTransport);
    return false;
  }

  if (rawData.GetSize() > 0) {
    packet.SetPayloadSize(rawData.GetSize());
    memcpy(packet.GetPayloadPtr(), rawData.GetPointer(), rawData.GetSize());
  }

  return true;
}


PBoolean OpalUDPMediaStream::WritePacket(RTP_DataFrame & Packet)
{
  if (IsSource()) {
    PTRACE(1, "Media\tTried to write to source media stream");
    return false;
  }

  if (Packet.GetPayloadSize() > 0) {
    if (!udpTransport.Write(Packet.GetPayloadPtr(), Packet.GetPayloadSize())) {
      PTRACE(2, "Media\tWrite on UDP transport failed: "
         << udpTransport.GetErrorText() << " transport: " << udpTransport);
      return false;
    }
  }

  return true;
}


PBoolean OpalUDPMediaStream::IsSynchronous() const
{
  return false;
}


void OpalUDPMediaStream::InternalClose()
{
  udpTransport.Close();
}


// End of file ////////////////////////////////////////////////////////////////
