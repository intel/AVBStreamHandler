/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbHwCaptureClockDomain.cpp
 * @brief   The definition of the IasAvbHwCaptureClockDomain class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbHwCaptureClockDomain.hpp"

#include <math.h>
#include <string.h>

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbHwCaptureClockDomain::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

#define HW_TIME_CAPTURING  // if not set, pps (pulse per second) is adjusted

#define SYSTIMR           0xB6F8   // sub nsec
#define SYSTIML           0xB600   // nsec
#define SYSTIMH           0xB604   // sec
#define TRGTTIML0         0xB644   // nsec
#define TRGTTIMH0         0xB648   // sec
#define TRGTTIML1         0xB64C   // nsec
#define TRGTTIMH1         0xB650   // sec
#define TSSDP             0x003C   // Time Sync SDP Configuration Register
#define TSAUXC            0xB640   // Auxiliary Control Register
#define CTRL_REG          0x0000
#define CTRL_SDP0_DATA    0x00040000
#define CTRL_EXT_REG      0x0018

#define AUXSTMPL0         0xB65C
#define AUXSTMPH0         0xB660

#define AUXSTMPL1         0xB664
#define AUXSTMPH1         0xB668

// for Chris Hall Algorithm
#define IGB_CTRL          0x0000
#define FREQOUT0          0xB654

#define PULSE_LENGTH      500000000
#define THRESHOLD         700000000 // threshold have to exceed the pulse length


/*
 *  Constructor.
 */
IasAvbHwCaptureClockDomain::IasAvbHwCaptureClockDomain()
  : IasAvbClockDomain(IasAvbStreamHandlerEnvironment::getDltContext("_HCD"), eIasAvbClockDomainHw)
  , mInstanceName("IasAvbHwCaptureClockDomain")
  , mEndThread(false)
  , mHwCaptureThread(NULL)
  , mIgbDevice(NULL)
  , mNominal(93.75) // default value for IVI-BRD2 (48000/512)
  , mSleep(2000000u)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  double timeConstant = 1.0;
  double factorLong   = 1.0;
  double factorUnlock = 1.0;
  uint32_t threshold1 = 100000u;
  uint32_t threshold2 = 100000u;

  uint64_t val = 0u;
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkHwTimeConstant, val))
  {
    timeConstant = double(val) * 0.001;
  }
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkHwDeviationLongterm, val))
  {
    factorLong = double(val) * 0.001;
  }
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkHwDeviationUnlock, val))
  {
    factorUnlock = double(val) * 0.001;
  }
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockHwCapFrequency, val))
  {
    mNominal = double(val) * 0.001;
    // calculate poll interval: according to Shannon, twice the frequency and then some
    mSleep = uint32_t(1e9 / 2.5 / mNominal);
    if (mSleep < 1000000u)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "Warning: very short sleep interval for HW capturing poll:",
          mSleep);
    }
  }

  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkHwLockTreshold1, threshold1);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkHwLockTreshold2, threshold2);


  setFilter(timeConstant, uint32_t(mNominal));
  setLockThreshold1(threshold1);
  setLockThreshold2(threshold2);
  setDerivationFactors(factorLong, factorUnlock);
}

/*
 *  Constructor.
 */
IasAvbProcessingResult IasAvbHwCaptureClockDomain::init()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (NULL == mHwCaptureThread)
  {
    setInitialValue( 1.0 );
    mIgbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();
    if (NULL == mIgbDevice)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "mIgbDevice == NULL!");
      result = eIasAvbProcInitializationFailed;
    }

    if (eIasAvbProcOK == result)
    {
      mHwCaptureThread = new (nothrow) IasThread(this, "AvbHwCapture");
      if (NULL == mHwCaptureThread)
      {
        /**
         * @log Not enough memory to create the thread.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't create HwCapture thread!!");
        result = eIasAvbProcInitializationFailed;
      }
      else
      {
        start();
      }
    }

    if (eIasAvbProcOK != result)
    {
      cleanup();
    }
  }
  else
  {
    // already initialized - missing of that value in enum
    result = eIasAvbProcInitializationFailed;
  }

  return result;
}


/*
 *  Cleanup.
 */
void IasAvbHwCaptureClockDomain::cleanup()
{
  if (mHwCaptureThread != NULL && mHwCaptureThread->isRunning())
  {
    mHwCaptureThread->stop();
  }

  delete mHwCaptureThread;
  mHwCaptureThread = NULL;
}


/*
 *  Destructor.
 */
IasAvbHwCaptureClockDomain::~IasAvbHwCaptureClockDomain()
{
  (void) cleanup();

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}


IasAvbProcessingResult IasAvbHwCaptureClockDomain::start()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (NULL != mHwCaptureThread)
  {
    IasThreadResult res = mHwCaptureThread->start(true);
    if ((res != IasResult::cOk) && (res != IasThreadResult::cThreadAlreadyStarted))
    {
      result = eIasAvbProcThreadStartFailed;
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "mHwCaptureThread == NULL");
    result = eIasAvbProcNullPointerAccess;
  }

  return result;
}


IasAvbProcessingResult IasAvbHwCaptureClockDomain::stop()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (NULL != mHwCaptureThread)
  {
    if (mHwCaptureThread->isRunning())
    {
      if (mHwCaptureThread->stop() != IasResult::cOk)
      {
        result = eIasAvbProcThreadStopFailed;
      }
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "mHwCaptureThread == NULL");
    result = eIasAvbProcNullPointerAccess;
  }

  return result;
}


