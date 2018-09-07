/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbReceiveEngine.cpp
 * @brief   The implementation of the IasAvbReceiveEngine class.
 * @date    2013
 */


#include "avb_streamhandler/IasAvbReceiveEngine.hpp"

#include "avb_streamhandler/IasDiaLogger.hpp"

#include "avb_streamhandler/IasAvbAudioStream.hpp"
#include "avb_streamhandler/IasAvbVideoStream.hpp"
#include "avb_streamhandler/IasAvbClockReferenceStream.hpp"
#include "avb_streamhandler/IasAvbPacket.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "avb_streamhandler/IasAvbStreamId.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEventInterface.hpp"

#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <cstring>

// Raw socket includes
#include <sys/types.h>
#include <sys/socket.h>

#include <linux/if_packet.h>
#include <linux/if_arp.h>
#include <linux/if_vlan.h>
#include <linux/sockios.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <cstdio>
#include <sys/select.h>

#include <limits>

#ifndef ETH_P_IEEE1722
#define ETH_P_IEEE1722 0x22F0
#endif

#if defined(DIRECT_RX_DMA)
#define RCTL        0x00100
#define RCTL_RXEN   (1 << 1)
#define SRRCTL(_n)  ((_n) < 4 ? (0x0280C + ((_n) * 0x100)) : \
         (0x0C00C + ((_n) * 0x40)))
#define RXDCTL(_n)  ((_n) < 4 ? (0x02828 + ((_n) * 0x100)) : \
         (0x0C028 + ((_n) * 0x40)))
#endif /* DIRECT_RX_DMA */

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbReceiveEngine::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasAvbReceiveEngine::IasAvbReceiveEngine()
: mInstanceName("IasAvbReceiveEngine")
, mEndThread(false)
, mReceiveThread(NULL)
, mLock()
, mEventInterface(NULL)
, mReceiveSocket(-1)
, mReceiveBuffer(NULL)
, mIgnoreStreamId(false)
, mLog(&IasAvbStreamHandlerEnvironment::getDltContext("_RXE"))
#if defined(DIRECT_RX_DMA)
, mIgbDevice(NULL)
, mRcvPacketPool(NULL)
, mPacketList()
, mRecoverIgbReceiver(true)
#endif /* DIRECT_RX_DMA */
, mRcvPortIfIndex(0)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}

/*
 *  Destructor.
 */
IasAvbReceiveEngine::~IasAvbReceiveEngine()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  cleanup();
}


/*
 *  Initialization method.
 */
IasAvbProcessingResult IasAvbReceiveEngine::init()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (NULL != mReceiveThread)
  {
    result = eIasAvbProcInitializationFailed;
  }
  else
  {
    uint64_t val = 0u;
    mIgnoreStreamId = (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxIgnoreStreamId, val) && (0u != val));

    mReceiveThread = new (nothrow) IasThread(this, "AvbRxWrk");
    if (NULL == mReceiveThread)
    {
      /**
       * @log Init failed: Not enough memory to create thread.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't create receive thread!");
      result = eIasAvbProcInitializationFailed;
    }

#if !defined(DIRECT_RX_DMA)
    if (eIasAvbProcOK == result)
    {
      mReceiveBuffer = new (nothrow) uint8_t[cReceiveBufferSize];
      if (NULL == mReceiveBuffer)
      {
        /**
         * @log Init failed: Not enough memory to create the buffer.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't create receive buffer!");
        result = eIasAvbProcInitializationFailed;
      }
    }
#else
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxRecoverIgbReceiver, mRecoverIgbReceiver);
    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Rx IGB Recovery:", mRecoverIgbReceiver ? "on" : "off");

    /*
     * Note: In the direct DMA mode the receive engine does not use the raw socket
     * to receive packets, but it still relies on the socket to set multicast mac
     * addresses to network interface. That's why still openReceiveSocket is called.
     */
#endif /* !DIRECT_RX_DMA */
    if (eIasAvbProcOK == result)
    {
      result = openReceiveSocket();
    }

    if (eIasAvbProcOK != result)
    {
      cleanup();
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbReceiveEngine::registerEventInterface( IasAvbStreamHandlerEventInterface * eventInterface )
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (NULL == mReceiveThread) // Is receive engine initialized?
  {
    result = eIasAvbProcNotInitialized; // Bad state
  }
  else if (NULL == eventInterface)
  {
    result = eIasAvbProcInvalidParam; // Invalid param
  }
  else if (NULL != mEventInterface) // Already registered ?
  {
    result = eIasAvbProcAlreadyInUse;
  }
  else
  {
    mEventInterface = eventInterface;
  }

  return result;
}


IasAvbProcessingResult IasAvbReceiveEngine::unregisterEventInterface( IasAvbStreamHandlerEventInterface * eventInterface )
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (NULL == mReceiveThread) // receive engine is not initialized
  {
    result = eIasAvbProcNotInitialized; // Bad state
  }
  else if ((NULL == eventInterface) || (mEventInterface != eventInterface))
  {
    result = eIasAvbProcInvalidParam; // Invalid param
  }
  else
  {
    mEventInterface = NULL;
  }

  return result;
}


IasAvbProcessingResult IasAvbReceiveEngine::start()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

#if defined(DIRECT_RX_DMA)
  /* Start direct DMA for RX */
  result = startIgbReceiveEngine();
#endif /* DIRECT_RX_DMA */

  if (eIasAvbProcOK == result)
  {
    if (NULL != mReceiveThread)
    {
      IasThreadResult res = mReceiveThread->start(true);
      if ((res != IasResult::cOk) && (res != IasThreadResult::cThreadAlreadyStarted))
      {
        result = eIasAvbProcThreadStartFailed;
      }
    }
    else
    {
      /**
       * @log Null pointer access: The returned mReceiveThread == NULL
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "mReceiveThread == NULL");
      result = eIasAvbProcNullPointerAccess;
    }

#if defined(DIRECT_RX_DMA)
    if (eIasAvbProcOK != result)
    {
      (void) stopIgbReceiveEngine();
    }
#endif /* DIRECT_RX_DMA */
  }

  if (eIasAvbProcOK == result)
  {
    IasDiaLogger* diaLogger = IasAvbStreamHandlerEnvironment::getDiaLogger();
    const int32_t* statusSocket = IasAvbStreamHandlerEnvironment::getStatusSocket();
    if ((NULL != diaLogger) && (NULL != statusSocket))
    {
      diaLogger->triggerListenerMediaReadyPacket(*statusSocket);
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbReceiveEngine::stop()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (NULL != mReceiveThread)
  {
    if (mReceiveThread->isRunning())
    {
      if (mReceiveThread->stop() != IasResult::cOk)
      {
        result = eIasAvbProcThreadStopFailed;
      }
      else
      {
#if defined(DIRECT_RX_DMA)
        (void) stopIgbReceiveEngine();
#endif /* DIRECT_RX_DMA */
      }
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "mReceiveThread == NULL");
    result = eIasAvbProcNullPointerAccess;
  }

  return result;
}


IasAvbProcessingResult IasAvbReceiveEngine::checkStreamIdInUse(const IasAvbStreamId & avbStreamId) const
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if (isValidStreamId(avbStreamId))
  {
    uint64_t id = avbStreamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "streamId already in use:", id);

    ret = eIasAvbProcAlreadyInUse;
  }

  return ret;
}


