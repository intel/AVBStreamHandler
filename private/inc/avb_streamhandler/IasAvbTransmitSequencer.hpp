/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbTransmitSequencer.hpp
 * @brief   Transmit sequenced perform the actual sending of AVB packets on a per-class basis.
 * @details The transmit sequencer runs a worker thread that checks a vector for active
 *          streams. If there are any, their packets will be requested from 'AvbStream' and
 *          be handed over to the 'igb' device. Packets from multiple streams are multiplexed
 *          based on their packet launch times. The worker thread starts on activation of
 *          the first AVB stream and will be stopped if the last AVB stream has been
 *          deactivated.
 * @date    2013
 */

#ifndef IASAVBTRANSMITSEQUENCER_HPP_
#define IASAVBTRANSMITSEQUENCER_HPP_

#include "IasAvbTypes.hpp"
#include "IasAvbStream.hpp"
#include "IasAvbStreamHandlerEnvironment.hpp"
#include "avb_helper/IasThread.hpp"
#include "avb_helper/IasIRunnable.hpp"
#include "avb_watchdog/IasWatchdogInterface.hpp"
#include <mutex>
#include <set>

namespace IasMediaTransportAvb {

class IasLocalAudioStream;
class IasLocalVideoStream;
class IasAvbClockDomain;
class IasAvbStreamHandlerEventInterface;

class IasAvbTransmitSequencer : private IasMediaTransportAvb::IasIRunnable
{
  public:
    /**
     *  @brief Constructor.
     */
    IasAvbTransmitSequencer(DltContext &ctx);

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbTransmitSequencer();

    /**
     * @brief Allocates internal resources and initializes instance.
     *
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult init(uint32_t queueIndex, IasAvbSrClass qavClass, bool doReclaim);

    /**
     *  @brief Clean up all allocated resources.
     *
     *  Calling this routine turns the object into the pre-init state, i.e.
     *  init() can be called again.
     *  Also called by the destructor.
     */
    void cleanup();

    /**
     * @brief register interface for event callbacks
     *
     * @param[in] interface pointer to instance implementing the callback interface
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbProcessingResult registerEventInterface( IasAvbStreamHandlerEventInterface * eventInterface );

    /**
     * @brief delete registration of interface for event callbacks
     *
     * @param[in] interface pointer to registered client
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbProcessingResult unregisterEventInterface( IasAvbStreamHandlerEventInterface * eventInterface );


    /**
     * @brief Starts the worker thread.
     *
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult start();

    /**
     * @brief Stops the worker thread.
     *
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult stop();


    /**
     * @brief add a stream to the list of streams to be processed by the transmit engine
     */
    IasAvbProcessingResult addStreamToTransmitList(IasAvbStream * stream);

    /**
     * @brief remove a stream from the list of streams to be processed by the transmit engine
     */
    IasAvbProcessingResult removeStreamFromTransmitList(IasAvbStream * stream);


    inline IasAvbSrClass getClass() const;

    inline uint32_t getCurrentBandwidth() const;

    /**
     * @brief update traffic shaper
     */
    void updateShaper();

    /**
     * @brief set the MaxFrameSize of High Class to the sequencer of Low Class
     *
     * The Low Class sequencer needs to know the MaxFrameSize of High Class
     * in order to calculate the HighCredit based on the equation below.
     *
     * LowClassHiCredit = (MaxInterferenceSize + HighClassMaxFrameSize) / (LinkRate - HighClassIdleSlope)
     *
     * setMaxFrameSizeHigh and getMaxFrameSizeHigh are used to tell the MaxFrameSize of High Class to
     * the Low Class sequencer. Note that since the High Class sequencer can determine the HighCredit
     * regardless of the MaxFrameSize of Low Class, there is no method such as setMaxFrameSizeLow().
     *
     */
    inline void setMaxFrameSizeHigh(uint32_t maxFrameSize);

    /**
     * @brief get the MaxFrameSize of High Class
     */
    inline uint32_t getMaxFrameSizeHigh();

  private:

    //
    // types
    //