IasResult IasAvbHwCaptureClockDomain::beforeRun()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  mEndThread = false;
  return IasResult::cOk;
}


IasResult IasAvbHwCaptureClockDomain::run()
#ifdef HW_TIME_CAPTURING
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  uint32_t tssdp      = 0;
  uint32_t tsauxc     = 0;
  uint32_t ctrl       = 0;
  uint32_t lastCtrl   = 0u;
  uint32_t value      = 0;
  uint32_t auxstmpl   = 0;
  uint32_t auxstmph   = 0;
  int64_t auxstmpNow  = 0;
  int64_t auxstmpLast = 0;
  int64_t diff        = 0;
  uint32_t cnt        = 0;

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
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX,
                   "Parameter of HWCaptureClockThread set to policy=",
                   policy,
                   "/ prio=",
                   sparam.sched_priority);
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error getting scheduler parameter:",
          strerror(errval));
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error setting scheduler parameter:",
        strerror(errval));
  }

  const uint32_t cntMax = mSleep ? (400000000u / mSleep) : 0u;

  // Upon a change in the input level of one of the SDP pins that was configured to detect Time stamp
  // events using the TSSDP register, a time stamp of the system time is captured into one of the two
  // auxiliary time stamp registers (AUXSTMPL/H0 or AUXSTMPL/H1). Software enables the time stamp of
  // input event as follow:

  // Set SDP0 to input
  igb_readreg(mIgbDevice, CTRL_REG, &ctrl);
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Current CTRL value:", ctrl);
  ctrl &= ~0x400000; // set bit 22 -> SDP0 Input
  ctrl &= ~0x200000; // switch of watchdog indication
  igb_writereg(mIgbDevice, CTRL_REG, ctrl);
  igb_readreg(mIgbDevice, CTRL_REG, &ctrl);
  /**
    * @log CTRL Register
    */
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "CTRL value:", ctrl);

  //    1. Define the sampled SDP on AUX time ‘x’ (‘x’ = 0b or 1b) by setting the TSSDP.AUXx_SDP_SEL field
  //    while setting the matched TSSDP.AUXx_TS_SDP_EN bit to 1b.
  igb_readreg(mIgbDevice, TSSDP, &tssdp);
  /**
   * @log TSSDP Register
   */
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "TSSDP value:", tssdp);

  tssdp = 0;
  tssdp |= 0x20; // 100b -> AUX1 register to SDP0 input change
  igb_writereg(mIgbDevice, TSSDP, tssdp);
  igb_readreg(mIgbDevice, TSSDP, &tssdp);
  /**
   * @log TSSDP EN FLAG Register
   */
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "TSSDP_EN_FLAG value:", tssdp);


  //    2. Set also the TSAUXC.EN_TSx bit (‘x’ = 0b or 1b) to 1b to enable “time stamping”.
  tsauxc  = 0;
  tsauxc |= 0x8;    // auto-clear
  tsauxc |= 0x400;  // set EN_TS0 - use Target Time Register 1
  tsauxc |= 0x40;   // auto-clear
  igb_writereg(mIgbDevice, TSAUXC, tsauxc);
  igb_readreg(mIgbDevice, TSAUXC, &value);


  // Following a transition on the selected SDP, the hardware does the following:
  // 1. The SYSTIM registers (low and high) are latched to the selected AUXSTMP registers (low and high)
  // 2. The selected AUTT0 or AUTT1 flags are set in the TSICR register. If the AUTT interrupt is enabled by
  // the TSIM register and the 1588 interrupts are enabled by the Time_Sync flag in the ICR register
  // then an interrupt is asserted as well.
  // After the hardware reports that an event time was latched, the software should read the latched time in
  // the selected AUXSTMP registers. Software should read first the Low register and only then the High
  // register. Reading the high register releases the registers to sample a new event.

  while (!mEndThread)
  {
    // poll for level change on the SDP0 pin
    igb_readreg(mIgbDevice, TSAUXC, &value);

    if (value & 0x800u)
    {
      // read out AUXSTMPx register - so level change detection register is reset and enabled for the next capture
      igb_readreg(mIgbDevice, AUXSTMPL1, &auxstmpl);
      igb_readreg(mIgbDevice, AUXSTMPH1, &auxstmph);
      auxstmpNow = (int64_t(1000000000) * int64_t(auxstmph)) + int64_t(auxstmpl);

      igb_readreg(mIgbDevice, CTRL_REG, &ctrl);

      if (((ctrl ^ lastCtrl) & CTRL_SDP0_DATA) != CTRL_SDP0_DATA)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "Missed at least one edge! ctrl:", ctrl,
            "current:", auxstmpNow,
            "last", auxstmpLast);
      }
      lastCtrl = ctrl;

      // process only the rising edges
      if ((ctrl & CTRL_SDP0_DATA) != 0u)
      {
        // calculate clock period in seconds from elapsed time in ns since the last rising edge
        double periodSdp = 1e-9 * double(auxstmpNow - auxstmpLast);

        // try to estimate number of periods missed by rounding to the nearest integer
        double wrapCount = round(mNominal * periodSdp);;
        if (1.0 > wrapCount)
        {
          /* The only rounded value < 1.0 is 0. So no complete period has elapsed since the last edge.
           * This can only happen if we've catched a glitch, or the nominal frequency is configured
           * wrong. In either case, skip the handling of this edge and wait for the next one.
           */
          DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "no full period between edges");
        }
        else
        {
          if (1.0 != wrapCount)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "assumed number of periods to be:", wrapCount,
                "periodSdp:", periodSdp,
                "*mNominal:", mNominal * periodSdp);

            periodSdp = periodSdp / wrapCount;
          }

          // QUICKHACK: Assume 48000Hz sample rate
          const uint64_t events = uint64_t(wrapCount * 48e3/mNominal);

          if (0 == auxstmpLast)
          {
            // first run - init
            setEventCount(0u, auxstmpNow);
          }
          else
          {
            // update the rate ratio of the AvbClockDomain
            updateRateRatio(mNominal * periodSdp);

            // also update number of sample elapsed
            incrementEventCount(events, auxstmpNow);
          }
          auxstmpLast = auxstmpNow;

          cnt++;
          if (cntMax == cnt)
          {
            cnt = 0;

            // assume 48kHz
            const double freqAudioClk = 48e3 * mNominal * periodSdp;

            DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "freqAudioClk=", freqAudioClk, "Hz");

            DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "AUXSTMPHx=", auxstmph,
                "AUXSTMPLx=", auxstmpl,
                "diff=", diff,
                "ns periodSdp=", periodSdp,
                "evt=", events
                );

          }
        }
      }
    }

    sleep(mSleep, 0);
  } // while(!mEndThread)

  return IasResult::cOk;
}