IasAvbProcessingResult IasAvbReceiveEngine::createReceiveAudioStream(IasAvbSrClass srClass,
                                                                     uint16_t const maxNumberChannels,
                                                                     uint32_t const sampleFreq,
                                                                     IasAvbAudioFormat const format,
                                                                     const IasAvbStreamId & streamId,
                                                                     const IasAvbMacAddress & destMacAddr,
                                                                     bool preconfigured)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX);

  IasAvbProcessingResult result = checkStreamIdInUse(streamId);

  if (eIasAvbProcOK == result)
  {
    IasAvbAudioStream *newAudioStream = new (nothrow) IasAvbAudioStream();
    if (NULL != newAudioStream)
    {
      result = newAudioStream->initReceive(srClass, maxNumberChannels, sampleFreq, format, streamId,
              destMacAddr, 2u, preconfigured);

      if (eIasAvbProcOK == result)
      {
        // add stream to vector
        StreamData data;
        data.stream = newAudioStream;
        data.lastState = newAudioStream->getStreamState();
        data.lastTimeDispatched = 0u;
        (void) lock();
        mAvbStreams[streamId] = data;
        (void) unlock();
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX,  "Init receive stream failed! Error=", int32_t(result));
        delete newAudioStream;
      }
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to instantiate AudioStream!");
      result = eIasAvbProcNotEnoughMemory;
    }
  }

  if (eIasAvbProcOK == result)
  {
    result = bindMcastAddr(destMacAddr);
    if (eIasAvbProcOK != result)
    {
      /*
       * Need to clean up the stream but shouldn't use destroyAvbStream() here
       * just to be safe. Because it internally calls unbindMcastAddr() which
       * might decrease the mac's ref counter despite of the failure of binding.
       */
      (void) lock();
      IasAvbStream *avbStream = mAvbStreams[streamId].stream;
      mAvbStreams.erase(streamId);
      delete avbStream;
      (void) unlock();
    }
  }

  return result;
}