    /**
     * @brief struct holding all config parameters for the TX engine
     */
    struct Config
    {
      Config();
      uint64_t txWindowWidthInit;       ///< initial width of the TX window in ns
      uint64_t txWindowWidth;           ///< width of the TX window in ns (window goes from "now" to x ns in the future)
      uint64_t txWindowPitchInit;       ///< initial iteration step with for TX window in ns
      uint64_t txWindowPitch;           ///< iteration step with for TX window in ns (window will be moved by x ns on each iteration)
      uint64_t txWindowCueThreshold;    ///< if current packet of stream is outdated by more than x ns, TX engine will dispose of packets until back in sync
      uint64_t txWindowResetThreshold;  ///< if current packet of stream is outdated by more than x ns, TX engine will reset the stream
      uint64_t txWindowPrefetchThreshold; ///< if current packet of stream is in the future by more than x ns, TX engine will reset the stream
      uint64_t txWindowMaxResetCount;   ///< maximum rest count TX engine can do for each stream during one TX window
      uint64_t txWindowMaxDropCount;    ///< maximum drop count TX engine can do for each stream during one TX window
      uint64_t txDelay;                 ///< delay launch of packet by x ns (to accomodate travel time through libigb and DMA)
      uint64_t txMaxBandwidth;          ///< maximum bandwidth to be used by all active streams in kBit/s
    };

    /**
     * @brief struct holding all diagnostic/debug variables for the TX engine
     */
    struct Diag
    {
      Diag();
      uint32_t sent;
      uint32_t dropped;
      uint32_t reordered;
      uint32_t debugOutputCount;
      uint32_t debugErrCount;
      uint32_t debugSkipCount;
      uint32_t debugTimingViolation;
      float avgPacketSent;
      float avgPacketReclaim;
      uint64_t debugLastLaunchTime;
      IasAvbStream * debugLastStream;
      uint64_t debugLastResetMsgOutputTime;
    };

    enum DoneState
    {
      eNotDone,
      eEndOfWindow,
      eDry,
      eWindowAdjust,
      eTxError
    };

    /// @brief helper type for packet sequencing
    struct StreamData
    {
      IasAvbStream * stream;
      IasAvbPacket * packet;
      uint64_t         launchTime;
      DoneState      done;

      bool operator<(const StreamData& x) const
      {
        // earlier launchTime should be served first (return true)
        return launchTime < x.launchTime;
      }
    };

    typedef std::list<StreamData> AvbStreamDataList;
    typedef std::set<IasAvbStream*> AvbStreamSet;

    //
    // helpers
    //

    //{@
    /// @brief IasRunnable implementation
    virtual IasResult beforeRun();
    virtual IasResult run();
    virtual IasResult shutDown();
    virtual IasResult afterRun();
    //@}

    //
    // constants
    //

    static const uint64_t cMinTxWindowPitch = 125000u; ///< minimum TX window pitch in ns
    static const uint64_t cMinTxWindowWidth = 250000u; ///< minimum TX window width in ns
    static const uint64_t cTxWindowAdjust = 125000u; ///< step width in ns for adjusting the TX window

    static const uint32_t cFlagEndThread = 1u;           ///< used to signal the worker thread it should end
    static const uint32_t cFlagRestartThread = 2u;       ///< used to signal the worker thread it should start over