#else // optional Pulse Per Second implementation
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  struct sched_param sparam;
  int32_t policy;
  uint32_t systimR     = 0;
  uint32_t systimL     = 0;
  uint32_t systimH     = 0;
  uint32_t targettimL  = 0;
  uint32_t targettimH  = 0;
  uint32_t targettimL1 = 0;
  uint32_t targettimH1 = 0;
  uint32_t value       = 0;
  uint32_t tssdp       = 0;
  uint32_t tsauxc      = 0;
  uint32_t ctrl        = 0;

  sparam.sched_priority = 0;
  int32_t errval = pthread_setschedparam( pthread_self(), SCHED_OTHER, &sparam );
  if(0 == errval)
  {
    errval = pthread_getschedparam( pthread_self(), &policy, &sparam );
    if(0 == errval)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Priority of ReaderhandlerThread actual set to prio:",
          sparam.__sched_priority);
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error getting sched prio:",
          strerror(errval));
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error setting sched prio:",
        strerror(errval));
  }

  while (!mEndThread)
  {
    // Step 1
    // Select a specific SDP pin by setting
    // the TSSDP.TS_SDPx_EN flag to 1b (while â€˜xâ€™ is 0, 1, 2 or 3).

    // Set to SDP0
    igb_readreg(mIgbDevice, TSSDP, &tssdp);
    tssdp  = 0;
    tssdp |= 0x100; // set bit 8 -> SDP0 Output
    igb_writereg(mIgbDevice, TSSDP, tssdp);
    igb_readreg(mIgbDevice, TSSDP, &tssdp);


    // Step 2
    // Select the target time register for the selected SDP that defines the beginning of the output pulse.
    // It is done by setting the TSSDP.TS_SDPx_SEL field to 00b or 01b if level change should occur when
    // SYSTIML/H equals TRGTTIML/H0 or TRGTTIML/H1, respectively.

    // Target Time 0 -> SDP0
    igb_readreg(mIgbDevice, TSSDP, &tssdp);
    tssdp &= ~0x40;
    igb_writereg(mIgbDevice, TSSDP, tssdp);
    igb_readreg(mIgbDevice, TSSDP, &tssdp);

    // Step 3
    // Define the selected SDPx pin as output, by setting the appropriate
    // SDPx_IODIR bit (while â€˜xâ€™ is 0, 1, 2, or 3) in the CTRL or CTRL_EXT registers.
    // set SDP0 to Output CTRL - bit 22 - SDP0_IODIR -TRUE

    // Set SDP0 to output
    igb_readreg(mIgbDevice, CTRL_REG, &ctrl);
    ctrl |= 0x400000; // set bit 22 -> SDP0 Output
    igb_writereg(mIgbDevice, CTRL_REG, ctrl);
    igb_readreg(mIgbDevice, CTRL_REG, &ctrl);


    // Step 4
    // Program the target time TRGTTIML/Hx (while â€˜xâ€™ is 0b or 1b) to the required event time. The
    // registers indicated by the TSSDP.TS_SDPx_SEL define the leading edge of the pulse and the other
    // ones define the trailing edge of the pulse.

    // Read current time - SYSTIM REGISTER
    igb_readreg(mIgbDevice, SYSTIMR, &systimR);
    igb_readreg(mIgbDevice, SYSTIML, &systimL);
    igb_readreg(mIgbDevice, SYSTIMH, &systimH);

    // Move point of start to the end of a second
    if (systimL < THRESHOLD)
    {
      sleep(THRESHOLD - systimL,0);
    }

    // Set TARGET TIME 0 REGISTER
    targettimL = 0;
    targettimH = systimH + 1;

    igb_writereg(mIgbDevice, TRGTTIML0, targettimL);
    igb_writereg(mIgbDevice, TRGTTIMH0, targettimH);

    igb_readreg(mIgbDevice, TRGTTIML0, &targettimL);
    igb_readreg(mIgbDevice, TRGTTIMH0, &targettimH);

    // Set TARGET TIME 1 REGISTER
    targettimL1 = PULSE_LENGTH;
    targettimH1 = systimH + 1;

    igb_writereg(mIgbDevice, TRGTTIML1, targettimL1);
    igb_writereg(mIgbDevice, TRGTTIMH1, targettimH1);

    igb_readreg(mIgbDevice, TRGTTIML1, &value);
    igb_readreg(mIgbDevice, TRGTTIMH1, &value);


    // Step 5
    // Program the TRGTTIML/Hx defined by the TSSDP.TS_SDPx_SEL to â€œStart of Pulseâ€� mode by setting
    // the TSAUXC.PLSG bit to 1b and TSAUXC.EN_TTx bit to 1b (while â€˜xâ€™ is 0b or 1b). The other
    // TRGTTIML/Hx register should be set to Level Change mode by setting the TSAUXC.PLSG bit to 0b
    // and TSAUXC.EN_TTx bit to 1b (while â€˜xâ€™ is 0b or 1b).
    tsauxc  = 0;
    tsauxc |= 0x1;     // set EN_TT0 - use Target Time Register 0
    tsauxc |= 0x2;     // set EN_TT0 - use Target Time Register 1 for end of pulse
    tsauxc |= 0x20000; // set bit 17 -> plsg -> start of pulse
    igb_writereg(mIgbDevice, TSAUXC, tsauxc);
    igb_readreg(mIgbDevice, TSAUXC, &value);

    // When the SYSTIML/H registers becomes equal or larger than the TRGTTIML/H registers that define
    // the beginning of the pulse, the selected SDP changes its level. Then, when the SYSTIML/H registers
    // becomes equal or larger than the other TRGTTIML/H registers (that define the trailing edge of the
    // pulse), the selected SDP changes its level back.
    sleep(0,1);

    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Pulse Per Second is running");

  } // while(!mEndThread)


  return IasResult::cOk;
}
#endif


IasResult IasAvbHwCaptureClockDomain::shutDown()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  mEndThread = true;
  return IasResult::cOk;
}


IasResult IasAvbHwCaptureClockDomain::afterRun()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  return IasResult::cOk;
}


} // namespace IasMediaTransportAvb