IasAvbProcessingResult IasAvbReceiveEngine::createReceiveVideoStream(IasAvbSrClass srClass,
                                                                     uint16_t const maxPacketRate,
                                                                     uint16_t const maxPacketSize,
                                                                     IasAvbVideoFormat const format,
                                                                     const IasAvbStreamId & streamId,
                                                                     const IasAvbMacAddress & destMacAddr,
                                                                     bool preconfigured)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX);

  IasAvbProcessingResult result = checkStreamIdInUse(streamId);

  if (eIasAvbProcOK == result)
  {
    IasAvbVideoStream *newVideoStream = new (nothrow) IasAvbVideoStream();
    if (NULL != newVideoStream)
    {
      result = newVideoStream->initReceive(srClass, maxPacketRate, maxPacketSize, format, streamId,
              destMacAddr, 2u, preconfigured);

      if (eIasAvbProcOK == result)
      {
        // add stream to vector
        StreamData data;
        data.stream = newVideoStream;
        data.lastState = newVideoStream->getStreamState();
        data.lastTimeDispatched = 0u;
        (void) lock();
        mAvbStreams[streamId] = data;
        (void) unlock();
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Init receive stream failed! Error=", (int32_t(result)));
        delete newVideoStream;
      }
    }
    else
    {
      /**
       * @log Not enough memory: Could not create a new instance of Video Stream.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to instantiate VideoStream!");
      result = eIasAvbProcNotEnoughMemory;
    }
  }

  if (eIasAvbProcOK == result)
  {
    result = bindMcastAddr(destMacAddr);
    if (eIasAvbProcOK != result)
    {
      (void) lock();
      IasAvbStream *avbStream = mAvbStreams[streamId].stream;
      mAvbStreams.erase(streamId);
      delete avbStream;
      (void) unlock();
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbReceiveEngine::createReceiveClockReferenceStream(IasAvbSrClass srClass,
                                                                              IasAvbClockReferenceStreamType type,
                                                                              uint16_t maxCrfStampsPerPdu,
                                                                              const IasAvbStreamId & streamId,
                                                                              const IasAvbMacAddress & destMacAddr)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX);

  IasAvbProcessingResult result = checkStreamIdInUse(streamId);

  if (eIasAvbProcOK == result)
  {
    IasAvbClockReferenceStream *newStream = new (nothrow) IasAvbClockReferenceStream();
    if (NULL != newStream)
    {
      result = newStream->initReceive(srClass, type, maxCrfStampsPerPdu, streamId, destMacAddr);

      if (eIasAvbProcOK == result)
      {
        // add stream to vector
        StreamData data;
        data.stream = newStream;
        data.lastState = newStream->getStreamState();
        data.lastTimeDispatched = 0u;
        (void) lock();
        mAvbStreams[streamId] = data;
        (void) unlock();
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Init receive stream failed! Error =", int32_t(result));
        delete newStream;
      }
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to instantiate clock reference stream!");
      result = eIasAvbProcNotEnoughMemory;
    }
  }

  if (eIasAvbProcOK == result)
  {
    result = bindMcastAddr(destMacAddr);
    if (eIasAvbProcOK != result)
    {
      (void) lock();
      IasAvbStream *avbStream = mAvbStreams[streamId].stream;
      mAvbStreams.erase(streamId);
      delete avbStream;
      (void) unlock();
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbReceiveEngine::destroyAvbStream(IasAvbStreamId const & streamId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;
  (void) lock();
  AvbStreamMap::iterator it = mAvbStreams.find(streamId);

  if (it == mAvbStreams.end())
  {
    result = eIasAvbProcInvalidParam;
  }
  else
  {
    IasAvbStream *avbStream = it->second.stream;

    mAvbStreams.erase(streamId);

    (void) unbindMcastAddr(avbStream->getDmac());

    delete avbStream;
  }

  (void) unlock();
  return result;
}


IasAvbProcessingResult IasAvbReceiveEngine::connectAudioStreams(const IasAvbStreamId & avbStreamId, IasLocalAudioStream * localStream)
{
  IasAvbProcessingResult result = eIasAvbProcInvalidParam;

  /*
   * Should we hold the lock here to access mAvbStreams? No, because ApiLock guarantees that
   * concurrent API calls must not happen, we can trust mAvbStreams does not change while
   * connectAudioStreams() API is being served. i.e. destroyStream cannot be served in parallel.
   * It is applicable to connectVideoStreams, disconnectStreams and getAvbStreamInfo as well.
   * Also getStreamById and isValidStreamId do not need locking because they are called only
   * at the init phase before either RX engine or IPC thread starts up.
   */

  // look-up AVB stream using the specified AVB stream id
  AvbStreamMap::iterator it = mAvbStreams.find(avbStreamId);
  if (it != mAvbStreams.end())
  {
    AVB_ASSERT(NULL != it->second.stream);

    // if AVB stream has been found, hand-over instance of local
    // audio stream to the AVB audio stream so they can connect
    if (eIasAvbAudioStream == it->second.stream->getStreamType())
    {
      result = static_cast<IasAvbAudioStream*>(it->second.stream)->connectTo(localStream);
    }
    else
    {
      /**
       * @log The StreamId parameter provided does not have a AudioStream type.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Wrong type! AvbStreamId does not correspond to an AvbAudioStream");
    }
  }
  else // no stream has been found related to specified stream id
  {
    /**
     * @log The StreamId parameter provided can not be found in the AvbStream list.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not connect since AVB stream could not be found!");
  }

  return result;
}


IasAvbProcessingResult IasAvbReceiveEngine::connectVideoStreams(const IasAvbStreamId & avbStreamId, IasLocalVideoStream * localStream)
{
  IasAvbProcessingResult result = eIasAvbProcInvalidParam;

  // look-up AVB stream using the specified AVB stream id
  AvbStreamMap::iterator it = mAvbStreams.find(avbStreamId);
  if (it != mAvbStreams.end())
  {
    AVB_ASSERT(NULL != it->second.stream);

    // if AVB stream has been found, hand-over instance of local
    // audio stream to the AVB audio stream so they can connect
    if (eIasAvbVideoStream == it->second.stream->getStreamType())
    {
      result = static_cast<IasAvbVideoStream*>(it->second.stream)->connectTo(localStream);
    }
    else
    {
      /**
       * @log The StreamId parameter provided does not have a VideoStream type.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Wrong type! AvbStreamId does not correspond to an AvbVideoStream");
    }
  }
  else // no stream has been found related to specified stream id
  {
    /**
     * @log The StreamId parameter provided can not be found in the AvbStream list.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not connect since AVB stream could not be found!");
  }

  return result;
}


IasAvbProcessingResult IasAvbReceiveEngine::disconnectStreams(const IasAvbStreamId & avbStreamId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  // look-up AVB stream using the specified AVB stream id
  AvbStreamMap::iterator it = mAvbStreams.find(avbStreamId);
  if (it != mAvbStreams.end())
  {
    AVB_ASSERT(NULL != it->second.stream);

    // if AVB stream has been found, check type and connect to NULL to disconnect,
    // else return error.
    IasAvbStreamType streamType = it->second.stream->getStreamType();
    if (eIasAvbVideoStream == streamType)
    {
      result = static_cast<IasAvbVideoStream*>(it->second.stream)->connectTo(NULL);
    }
    else if (eIasAvbAudioStream == streamType)
    {
      result = static_cast<IasAvbAudioStream*>(it->second.stream)->connectTo(NULL);
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX,
        "Wrong type! AvbStreamId does not correspond to an AvbVideo or AvbAudio stream.");

      result = eIasAvbProcInvalidParam;
    }
  }
  else
  {
    uint64_t sid = avbStreamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Invalid streamId", sid);

    result = eIasAvbProcInvalidParam;
  }

  return result;
}


IasResult IasAvbReceiveEngine::shutDown()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  mEndThread = true;
  return IasResult::cOk;
}


IasResult IasAvbReceiveEngine::afterRun()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  return IasResult::cOk;
}


IasResult IasAvbReceiveEngine::beforeRun()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  mEndThread = false;
  return IasResult::cOk;
}


IasAvbProcessingResult IasAvbReceiveEngine::openReceiveSocket()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  struct sockaddr_ll recv_sa;
  struct ifreq ifr;
  int32_t ifindex;
  typedef int Int; // avoid complaints about naked fundamental types
  Int bufSize = 0u;
  socklen_t argSize = sizeof bufSize;
  const std::string* recvport = IasAvbStreamHandlerEnvironment::getNetworkInterfaceName();

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  // already opened or error querying network interface name
  if ((-1 != mReceiveSocket) || (NULL == recvport))
  {
    result = eIasAvbProcInitializationFailed;
  }

  if (eIasAvbProcOK == result)
  {
    // open socket in raw mode
    mReceiveSocket = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IEEE1722));

    if (mReceiveSocket == -1)
    {
      /**
       * @log Init failed: Raw receive socket could not be opened.
       */

      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX,  "mIgbDevice == NULL!");
      result = eIasAvbProcInitializationFailed;
    }
  }

  if (eIasAvbProcOK == result)
  {
    // verify if network interface exists
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, recvport->c_str(), (sizeof ifr.ifr_name) - 1u);
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

    if (ioctl(mReceiveSocket, SIOCGIFINDEX, &ifr) == -1)
    {
      /**
       * @log Init failed: Interface index not recognised.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "No such interface: ",
          recvport->c_str());
      result = eIasAvbProcInitializationFailed;
    }
    else
    {
      ifindex = ifr.ifr_ifindex;
      mRcvPortIfIndex = ifindex;
    }
  }

  if (eIasAvbProcOK == result)
  {
    // Read flags from network interface...
    strncpy(ifr.ifr_name, recvport->c_str(), (sizeof ifr.ifr_name) - 1u);
    ifr.ifr_name[(sizeof ifr.ifr_name) - 1u] = '\0';

    if (ioctl(mReceiveSocket, SIOCGIFFLAGS, &ifr) < 0)
    {
      /**
       * @log Init failed: IOCTL could not read out the flags from the NIC.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX,  "ioctl fails read out the current flags: ",
          int32_t(errno), " (", strerror(errno), ")");
      result = eIasAvbProcInitializationFailed;
    }
    else if ((ifr.ifr_flags & IFF_UP) == 0)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Warning: Interface ",
          recvport->c_str(), " is down");
    }
    else if ((ifr.ifr_flags & IFF_RUNNING) == 0)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Warning: Interface ",
          recvport->c_str(), "  is up, but not connected");
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "flags: ",
          ((ifr.ifr_flags & IFF_PROMISC) ? "PROMISC ON" : "PROMISC OFF"),
          ((ifr.ifr_flags & IFF_MULTICAST) ? "MULTICAST ON" : "MULTICAST OFF"),
          ((ifr.ifr_flags & IFF_ALLMULTI) ? "ALLMULTI ON" : "ALLMULTI OFF")
          );
    }
  }

#if defined(DIRECT_RX_DMA)
  (void) argSize;
#else
  if (eIasAvbProcOK == result)
  {
    if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxSocketRxBufSize, bufSize))
    {
      // This is not needed for the direct RX mode and even worse it requires the cap_net_admin privilege.
      if (setsockopt(mReceiveSocket, SOL_SOCKET, SO_RCVBUFFORCE, &bufSize, sizeof bufSize) < 0)
      {
        /**
         * @log Init failed: The receive buffer size could not be set.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "failed to set RCV buffer size: ",
            int32_t(errno), " (", strerror(errno), ")");
        result = eIasAvbProcInitializationFailed;
      }
    }
  }

  if (eIasAvbProcOK == result)
  {
    if (getsockopt(mReceiveSocket, SOL_SOCKET, SO_RCVBUF, &bufSize, &argSize) < 0)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "warning: failed to read RCV buffer size: ",
          int32_t(errno), " (", strerror(errno), ")");
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "socket RCV buffer size is ",
          (bufSize/2), " (", bufSize , "real)");
    }
  }
#endif

  if (eIasAvbProcOK == result)
  {
    memset(&recv_sa, 0, sizeof recv_sa);
    recv_sa.sll_family = AF_PACKET;
    recv_sa.sll_ifindex = ifindex;
    recv_sa.sll_protocol = htons(ETH_P_IEEE1722);
    recv_sa.sll_hatype = PACKET_MULTICAST;

    if (bind(mReceiveSocket, reinterpret_cast<sockaddr*>(&recv_sa), sizeof recv_sa) == -1)
    {
      /**
       * @log Init failed: Unable to bind socket to the interface.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "error binding socket to if",
          int32_t(ifindex), "(", ifr.ifr_name, ":", strerror(errno), ")");
      result = eIasAvbProcInitializationFailed;
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "bound socket to if",
          int32_t(ifindex), "(", ifr.ifr_name, ")");
    }
  }

  if (eIasAvbProcOK != result)
  {
    closeSocket();
  }

  return result;
}


IasResult IasAvbReceiveEngine::run()
{
  IasAvbStreamId avbStreamId;
  int32_t recv_length = 0;
  uint32_t packetsReceived = 0u;
  uint32_t packetsDispatched = 0u;
  uint32_t packetsDiscarded = 0u;
  uint32_t packetsValid = 0u;
  uint32_t cycles = 0u;
  uint64_t lastDebugOut = 0u;
  int32_t timeDiffMin = std::numeric_limits<int32_t>::max();
  int32_t timeDiffMax = std::numeric_limits<int32_t>::min();
  int64_t timeDiffAcc = 0;

  IasDiaLogger* diaLogger = IasAvbStreamHandlerEnvironment::getDiaLogger();

  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX);

  struct sched_param sparam;
  std::string policyStr = "fifo";
  int32_t priority = 1;

  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cSchedPolicy, policyStr);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cSchedPriority, priority);

  int32_t policy = (policyStr == "other") ? SCHED_OTHER : (policyStr == "rr") ? SCHED_RR : SCHED_FIFO;
  sparam.sched_priority = priority;

  int32_t errval = pthread_setschedparam(pthread_self(), policy, &sparam);
  if(0 == errval)
  {
    errval = pthread_getschedparam(pthread_self(), &policy, &sparam);
    if(0 == errval)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Parameter of ReceiveEngineThread set to policy ", policy,
          " / prio ", sparam.sched_priority);

    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error getting scheduler parameter: ", strerror(errval));
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error setting scheduler parameter: ", strerror(errval));
  }

#if defined(DIRECT_RX_DMA)
  IasAvbPacket* packet = NULL;
  uint32_t elapsedTimeNs = 0u; /* elapsed time (ns) without packet reception */
  uint32_t count = 0;
#else
  fd_set readSet;
  fd_set exceptSet;
  timeval selectWaitTime;
#endif /* DIRECT_RX_DMA */
  int32_t selectResult;
  IasAvbStreamId wildcardId(uint64_t(0u));
  IasAvbMacAddress wildcardMac;
  std::memset(wildcardMac, 0, cIasAvbMacAddressLength);

  uint32_t cycleWait = 2000000u; // ns
  uint32_t idleWait = 25000u; // 25ms, enough to deal with standard clock reference streams (50 PDU/s)
  uint32_t discardAfter = 0u; // ns
  bool doDiscardByPts;

  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxCycleWait, cycleWait);
#if defined(DIRECT_RX_DMA)
  /* cycleWait must be a non-zero value to calculate the time-out value */
  AVB_ASSERT(cycleWait != 0u);
#endif
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxIdleWait, idleWait))
  {
    // config value is specified in ns
    idleWait /= 1000u;
  }
  doDiscardByPts = IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxDiscardAfter, discardAfter);

  IasLibPtpDaemon* ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
  AVB_ASSERT(NULL != ptp);
  uint64_t now = ptp->getLocalTime();
  uint64_t lastTimeoutCheck = now;  // This is used to perform an timeout check for individual streams

  while (!mEndThread)
  {
    while (!IasAvbStreamHandlerEnvironment::isLinkUp() && !mEndThread)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "waiting for network link...");
      ::sleep( 1u );
    }