    static const uint32_t cTxMaxInterferenceSize = 1522u; ///< assumed maximum frame size of Non-SR packets


    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbTransmitSequencer(IasAvbTransmitSequencer const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbTransmitSequencer& operator=(IasAvbTransmitSequencer const &other);

    /**
     * @brief re-claims the buffers for sent packets from the driver
     *
     * @returns number of re-claimed buffers
     */
    uint32_t reclaimPackets();

    /**
     * @brief check if link is up & running
     */
    void checkLinkStatus(bool &linkState);

    /**
     * @brief check for requests to activate/deactive streams and update TX sequence
     */
    void updateSequence(AvbStreamDataList::iterator & it);

    /**
     * @brief send packet, fetch next one, reorder TX sequence if necessary
     * @param[in] windowStart begin of TX window
     * @param[inout] nextStreamToService reference to next stream to service, affected by reordering
     * @return code for state of the serviced stream
     */
    DoneState serviceStream(uint64_t windowStart, AvbStreamDataList::iterator & nextStreamToService);

    /**
     * @brief update TX sequence depending on packet launch times, determine the next stream that sends a packet
     * @param[inout] iterator pointing to the element to be sorted
     */
    void sortByLaunchTime(AvbStreamDataList::iterator & it);

    /**
     * @brief generate diagnostic output for verbose mode
     */
    void logOutput(float elapsed, float reclaimed);

    /**
     * @brief helper to provide memory barrier functionality
     */
    inline void sync();

    /**
     * @brief let the thread sleep
     */
    inline void nssleep(uint32_t ns);

    /**
     * @brief return next iterator in sequence, considering wrap-around
     */
    inline AvbStreamDataList::iterator next(AvbStreamDataList::iterator it);

    /**
     * @brief return previous iterator in sequence, considering wrap-around
     */
    inline AvbStreamDataList::iterator prev(AvbStreamDataList::iterator it);

    /**
     * @brief reset all packet pools of a the active streams
     */
    void resetPoolsOfActiveStreams();

    inline bool isInitialized() const;

    ///
    /// Member Variables
    ///

    volatile uint32_t     mThreadControl;
    IasThread            *mTransmitThread;
    device_t             *mIgbDevice;
    uint32_t              mQueueIndex;
    IasAvbSrClass         mClass;
    int32_t               mRequestCount;
    int32_t               mResponseCount;
    uint32_t              mCurrentBandwidth;
    uint32_t              mCurrentMaxIntervalFrames;
    uint32_t              mMaxFrameSizeHigh; // used calculate HiCredit for Class B/C
    bool                  mUseShaper;
    uint32_t              mShaperBwRate;
    AvbStreamDataList     mSequence;
    AvbStreamSet          mActiveStreams;
    bool                  mDoReclaim;
    std::mutex            mLock;
    Diag                  mDiag;
    Config                mConfig;
    IasAvbStreamHandlerEventInterface *mEventInterface;
    DltContext           *mLog;           // context for Log & Trace
    IasWatchdog::IasWatchdogInterface *mWatchdog;
    //IasWatchdog::IasSystemdWatchdogManager *mWatchdog;
    bool                  mFirstRun;
    bool                  mBTMEnable;
    bool                  mStrictPktOrderEn;
};


inline void IasAvbTransmitSequencer::sync()
{
  // gcc specific
  __sync_synchronize();
}

inline void IasAvbTransmitSequencer::nssleep( uint32_t ns )
{
  // sleep some time and try again
  struct timespec req;
  struct timespec rem;
  req.tv_nsec = ns;
  req.tv_sec = 0;
  nanosleep(&req, &rem);
}

inline void IasAvbTransmitSequencer::setMaxFrameSizeHigh(uint32_t maxFrameSize)
{
  mMaxFrameSizeHigh = maxFrameSize;
}

inline uint32_t IasAvbTransmitSequencer::getMaxFrameSizeHigh()
{
  return mMaxFrameSizeHigh;
}

inline IasAvbTransmitSequencer::AvbStreamDataList::iterator IasAvbTransmitSequencer::next(AvbStreamDataList::iterator it)
{
  AVB_ASSERT(it != mSequence.end());
  it++;
  if (mSequence.end() == it)
  {
    it = mSequence.begin();
  }
  return it;
}

inline IasAvbTransmitSequencer::AvbStreamDataList::iterator IasAvbTransmitSequencer::prev(AvbStreamDataList::iterator it)
{
  if (it != mSequence.end())
  {
    if (mSequence.begin() == it)
    {
      it = mSequence.end();
    }
    it--;
  }
  return it;
}


inline IasAvbSrClass IasAvbTransmitSequencer::getClass() const
{
  return mClass;
}

inline bool IasAvbTransmitSequencer::isInitialized() const
{
  return (NULL != mTransmitThread);
}

inline uint32_t IasAvbTransmitSequencer::getCurrentBandwidth() const
{
  return mCurrentBandwidth;
}



} // namespace IasMediaTransportAvb

#endif /* IASAVBTRANSMITSEQUENCER_HPP_ */