#if defined(DIRECT_RX_DMA)
    if ((elapsedTimeNs / 1000) >= idleWait) /* us */
    {
      /* invoke the error handling since the 'idleWait' time elapsed without packet reception */
      selectResult  = 0u;

      /* reset the counter */
      elapsedTimeNs = 0u;
    }
    else
    {
      /* poll the network interface to retrieve a received packet */
      selectResult = 1u;
    }
#else
    FD_ZERO(&readSet);
    FD_SET(mReceiveSocket, &readSet);
    FD_ZERO(&exceptSet);
    FD_SET(mReceiveSocket, &exceptSet);

    selectWaitTime.tv_sec = 0u;
    selectWaitTime.tv_usec = idleWait;

    selectResult = select( FD_SETSIZE, &readSet, NULL, &exceptSet, &selectWaitTime );
#endif /* DIRECT_RX_DMA */

    // should "now" be updated here rather than after the nanosleep?

    if (0 == selectResult)
    {
      // general timeout, notify streams that no data has arrived
      (void) lock();
      for (AvbStreamMap::iterator it = mAvbStreams.begin(); mAvbStreams.end() != it; it++)
      {
        (void) dispatchPacket(it->second, NULL, 0u, now);
      }
      (void) unlock();
    }
    else if (selectResult < 0)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "select error: ",
          int32_t(errno), " (", strerror(errno), ")");
      mEndThread = true;
    }
    else
    {
      // If idleWait period is over check whether a timeout for a particular stream has occurred. */
      if (now - lastTimeoutCheck > (idleWait * 1000u))
      {
        // Iterate over stream list and check for individual timeouts
        (void) lock();
        for (AvbStreamMap::iterator it = mAvbStreams.begin(); mAvbStreams.end() != it; it++)
        {
          // If the stream hasn't been serviced for idleWait period trigger stream state change notification
          if (now - it->second.lastTimeDispatched > (idleWait * 1000u))
          {
            (void) dispatchPacket(it->second, NULL, 0u, now);
          }
        }
        (void) unlock();
        lastTimeoutCheck = now; // Memorize the time of the last timeout check
      }
      else
      {
#if !DIRECT_RX_DMA
        if (FD_ISSET(mReceiveSocket, &readSet))
#endif /* !DIRECT_RX_DMA */
        {
          (void) lock(); // protect mAvbStreams

          for(;;)
          {
#if defined(DIRECT_RX_DMA)
            if (NULL != packet)
            {
              /* put back the used packet buffer */
              if (igb_refresh_buffers(mIgbDevice, eRxQueue0, reinterpret_cast<struct igb_packet **>(&packet), 1u) == 0)
              {
                packet = NULL;
              }
            }

            /* reset the variable */
            recv_length = -1u;

            if (NULL == packet)
            {
#if defined(DEBUG_LISTENER_UNCERTAINTY)
              const uint64_t rxTstamp = ptp->getLocalTime();
              const size_t rxTstampSz = sizeof(rxTstamp);
#endif
              /* try getting a received packet */
              count = 1u;
              if (igb_receive(mIgbDevice, eRxQueue0, reinterpret_cast<struct igb_packet **>(&packet), &count) == 0)
              {
                if (NULL != packet)
                {
                  /* a packet is available */
                  mReceiveBuffer = reinterpret_cast<uint8_t*>(packet->getBasePtr());
                  recv_length = packet->len;

                  /* reset the counter */
                  elapsedTimeNs = 0u;

#if defined(DEBUG_LISTENER_UNCERTAINTY)
                  /* DO NOT ENABLE THESE LINES FOR PRODUCTION SW */
                  if ((recv_length + rxTstampSz) <= cReceiveBufferSize)
                  {
                    /*
                     * put the current time to the bottom of the receive buffer
                     * assuming the received packet size is smaller than the buffer size of 2KB
                     */
                    uint64_t rxTstampBuf = uint64_t((mReceiveBuffer + recv_length + (rxTstampSz - 1u))) & ~(rxTstampSz - 1u);

                    // insert the received timestamp to the buffer just after the payload
                    *((uint64_t*)rxTstampBuf) = rxTstamp;
                  }
#endif
                }
              }
              else
              {
                /*
                 * RCTL.RXEN bit could mistakenly be turned off as initializing i210's direct rx mode if some programs
                 * such as ifconfig or commnand concurrently access network interface on i210. This will drop all
                 * incoming packets. Root cause is synchronization problem between libigb (user-side) and igb_avb
                 * (kernel-side). As a workaround, monitor the bit if there is no incoming packet and enable it in case.
                 * (defect: 201518)
                 */
                if (mRecoverIgbReceiver)
                {
                  uint32_t rctlReg = 0u;
                  (void) igb_readreg(mIgbDevice, RCTL, &rctlReg);
                  if (!(rctlReg & RCTL_RXEN))
                  {
                    rctlReg |= RCTL_RXEN;
                    (void) igb_writereg(mIgbDevice, RCTL, rctlReg);

                    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Rx IGB Recovery: enabled RCTL.RXEN ( regval =", rctlReg, ")");
                  }
                }
              }
            }
#else
            recv_length = static_cast<int32_t>(recvfrom(mReceiveSocket, &mReceiveBuffer[0], cReceiveBufferSize, MSG_DONTWAIT, NULL, NULL ));
#endif /* DIRECT_RX_DMA */
            if (recv_length < 0)
            {
              const int32_t err = errno;
              if ((EAGAIN == err) || (EWOULDBLOCK == err))
              {
                // no new packets, just leave loop
              }
              else
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "recvfrom error: ", int32_t(err),
                    " (", strerror(err), ")");
                mEndThread = true;
              }

              break;
            }
            else if (recv_length > 0)
            {
              const uint16_t * ethType = reinterpret_cast<uint16_t*>(mReceiveBuffer + (ETH_HLEN - 2u));
              if (*ethType == htons(ETH_P_8021Q))
              {
                ethType += 2u;
              }

              if (*ethType == htons(ETH_P_IEEE1722)) // valid AVTP packet detected
              {
                bool updateSmac = false;
                packetsReceived++;
                if (NULL != diaLogger)
                {
                  diaLogger->incRxCount();
                }

                const uint16_t* avtpBase16 = ethType + 1u;
                const uint8_t* avtpBase8 = reinterpret_cast<const uint8_t*>(avtpBase16);
                const uint32_t* avtpBase32 = reinterpret_cast<const uint32_t*>(avtpBase16);

#if defined(PERFORMANCE_MEASUREMENT)
                if (IasAvbStreamHandlerEnvironment::isAudioFlowLogEnabled()) // latency analysis
                {
                  uint32_t state = 0u;
                  uint64_t logtime = 0u;
                  (void) IasAvbStreamHandlerEnvironment::getAudioFlowLoggingState(state, logtime);

                  uint64_t tscNow = ptp->getTsc();

                  if ((0x02 == avtpBase8[0]) && // AAF
                          ((0u == state) || (tscNow - logtime > (uint64_t)(1e9)))) // measurement is not ongoing or timed-out
                  {
                    uint16_t streamDataLen = ntohs(avtpBase16[10]);
                    const uint32_t cBufSize = sizeof(uint16_t) * 64u;
                    static uint8_t zeroBuf[cBufSize];
                    if (0 != zeroBuf[0])
                    {
                      (void) std::memset(zeroBuf, 0, cBufSize);
                    }

                    if (streamDataLen > cBufSize)
                    {
                      streamDataLen = cBufSize;
                    }

                    if ((0 != avtpBase16[12]) ||
                        (0 != std::memcmp(&avtpBase16[12], zeroBuf, streamDataLen)))
                    {
                      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX,
                                  "latency-analysis(1): received samples from MAC system time =", tscNow);

                      IasAvbStreamHandlerEnvironment::setAudioFlowLoggingState(1u, tscNow);
                    }
                  }
                }
#endif

                avbStreamId.setStreamId(avtpBase8 + 4u);

                bool dispatch = true;

                if ((avtpBase8[1] & 0x80) == 0)
                {
                  // streamId invalid

                  /* NOTE: The RX engine does only handle stream data. Any other
                   * AVTPPDU has to be handled by other processes opening their
                   * own raw sockets (such as MRPD).
                   */
                  dispatch = false;
                }
                else if (doDiscardByPts && (avtpBase8[1] & 0x01))
                {
                  // timestamp valid
                  const int32_t delta = int32_t(now - ntohl(avtpBase32[3]));

                  timeDiffMin = timeDiffMin < delta ? timeDiffMin : delta;
                  timeDiffMax = timeDiffMax > delta ? timeDiffMax : delta;
                  timeDiffAcc += delta;

                  if (delta > int32_t(discardAfter))
                  {
                    dispatch = false;
                    packetsDiscarded++;
                  }
                }
                else
                {
                  // do nothing, dispatch is true already
                }

                if (dispatch)
                {
                  AvbStreamMap::iterator it = mAvbStreams.find(avbStreamId);

                  if (mAvbStreams.end() == it)
                  {
                    // not found, look for wildcard
                    it = mAvbStreams.find(wildcardId);

                    /*
                     * Extended wildcard semantics:
                     * If stream has been found by wildcard, and wildcard stream has DMAC != 0,
                     * and the DMAC matches, turn wildcard stream into regular stream by
                     * setting the StreamId and replacing it in the lookup map.
                     */

                    if (mAvbStreams.end() != it)
                    {
                      StreamData data = it->second;
                      AVB_ASSERT(NULL != data.stream);
                      if (0 == std::memcmp(data.stream->getDmac(), mReceiveBuffer, cIasAvbMacAddressLength))
                      {
                        data.stream->changeStreamId(avbStreamId);
                        mAvbStreams.erase(wildcardId);
                        mAvbStreams[avbStreamId] = data;
                        it = mAvbStreams.find(avbStreamId);
                      }
                      else if (0 == std::memcmp(data.stream->getDmac(), wildcardMac, cIasAvbMacAddressLength))
                      {
                        // just use the wildcard stream found
                      }
                      else
                      {
                        // no matching entry found
                        it = mAvbStreams.end();
                      }
                    }
                  }

                  if (mIgnoreStreamId && (mAvbStreams.end() == it))
                  {
                    /*
                     * still not found, "ignore mode" active, use first available stream
                     * NOTE: For testing only, this should be used only under lab conditions!
                     */

                    it = mAvbStreams.begin();
                  }

                  if (mAvbStreams.end() != it)
                  {
                    packetsDispatched++;

                    const uint8_t * sMac= mReceiveBuffer + 6u;
                    IasAvbStream *stream = it->second.stream;
                    AVB_ASSERT(NULL != stream);
                    if (0 != std::memcmp(stream->getSmac(), sMac, cIasAvbMacAddressLength))
                    {
                      updateSmac = true;
                    }

                    if (dispatchPacket(it->second, avtpBase8, recv_length - (avtpBase8 - mReceiveBuffer), now))
                    {
                      if (updateSmac)
                      {
                        stream->setSmac(sMac);
                      }
                      packetsValid++;
                    }
                  }
                }
              }
            }
            else
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX,  "unexpected: recvfrom( returned 0)");
            }
          }

          (void) unlock();
        }

        cycles++;

        if (cycleWait != 0u)
        {
          // sleep
          struct timespec req;
          struct timespec rem;
          req.tv_nsec = cycleWait;
          req.tv_sec = 0u;
          nanosleep(&req, &rem);

  #if defined(DIRECT_RX_DMA)
          /* increment the time-out counter */
          elapsedTimeNs += cycleWait; /* ns */
  #endif
        }
      }
    }

    now = ptp->getLocalTime();

    if ((now - lastDebugOut) > 1000000000u)
    {
      lastDebugOut = now;
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, packetsReceived,
          " SAF packets received , ",
          packetsDispatched, " dispatched, ",
          packetsValid, " valid, ",
          packetsDiscarded, " discarded, ",
          cycles, " cycles, ",
          (cycles > 0) ? float(packetsReceived)/float(cycles) : float(0), " pkt/cycle"
          );
      packetsDispatched = 0u;
      packetsValid = 0u;
      packetsDiscarded = 0u;
      cycles = 0u;

      if (doDiscardByPts)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "presentation time delta: ",
            timeDiffMin, " min, ",
            timeDiffMax, " max, ",
            (packetsReceived > 0) ? float(timeDiffAcc) / float(packetsReceived) : float(0), " avg/"
            );
        timeDiffMin = std::numeric_limits<int32_t>::max();
        timeDiffMax = std::numeric_limits<int32_t>::min();
        timeDiffAcc = 0;
      }
      packetsReceived = 0u;

      if (NULL != diaLogger)
      {
        diaLogger->clearRxCount();
      }
    }
  }

  return IasResult::cOk;
}


bool IasAvbReceiveEngine::dispatchPacket(StreamData &streamData, const void* packet, size_t length, uint64_t now)
{
  IasAvbStream* stream = streamData.stream;
  AVB_ASSERT(NULL != streamData.stream);

  (void) checkStreamState(streamData);

  // @@DIAG EARLY/LATE_TIMESTAMP
  stream->dispatchPacket(packet, length, now);
  streamData.lastTimeDispatched = now;          // Memorize the time when the stream has been dispatched

  return checkStreamState(streamData);
}


bool IasAvbReceiveEngine::checkStreamState(StreamData &streamData)
{
  AVB_ASSERT(NULL != streamData.stream);
  const IasAvbStream* stream = streamData.stream;
  IasAvbStreamState newState = stream->getStreamState();

  if (streamData.lastState != newState)
  {
    uint64_t streamId = stream->getStreamId();
    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << streamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "state change for stream ", ssStreamId.str(),
        ": ", int32_t(streamData.lastState),
        "->", int32_t(newState));
    streamData.lastState = newState;
    if (NULL != mEventInterface)
    {
      mEventInterface->updateStreamStatus(streamId, newState);
    }
  }

  return (IasAvbStreamState::eIasAvbStreamValid == newState);
}


/*
 *  Cleanup method.
 */
void IasAvbReceiveEngine::cleanup()
{
  if (mReceiveThread != NULL && mReceiveThread->isRunning())
  {
    mReceiveThread->stop();
  }

  delete mReceiveThread;
  mReceiveThread = NULL;

#if !defined(DIRECT_RX_DMA)
  delete[] mReceiveBuffer;
  mReceiveBuffer = NULL;
#endif /* !DIRECT_RX_DMA */

  for (AvbStreamMap::iterator it = mAvbStreams.begin(); it != mAvbStreams.end(); it++)
  {
    IasAvbStream * s = it->second.stream;
    AVB_ASSERT( NULL != s );

    /*
     * Should we call unbindMcastAddr() to remove the stream's mcast address from
     * the network interface? No because upon closing the socket, kernel will flush
     * the associated mcast list. So we don't need to explicitly remove it at shutdown.
     */

    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << s->getStreamId();
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "destroying stream", ssStreamId.str());
    delete s;
  }
  mAvbStreams.clear();

  (void) closeSocket();

#if defined(DIRECT_RX_DMA)
  (void) stopIgbReceiveEngine();
#endif /* DIRECT_RX_DMA */
}

bool IasAvbReceiveEngine::getAvbStreamInfo(const IasAvbStreamId &id,
                                           AudioStreamInfoList &audioStreamInfo,
                                           VideoStreamInfoList &videoStreamInfo,
                                           ClockReferenceStreamInfoList &clockRefStreamInfo) const
{
  uint64_t streamId = uint64_t(id);
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Stream Id =", streamId);
  bool found = false;
  for (AvbStreamMap::const_iterator it = mAvbStreams.begin(); it != mAvbStreams.end() && !found; ++it)
  {
    if (uint64_t(0u) == streamId || it->first == id)
    {
      IasAvbStream *stream = it->second.stream;
      IasAvbStreamDiagnostics diag = stream->getDiagnostics();
      bool preConfigured = stream->getPreconfigured();
      // Add info into list
      if (IasAvbStreamType::eIasAvbAudioStream == stream->getStreamType())
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Audio");
        IasAvbAudioStream *audioStream = static_cast<IasAvbAudioStream *>(stream);
        IasAvbAudioStreamAttributes att;
        att.setStreamId(it->first);
        att.setRxStatus(it->second.lastState);
        att.setDirection(stream->getDirection());

        const IasAvbMacAddress &smac_array = audioStream->getSmac();
        const IasAvbMacAddress &dmac_array = audioStream->getDmac();
        uint64_t dmac = 0;
        uint64_t smac = 0;
        for (unsigned int i=0; i < cIasAvbMacAddressLength; ++i)
        {
          dmac = dmac << 8;
          dmac += dmac_array[i];

          smac = smac << 8;
          smac += smac_array[i];
        }
        att.setDmac(dmac);
        att.setSourceMac(smac);

        att.setTxActive(false);
        att.setMaxNumChannels(audioStream->getMaxNumChannels());
        att.setSampleFreq(audioStream->getSampleFrequency());
        att.setFormat(audioStream->getAudioFormat());
        att.setNumChannels(audioStream->getLocalNumChannels());
        att.setLocalStreamId(audioStream->getLocalStreamId());

        if (NULL != audioStream->getAvbClockDomain())
        {
          att.setClockId(audioStream->getAvbClockDomain()->getClockDomainId());
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "Avb Clock Domain was NULL");
          att.setClockId(-1); // Debug hint of an issue.
        }

        att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeStatic);
        att.setPreconfigured(preConfigured);

        att.setDiagnostics(diag);
        audioStreamInfo.push_back(att);
      }
      else if (IasAvbStreamType::eIasAvbVideoStream == stream->getStreamType())
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Video");
        IasAvbVideoStream *videoStream = static_cast<IasAvbVideoStream *>(stream);
        IasAvbVideoStreamAttributes att;
        att.setStreamId(it->first);
        att.setRxStatus(it->second.lastState);
        att.setDirection(stream->getDirection());

        const IasAvbMacAddress &smac_array = videoStream->getSmac();
        const IasAvbMacAddress &dmac_array = videoStream->getDmac();
        uint64_t dmac = 0;
        uint64_t smac = 0;
        for (unsigned int i=0; i < cIasAvbMacAddressLength; ++i)
        {
          dmac = dmac << 8;
          dmac += dmac_array[i];

          smac = smac << 8;
          smac += smac_array[i];
        }
        att.setDmac(dmac);
        att.setSourceMac(smac);

        att.setTxActive(false);

        att.setLocalStreamId(videoStream->getLocalStreamId());
        att.setFormat(videoStream->getAvbVideoFormat());
        att.setMaxPacketRate(videoStream->getMaxPacketRate());
        att.setMaxPacketSize(videoStream->getMaxPacketSize());

        if (NULL != videoStream->getAvbClockDomain())
        {
          att.setClockId(videoStream->getAvbClockDomain()->getClockDomainId());
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "Avb Clock Domain was NULL");
          att.setClockId(-1); // Debug hint of an issue.
        }

        att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeStatic);
        att.setPreconfigured(preConfigured);

        att.setDiagnostics(diag);
        videoStreamInfo.push_back(att);
      }
      else if (IasAvbStreamType::eIasAvbClockReferenceStream == stream->getStreamType())
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " - Clock Reference");
        IasAvbClockReferenceStreamAttributes att;
        IasAvbClockReferenceStream *crfStream = static_cast<IasAvbClockReferenceStream *>(stream);
        att.setDirection(stream->getDirection());
        att.setType(crfStream->getCrfStreamType());
        att.setCrfStampsPerPdu(crfStream->getTimeStampsPerPdu());
        att.setCrfStampInterval(crfStream->getTimeStampInterval());
        att.setBaseFreq(crfStream->getBaseFrequency());
        att.setPull(crfStream->getClockMultiplier());
        att.setStreamId(it->first);

        if (NULL != crfStream->getAvbClockDomain())
        {
          att.setClockId(crfStream->getAvbClockDomain()->getClockDomainId());
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Avb Clock Domain was NULL!");
          att.setClockId(-1); // Debug hint of an issue.
        }

        const IasAvbMacAddress &smac_array = crfStream->getSmac();
        const IasAvbMacAddress &dmac_array = crfStream->getDmac();
        uint64_t dmac = 0;
        uint64_t smac = 0;
        for (unsigned int i=0; i < cIasAvbMacAddressLength; ++i)
        {
          dmac = dmac << 8;
          dmac += dmac_array[i];

          smac = smac << 8;
          smac += smac_array[i];
        }
        att.setDmac(dmac);
        att.setSourceMac(smac);

        att.setRxStatus(it->second.lastState);

        att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeStatic);
        att.setPreconfigured(preConfigured);

        att.setDiagnostics(diag);
        clockRefStreamInfo.push_back(att);
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Unknown stream type!");
      }

      if ((0u != streamId) && (it->first == id))
      {
        found = true;
      }
    }
  }

  return found;
}


#if defined(DIRECT_RX_DMA)
IasAvbProcessingResult IasAvbReceiveEngine::startIgbReceiveEngine()
{
  IasAvbProcessingResult result = eIasAvbProcInitializationFailed;

  IasAvbPacket* packet = NULL;

  uint8_t flexFilterData[cReceiveFilterDataSize];
  uint8_t flexFilterMask[cReceiveFilterMaskSize];

  struct ethhdr * ethHdr = reinterpret_cast<struct ethhdr*>(flexFilterData);
  uint16_t * vlanEthType = reinterpret_cast<uint16_t*>(flexFilterData + ETH_HLEN + 2u);

  mIgbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();
  if (NULL == mIgbDevice)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "igb device is not ready",
                 int32_t(errno), " (", strerror(errno), ")");
  }
  else
  {
    if (NULL != mRcvPacketPool)
    {
      // already started the receive functionality
    }
    else
    {
      // create RX packet buffers
      mRcvPacketPool = new (nothrow) IasAvbPacketPool(*mLog);
      if (NULL == mRcvPacketPool)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "failed to allocate packet pool",
            int32_t(errno), " (", strerror(errno), ")");
        result = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        result = mRcvPacketPool->init(cReceiveBufferSize, cReceivePoolSize);
        if (eIasAvbProcOK != result)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "failed to initialize packet pool",
			  int32_t(errno), " (", strerror(errno), ")");
        }
        else
        {
          while ((packet = mRcvPacketPool->getPacket()) != NULL)
          {
            if (igb_refresh_buffers(mIgbDevice, eRxQueue0, reinterpret_cast<igb_packet**>(&packet), 1u) != 0)
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "failed to refresh RX buffer",
                          int32_t(errno), " (", strerror(errno), ")");
              break;
            }
            mPacketList.push_back(packet);
          }

          if (mPacketList.size() < cReceivePoolSize)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "unable to get packet buffer",
                        int32_t(errno), " (", strerror(errno), ")");
            result = eIasAvbProcNotEnoughMemory;
          }
          else
          {
            // setup the filter
            std::memset((void*)flexFilterData, 0u, cReceiveFilterDataSize);
            std::memset((void*)flexFilterMask, 0u, cReceiveFilterMaskSize);

            // accept the IEEE1722 packets with a VLAN tag
            ethHdr->h_proto = htons(ETH_P_8021Q);
            *vlanEthType = htons(ETH_P_IEEE1722);

            flexFilterMask[1] = 0x30; /* 00110000b = ethtype */
            flexFilterMask[2] = 0x03; /* 00000011b = ethtype after a vlan tag */

            // length must be 8 byte-aligned
            uint32_t len = (uint32_t)sizeof(struct ethhdr) + 4u;
            len = ((len + (8u - 1u)) / 8u) * 8u;

            if (0 != igb_lock(mIgbDevice))
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "IGB lock failure");
              result = eIasAvbProcInitializationFailed;
            }
            else
            {
              // enable the filter on queue 0 with filter_id 0
              if (igb_setup_flex_filter(mIgbDevice, eRxQueue0, eRxFilter0, len, flexFilterData, flexFilterMask) != 0)
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "error enabling the flex filter",
                            int32_t(errno), " (", strerror(errno), ")");
                result = eIasAvbProcInitializationFailed;
              }
              else
              {
                /*
                 * SRRCTL.Drop_En : Drop Enabled.
                 *
                 * If set, packets received to the queue when no descriptors are available to store them are dropped.
                 * Default is 0b for queue 0. While AVBSH is at the stop state upon receiving the SIGSTOP signal, RX
                 * engine can't fetch packets from queue 0. Although we separate queue 0 from another queue for the
                 * best-effort packets, all queues share single packet buffer at the entry point of I210. Therefore if
                 * we do not permit dropping AVTP packets assigned to queue 0, the packet buffer will eventually be
                 * occupied by AVTP packets which interferes with the reception of the best-effort packets at the stop
                 * state.
                 *
                 * Enabling this bit will secure bandwidth for the best-effort packets at the stop state but on the
                 * other hand it may increase the threat of packet dropping of AVTP packets during normal operation.
                 * Since SIGSTOP is a non-catchable signal, it is impossible to switch the mode on the fly. Thus we have
                 * to decide the mode at the startup time with the cRxDiscardOverrun registry key.
                 */

                uint64_t rxDiscardOverrun = 0u;
                (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxDiscardOverrun, rxDiscardOverrun);

                uint32_t srrCtlReg = 0u;
                const uint32_t cSrrCtlDropEn = 0x80000000;

                (void) igb_readreg(mIgbDevice, SRRCTL(eRxQueue0), &srrCtlReg);

                if (0u == rxDiscardOverrun)
                {
                  srrCtlReg &= ~cSrrCtlDropEn;  // disable
                }
                else
                {
                  srrCtlReg |= cSrrCtlDropEn;   // enable
                }

                (void) igb_writereg(mIgbDevice, SRRCTL(eRxQueue0), srrCtlReg);

                DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Rx Overrun Drop:", (0u != rxDiscardOverrun) ? "on" : "off");
                DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "SRRCTL:", srrCtlReg);

                /* success */
                result = eIasAvbProcOK;
              }

              (void) igb_unlock(mIgbDevice);
            }
          }
        }
      }
    }
  }

  if (eIasAvbProcOK != result)
  {
    (void) stopIgbReceiveEngine();
  }

  return result;
}


void IasAvbReceiveEngine::stopIgbReceiveEngine()
{
  IasAvbPacket* packet = NULL;

  // Disable the Flex filterings
  if (NULL != mIgbDevice)
  {
    (void) igb_clear_flex_filter(mIgbDevice, eRxFilter0);

    /*
     * Disable the queue before releasing the memory allocated to this
     * queue. Also it will make sure that the filter is disabled and
     * all received packets are routed to the normal queue.
     */
    (void) igb_writereg(mIgbDevice, RXDCTL(eRxQueue0), 0u);
  }

  while (!mPacketList.empty())
  {
    packet = mPacketList.back();
    if (NULL != packet)
    {
      (void) IasAvbPacketPool::returnPacket(packet);
    }
    (void) mPacketList.pop_back();
  }

  if (NULL != mRcvPacketPool)
  {
    delete mRcvPacketPool;
    mRcvPacketPool = NULL;
  }
}
#endif /* DIRECT_RX_DMA */


IasAvbProcessingResult IasAvbReceiveEngine::bindMcastAddr(const IasAvbMacAddress &mCastMacAddr, bool bind)
{
  IasAvbProcessingResult result = eIasAvbProcErr;

  const std::string* recvport = IasAvbStreamHandlerEnvironment::getNetworkInterfaceName();
  AVB_ASSERT(recvport != NULL);

  packet_mreq recv_addr;

  (void) std::memset(&recv_addr, 0, sizeof(recv_addr));

  recv_addr.mr_ifindex = mRcvPortIfIndex;
  recv_addr.mr_type = PACKET_MR_MULTICAST;
  recv_addr.mr_alen = ETH_ALEN;

  avb_safe_result copyresult;
  copyresult = avb_safe_memcpy((void*)(recv_addr.mr_address), sizeof(recv_addr.mr_address),
                               (const void*)(mCastMacAddr), cIasAvbMacAddressLength);
  (void) copyresult;

  std::stringstream strMacAddr;
  strMacAddr << std::hex
             << std::setfill('0') << std::setw(2) << std::right << uint32_t(mCastMacAddr[0]) << ":"
             << std::setfill('0') << std::setw(2) << std::right << uint32_t(mCastMacAddr[1]) << ":"
             << std::setfill('0') << std::setw(2) << std::right << uint32_t(mCastMacAddr[2]) << ":"
             << std::setfill('0') << std::setw(2) << std::right << uint32_t(mCastMacAddr[3]) << ":"
             << std::setfill('0') << std::setw(2) << std::right << uint32_t(mCastMacAddr[4]) << ":"
             << std::setfill('0') << std::setw(2) << std::right << uint32_t(mCastMacAddr[5]);

  int optname = (true == bind) ? PACKET_ADD_MEMBERSHIP : PACKET_DROP_MEMBERSHIP;
  if (setsockopt(mReceiveSocket, SOL_PACKET, optname, &recv_addr, sizeof recv_addr) == -1)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "error setting multicast address for if",
        int32_t(mRcvPortIfIndex), "(", recvport->c_str(), ") bind =", optname, "mac =", strMacAddr.str(),
        ":", strerror(errno));
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "successfully set multicast address for if",
        int32_t(mRcvPortIfIndex), "(", recvport->c_str(), ") bind =", optname, "mac =", strMacAddr.str());

    /*
     * Note: Kernel manages mcast addresses with reference counters. It can appropriately handle
     * multiple add/drop requests on a same mcast address. Thus even if we accidentally have
     * multiple AVB streams bound on a same mcast address, we can simply issue the add/drop commands
     * without checking duplication.
     */
    result = eIasAvbProcOK;
  }

  return result;
}


void IasAvbReceiveEngine::emergencyShutdown()
{
#if defined(DIRECT_RX_DMA)
  if (NULL != mIgbDevice)
  {
    if (0 == igb_lock(mIgbDevice))
    {
      // route all packets to the best-effort queue
      (void) igb_clear_flex_filter(mIgbDevice, eRxFilter0);

      /*
       * Disable queue 0 so that AVTP packets remaining in the I210's packet buffer can be discarded.
       * Otherwise pending packets might keep occupying the buffer space if the SRRCTL.Drop_En bit is 0 and
       * interfere with the reception of the best-effort packets even after the shutdown of AVBSH.
       */
      (void) igb_writereg(mIgbDevice, RXDCTL(eRxQueue0), 0u);

      (void) igb_unlock(mIgbDevice);
    }
  }
#endif
}


} // namespace IasMediaTransportAvb
