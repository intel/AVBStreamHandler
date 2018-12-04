/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbStreamHandler.cpp
 * @brief   This is the implementation of the IasAvbStreamHandler class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbStreamHandler.hpp"

#include "media_transport/avb_streamhandler_api/IasAvbConfiguratorInterface.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
// TO BE REPLACED #include "core_libraries/btm/ias_dlt_btm.h"

#include "avb_streamhandler/IasAvbReceiveEngine.hpp"
#include "avb_streamhandler/IasAvbTransmitEngine.hpp"
#include "avb_streamhandler/IasAlsaEngine.hpp"

#ifdef ANDROID
/* Android specific code */
#include "media_transport/avb_streaming/IasStreamInterfaceStub.hpp"
#else
/* Default code */
#include "avb_streamhandler/IasVideoStreamInterface.hpp"
#include "avb_streamhandler/IasLocalVideoStream.hpp"
#endif

#include "avb_streamhandler/IasTestToneStream.hpp"
#include "avb_streamhandler/IasLocalAudioStream.hpp"
#include "avb_streamhandler/IasAvbSwClockDomain.hpp"
#include "avb_streamhandler/IasAvbPtpClockDomain.hpp"
#include "avb_streamhandler/IasAvbRawClockDomain.hpp"
#include "avb_streamhandler/IasAvbHwCaptureClockDomain.hpp"
#include "avb_streamhandler/IasAvbRxStreamClockDomain.hpp"
#include "avb_streamhandler/IasAvbClockController.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"


#include <iostream>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <dlfcn.h>

extern int32_t verbosity;

namespace IasMediaTransportAvb {

#define PTP_ACCESS_SLEEP 10000000 // in ns: 10 ms
#define PTP_GET_PORTSTATE_TIMEOUT_CNT 10000

static const std::string cClassName = "IasAvbStreamHandler::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

IasAvbStreamHandler::IasAvbStreamHandler(DltLogLevelType dltLogLevel)
  : mState(eIasDead)
  , mAvbReceiveEngine(NULL)
  , mAvbTransmitEngine(NULL)
  , mAlsaEngine(NULL)
  , mVideoStreamInterface(NULL)
  , mTestToneStreams()
  , mEnvironment(NULL)
  , mClient(NULL)
  , mAvbClockDomains()
  , mNextLocalStreamId(1u)
  , mNextClockDomainId(cRxClockDomainIdStart)
  , mClockControllers()
  , mLog(NULL)
  , mDltLogLevel(dltLogLevel)
  , mConfigPluginHandle(NULL)
  , mPreConfigurationInProgress(false)
  , mBTMEnable(false)
  , mApiMutexEnable(false)
  , mApiMutexEnableConfig(true)
{

  mLog = new DltContext();
  // register context for DLT
  if (DLT_LOG_DEFAULT == mDltLogLevel)
  {
    DLT_REGISTER_CONTEXT(*mLog, "_ASH", "AVB Streamhandler");
  }
  else
  {
    DLT_REGISTER_CONTEXT_LL_TS(*mLog, "_ASH", "AVB Streamhandler", mDltLogLevel, DLT_TRACE_STATUS_OFF);
  }

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}


IasAvbStreamHandler::~IasAvbStreamHandler()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  cleanup();

  DLT_UNREGISTER_CONTEXT(*mLog);
  delete mLog;
  mLog = NULL;
}


IasAvbProcessingResult IasAvbStreamHandler::init( const std::string& configName, bool runSetup, int32_t setupArgc, char** setupArgv, const char* argv0 )
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cBootTimeMeasurement, mBTMEnable);

  if (isInitialized())
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Already initialized!");
    result = eIasAvbProcAlreadyInUse;
  }
  else
  {
    typedef IasAvbConfiguratorInterface& (*InterfaceFunctionGetter)();
    InterfaceFunctionGetter getter = NULL;

    if (configName.empty())
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "no config plugin file name specified");
      result = eIasAvbProcInvalidParam;
    }
    else
    {
      AVB_ASSERT(mConfigPluginHandle == NULL);
      mConfigPluginHandle = ::dlopen(configName.c_str(), RTLD_LAZY);
      if (!mConfigPluginHandle)
      {
        // config plugin could not be loaded, try loading from same path as executable
        std::string path = argv0;
        path = path.substr(0, path.rfind("/"));

        mConfigPluginHandle = ::dlopen(std::string(path + "/" + configName).c_str(), RTLD_LAZY);
        if (!mConfigPluginHandle)
        {
          const char *errorMsg = ::dlerror();
          AVB_ASSERT(errorMsg != NULL);
          DLT_LOG_CXX(*mLog, DLT_LOG_FATAL, LOG_PREFIX, " Error loading configuration plugin =", errorMsg, "!\n");
          result = eIasAvbProcInitializationFailed;
        }
      }
      if (mConfigPluginHandle)
      {
        // C-style cast is about the only form that allows void*-to-function pointer
        getter = (InterfaceFunctionGetter) ::dlsym( mConfigPluginHandle, "getIasAvbConfiguratorInterfaceInstance");
        const char *lastErr = ::dlerror();
        if (NULL != lastErr)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, configName.c_str(),
              "does not seem to be a valid configuration plugin (",
              lastErr, ")");

          (void) ::dlclose(mConfigPluginHandle);
          result = eIasAvbProcInitializationFailed;
        }
      }
    }

    if (eIasAvbProcOK == result)
    {
      AVB_ASSERT(NULL != getter);
      IasAvbConfiguratorInterface & config = getter();

      mEnvironment = new (nothrow) IasAvbStreamHandlerEnvironment(mDltLogLevel);

      if (NULL == mEnvironment)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to allocate IasAvbStreamHandlerEnvironment!");
        result = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        mEnvironment->setDefaultConfigValues();

        if ((setupArgc > 0) && (NULL != setupArgv))
        {
          if (!config.passArguments( setupArgc, setupArgv, verbosity, *mEnvironment ))
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Configuration library failed to parse arguments!");
            result = eIasAvbProcInitializationFailed;
          }
        }

        if (!mEnvironment->validateRegistryEntries())
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "ValidateRegistryEntries failed!");
          result = eIasAvbProcInitializationFailed;
        }
      }
      if (eIasAvbProcOK == result)
      {
        (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cApiMutex, mApiMutexEnableConfig);

        if (mBTMEnable)
        {
          // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb created registry",NULL);
        }
        std::string logLevelKey = IasRegKeys::cDebugLogLevelPrefix;
        logLevelKey += "_ash";
        int32_t logLevel = mDltLogLevel;
        if (IasAvbStreamHandlerEnvironment::getConfigValue(logLevelKey, logLevel) && logLevel != mDltLogLevel)
        {
          DLT_UNREGISTER_CONTEXT(*mLog);
          if (DLT_LOG_DEFAULT == logLevel)
          {
            DLT_REGISTER_CONTEXT(*mLog, "_ASH", "AVB Streamhandler");
          }
          else
          {
            DLT_REGISTER_CONTEXT_LL_TS(*mLog, "_ASH", "AVB Streamhandler", logLevel, DLT_TRACE_STATUS_OFF);
          }
        }
        result = mEnvironment->registerDltContexts();
      }

      if (eIasAvbProcOK == result)
      {
        IasAvbTSpec::initTables();
      }

      if (eIasAvbProcOK == result)
      {
        std::string driverFileName;

        if (IasAvbStreamHandlerEnvironment::getConfigValue( IasRegKeys::cClockDriverFileName, driverFileName ))
        {
          result = mEnvironment->loadClockDriver( driverFileName );

          if (eIasAvbProcOK == result)
          {
            IasAvbClockDriverInterface* driver = IasAvbStreamHandlerEnvironment::getClockDriver();

            AVB_ASSERT(NULL != driver);
            if (IasAvbResult::eIasAvbResultOk != driver->init(*mEnvironment))
            {
              result = eIasAvbProcInitializationFailed;
            }
          }
        }
      }
      if (eIasAvbProcOK == result)
      {
        AVB_ASSERT( NULL != mEnvironment );

        if (mEnvironment->setTxRingSize() != eIasAvbProcOK)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Setting TX ring size failed!");
          result = eIasAvbProcInitializationFailed;
        }
      }

      if (eIasAvbProcOK == result)
      {
        AVB_ASSERT( NULL != mEnvironment );

        if (mEnvironment->createIgbDevice() != eIasAvbProcOK)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Init of igb_avb device failed");
          result = eIasAvbProcInitializationFailed;
        }
      }
      if (eIasAvbProcOK == result)
      {
        if (mBTMEnable)
        {
          // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb created igb_avb",NULL);
        }
        AVB_ASSERT( NULL != mEnvironment );
        if (mEnvironment->createPtpProxy() != eIasAvbProcOK)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Init of PTP daemon lib failed!");
          result = eIasAvbProcInitializationFailed;
        }
        else
        {
          uint32_t loopCount = 0; // ptp check is disabled as default
          uint64_t loopSleep = 10000000; // in ns: 10 ms
          IasLibPtpDaemon* const ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cPtpLoopCount, loopCount);
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cPtpLoopSleep, loopSleep);

          if (NULL != ptp)
          {
            if (0 != loopCount)
            {
              if (!IasAvbStreamHandlerEnvironment::isLinkUp())
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " link not ready - deferring PTP sync wait");
              }
              else
              {
                uint32_t errCount = 0;

                // Retry until ptp port state is ready.
                if (mBTMEnable)  // this is neccessary in the loop, otherwise BTM will quit when PTP startup is slow
                {
                  // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb Link up wait for PTP",NULL);
                }
                while ((loopCount > errCount))
                {
                  if(ptp->isPtpReady())
                  {
                    break;
                  }
                  else
                  {
                    sleep_ns(static_cast<uint32_t>(loopSleep));
                    errCount ++;
                  }
                }

                if (loopCount <= errCount)
                {
                  DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, " PTP daemon not ready after",
                      static_cast<uint32_t>(errCount * (loopSleep / 1000000u)), "ms");
                }
                else
                {
                  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " PTP daemon ready after",
                      static_cast<uint32_t>(errCount * (loopSleep / 1000000u)), "ms");
                }
                if (mBTMEnable)
                {
                  // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb ptp port state ready",NULL);
                }
              }
            }
            else
            {
              // initial call so gptp process id is stored to get access to persistency storage data
              ptp->isPtpReady();
            }
          }
        }
      }
      if (eIasAvbProcOK == result)
      {
        if (mBTMEnable)
        {
          // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb created ptp proxy",NULL);
        }
        AVB_ASSERT( NULL != mEnvironment );

        /* The MAC address must be valid here as createIgbDevice() called earlier during the initialization and would
         * have thrown an error making result != eIasAvbProcOk if that was not the case.
         */
        const IasAvbMacAddress * pmac = mEnvironment->getSourceMac();
        AVB_ASSERT( NULL != pmac );
        const IasAvbMacAddress & mac = *pmac;

        std::stringstream dbgStringStream;
        dbgStringStream << " local MAC address = " << std::setfill('0') << std::hex << std::setw(2) << int(mac[0])
            << ":" << std::setw(2) << int(mac[1]) << ":" << std::setw(2) << int(mac[2]) << ":"
            << std::setw(2) << int(mac[3]) << ":" << std::setw(2) << int(mac[4]) << ":" << std::setw(2) << int(mac[5]);
        DLT_LOG_CXX(*mLog,  DLT_LOG_INFO, LOG_PREFIX, dbgStringStream.str());

        if (!mEnvironment->isLinkUp())
        {
          DLT_LOG_CXX(*mLog,  DLT_LOG_WARN, LOG_PREFIX, " Network interface down!");
        }
      }

      if (eIasAvbProcOK == result)
      {
        IasAvbPtpClockDomain * newAvbClockDomain = new (nothrow) IasAvbPtpClockDomain();
        if (NULL == newAvbClockDomain)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to instantiate IasAvbPtpClockDomain!");
          result = eIasAvbProcInitializationFailed;
        }
        else
        {
          newAvbClockDomain->setClockDomainId(cIasAvbPtpClockDomainId);
          mAvbClockDomains[cIasAvbPtpClockDomainId] = newAvbClockDomain;
        }
      }

      if (eIasAvbProcOK == result)
      {
        IasAvbRawClockDomain * newAvbClockDomain = new (nothrow) IasAvbRawClockDomain();
        if (NULL == newAvbClockDomain)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to instantiate IasAvbRawClockDomain!");
          result = eIasAvbProcInitializationFailed;
        }
        else
        {
          newAvbClockDomain->setClockDomainId(cIasAvbRawClockDomainId);
          mAvbClockDomains[cIasAvbRawClockDomainId] = newAvbClockDomain;
        }
      }

      if (eIasAvbProcOK == result)
      {
        uint64_t val = 0u;
        (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockHwCapFrequency, val);
        if (0u != val)
        {
          IasAvbHwCaptureClockDomain * newAvbClockDomain = new (nothrow) IasAvbHwCaptureClockDomain();
          if (NULL == newAvbClockDomain)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to instantiate IasAvbHwCaptureClockDomain!");
            result = eIasAvbProcInitializationFailed;
          }
          else
          {
            result = newAvbClockDomain->init();

            if (eIasAvbProcOK != result)
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Failed to initialize IasAvbHwCaptureClockDomain!");
              result = eIasAvbProcInitializationFailed;
            }
            else
            {
              newAvbClockDomain->setClockDomainId(cIasAvbHwCaptureClockDomainId);
              mAvbClockDomains[cIasAvbHwCaptureClockDomainId] = newAvbClockDomain;
            }
          }
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " IasAvbHwCaptureClockDomain is disabled!");
        }
      }

      if (eIasAvbProcOK == result)
      {
        // TODO: Add a config option to receive user input to disable avb_watchdog at runtime.
        //       Otherwise, would not be able to start by executing the demo_app normally
        //       since WATCHDOG_USEC is set by systemctl.
        result = mEnvironment->createWatchdog();
        if (result == eIasAvbProcInvalidParam)
        {
          uint64_t wd_en = 0;
          DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX,
                      "WATCHDOG_USEC is not configured!");

          mEnvironment->getConfigValue(IasRegKeys::cUseWatchdog, wd_en);
          if (wd_en)
              DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX,
                          "Watchdog won't work. Please configure WATCHDOG_USEC.");

          result = eIasAvbProcOK;
        }
        else if (result != eIasAvbProcOK)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Init of watchdog failed");
          result = eIasAvbProcInitializationFailed;
        }

      }

      if (eIasAvbProcOK == result)
      {
        mState = eIasInitialized;
      }

      if ((eIasAvbProcOK == result) && runSetup)
      {
        // Set this flag to indicate that pre-configuration by configuration library is in progress. This means that
        // streams created during this preconfiguration step will have its preconfigured flag set to 'true' and streams
        // created afterwards will have its preconfigured flag set to 'false'.
        mPreConfigurationInProgress = true;
        if (!config.setup(this))
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " config.setup failed!");
          result = eIasAvbProcInitializationFailed;
        }
        mPreConfigurationInProgress = false;
      }
    }

    if (eIasAvbProcOK != result)
    {
      cleanup();
    }
    else
    {
      if (mBTMEnable)
      {
        // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb started streamhandler",NULL);
      }
    }
  }


  return result;
}


void IasAvbStreamHandler::cleanup()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (NULL != mVideoStreamInterface)
  {
    (void) mVideoStreamInterface->stop();
  }

  (void) stop(false);

  // remove and delete clock controllers
  for (AvbClockControllers::iterator it = mClockControllers.begin(); it != mClockControllers.end(); it++)
  {
    delete *it;
  }
  mClockControllers.clear();

  IasAvbClockDriverInterface* driver = IasAvbStreamHandlerEnvironment::getClockDriver();
  if (NULL != driver)
  {
    driver->cleanup();
  }

  /*
   *  first remove transmit and receive engine, since they need to
   *  disconnect from resources provided.
   */

  // remove transmit engine
  delete mAvbTransmitEngine;
  mAvbTransmitEngine = NULL;

  delete mAvbReceiveEngine;
  mAvbReceiveEngine = NULL;

  delete mAlsaEngine;
  mAlsaEngine = NULL;

  delete mVideoStreamInterface;
  mVideoStreamInterface = NULL;

  for (TestToneStreamMap::iterator it = mTestToneStreams.begin(); it != mTestToneStreams.end(); it++)
  {
    AVB_ASSERT(it->second);
    delete it->second;
  }
  mTestToneStreams.clear();

  // remove and delete clock domains
  for (AvbClockDomains::iterator it = mAvbClockDomains.begin(); it != mAvbClockDomains.end(); it++)
  {
    AVB_ASSERT(it->second);
    delete it->second;
  }
  mAvbClockDomains.clear();

  // unregister DLT contexts
  if (NULL != mEnvironment)
  {
    (void) mEnvironment->unregisterDltContexts();
  }

  // remove environment
  delete mEnvironment;
  mEnvironment = NULL;

  // remove client
  mClient = NULL;

  mState = eIasDead;

  if (NULL != mConfigPluginHandle)
  {
    (void) ::dlclose(mConfigPluginHandle);
    mConfigPluginHandle = NULL;
  }
}


void IasAvbStreamHandler::lockApiMutex()
{
  if (true == mApiMutexEnable)
  {
    mApiMtx.lock();
  }
}


void IasAvbStreamHandler::unlockApiMutex()
{
  if (true == mApiMutexEnable)
  {
    mApiMtx.unlock();
  }
}


void IasAvbStreamHandler::activateMutexHandling()
{
  // switch API mutex off if set so via command line config
  if(false == mApiMutexEnableConfig)
  {
    mApiMtx.lock();
    mApiMutexEnable = mApiMutexEnableConfig;
    mApiMtx.unlock();
    DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "API mutex disabled by command line");
  }
  else
  {
    // enable API mutex per default
    mApiMtx.lock();
    mApiMutexEnable = true;
    mApiMtx.unlock();
  }
}


IasAvbResult IasAvbStreamHandler::registerClient( IasAvbStreamHandlerClientInterface * client )
{
  IasAvbResult ret = IasAvbResult::eIasAvbResultOk;

  if (!isInitialized())
  {
    ret = IasAvbResult::eIasAvbResultErr; // bad state
  }
  else if (NULL == client)
  {
    ret = IasAvbResult::eIasAvbResultErr; // invalid param
  }
  else if (NULL != mClient)
  {
    ret = IasAvbResult::eIasAvbResultErr; // number of clients exceeded
  }
  else
  {
    mClient = client;
  }

  return ret;
}


IasAvbResult IasAvbStreamHandler::unregisterClient( IasAvbStreamHandlerClientInterface * client )
{
  IasAvbResult ret = IasAvbResult::eIasAvbResultOk;

  if (!isInitialized())
  {
    ret = IasAvbResult::eIasAvbResultErr; // bad state
  }
  else if ((NULL == client) || (mClient != client))
  {
    ret = IasAvbResult::eIasAvbResultErr; // invalid param
  }
  else
  {
    mClient = NULL;
  }

  return ret;
}


IasAvbResult IasAvbStreamHandler::createReceiveAudioStream(IasAvbSrClass srClass, uint16_t maxNumberChannels,
                                                           uint32_t sampleFreq, AvbStreamId streamId,
                                                           MacAddress destMacAddr)
{

  IasAvbProcessingResult result = eIasAvbProcOK;
  IasAvbStreamId avbStreamId(streamId);
  IasAvbAudioFormat const format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;

  lockApiMutex();

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (maxNumberChannels > cIasAvbMaxNumChannels)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " maxNumberChannels (",
              uint16_t(maxNumberChannels), " > max number of channels allowed (",
              uint16_t(cIasAvbMaxNumChannels), ")");
      result = eIasAvbProcErr;
    }
  }

  if (eIasAvbProcOK == result)
  {
    if (NULL == mAvbReceiveEngine)
    {
      // if transmit engine is not already available, create and initialize it
      mAvbReceiveEngine = new (nothrow) IasAvbReceiveEngine();
      if (NULL == mAvbReceiveEngine)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to instantiate ReceiveEngine!");
        result = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        result = mAvbReceiveEngine->init();
        if (eIasAvbProcOK != result)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Failed to initialize ReceiveEngine! Result=", int32_t(result));
        }
        else
        {
          result = mAvbReceiveEngine->registerEventInterface(this);
          if (eIasAvbProcOK != result)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Failed to register at ReceiveEngine!", int32_t(result));
          }
        }
        if ((eIasAvbProcOK == result) && isStarted())
        {
          result = mAvbReceiveEngine->start();
        }
      }
    }

    if (eIasAvbProcOK != result)
    {
      delete mAvbReceiveEngine;
      mAvbReceiveEngine = NULL;
    }
  }

  if (eIasAvbProcOK == result)
  {
    IasAvbMacAddress mac;
    for (uint32_t i = 0u; i < cIasAvbMacAddressLength; i++)
    {
      mac[i] = uint8_t(destMacAddr >> ((cIasAvbMacAddressLength - i - 1) * 8));
    }
    result = mAvbReceiveEngine->createReceiveAudioStream(srClass, maxNumberChannels, sampleFreq, format, avbStreamId,
                                                         mac, mPreConfigurationInProgress);
  }

  if (eIasAvbProcOK == result)
  {
    uint64_t streamIdMcr = 0;
    if (mEnvironment->queryConfigValue(IasRegKeys::cClkRecoverFrom, streamIdMcr))
    {
      if (streamIdMcr == streamId)
      {
        uint32_t rxClockId = 0u;
        IasAvbResult res = deriveClockDomainFromRxStream(streamId, rxClockId);
        if (IasAvbResult::eIasAvbResultOk == res)
        {
          uint64_t slaveClockId = cIasAvbHwCaptureClockDomainId;
          mEnvironment->queryConfigValue(IasRegKeys::cClkRecoverUsing, slaveClockId);
          res = setClockRecoveryParams(rxClockId, static_cast<uint32_t>(slaveClockId), 0u);
        }

        result = (IasAvbResult::eIasAvbResultOk == res) ? eIasAvbProcOK : eIasAvbProcErr;
      }
    }
  }

  if (eIasAvbProcOK == result)
  {
    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << streamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " AVB receive stream", ssStreamId.str() , "created");
  }

  unlockApiMutex();

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::createTransmitAudioStream(IasAvbSrClass srClass, uint16_t maxNumberChannels, uint32_t sampleFreq,
    IasAvbAudioFormat format, uint32_t clockId, IasAvbIdAssignMode assignMode, AvbStreamId & streamId,
    MacAddress & destMacAddr, bool active)
{
  IasAvbProcessingResult result = eIasAvbProcOK;
  IasAvbStreamId avbStreamId(streamId);

  lockApiMutex();

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (maxNumberChannels > cIasAvbMaxNumChannels)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " maxNumberChannels (",
              uint16_t(maxNumberChannels), " > max number of channels allowed (",
              uint16_t(cIasAvbMaxNumChannels), ")");
      result = eIasAvbProcErr;
    }
  }

  if (eIasAvbProcOK == result)
  {
    if (NULL == mAvbTransmitEngine)
    {
      result = createTransmitEngine();
    }
  }

  if (eIasAvbProcOK == result)
  {
    AVB_ASSERT(NULL != mAvbTransmitEngine);
    IasAvbClockDomain * const clockDomain = getClockDomainById( clockId );
    if (NULL == clockDomain)
    {
      /**
       * @log Invalid param: The clockId parameter is not present in the Clock Domain map.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Clock Domain not found!!");
      result = eIasAvbProcInvalidParam;
    }

    if (eIasAvbProcOK == result)
    {
      // set streamId and DMAC depending on assign mode
      // at the moment only static mode is supported!
      if (IasAvbIdAssignMode::eIasAvbIdAssignModeStatic == assignMode)
      {
        IasAvbMacAddress mac;
        for (uint32_t i = 0u; i < cIasAvbMacAddressLength; i++)
        {
          mac[i] = uint8_t( destMacAddr >> ((cIasAvbMacAddressLength-i-1) * 8) );
        }
        result = mAvbTransmitEngine->createTransmitAudioStream(srClass, maxNumberChannels, sampleFreq, format,
            clockDomain, avbStreamId, mac, mPreConfigurationInProgress);
      }
      else
      {
        /**
         * @log Not implemented: Attempt to use an assign mode that is currently unimplemented.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Assign mode other than 'eIasAvbIdAssignModeStatic' not implemented!");
        result = eIasAvbProcNotImplemented;
      }
    }
  }

  if ((eIasAvbProcOK == result) && (active))
  {
    result = mAvbTransmitEngine->activateAvbStream(avbStreamId);

    if (eIasAvbProcOK != result)
    {
      // stream has been created but couldn't been activated, so remove it
      (void) mAvbTransmitEngine->destroyAvbStream(avbStreamId);
    }
  }

  if (eIasAvbProcOK == result)
  {
    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << streamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " AVB transmit audio stream", ssStreamId.str(),
                "created", (active ? "(active)" : "(decative)"));
  }

  unlockApiMutex();

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::destroyStream(AvbStreamId streamId)
{
  IasAvbProcessingResult result = eIasAvbProcNotInitialized;
  IasAvbStreamId avbStreamId(streamId);

  lockApiMutex();

  if (isInitialized() && NULL != mAvbTransmitEngine)
  {
    result = mAvbTransmitEngine->destroyAvbStream(avbStreamId);
  }

  if(eIasAvbProcOK != result )
  {
    if (isInitialized() && NULL != mAvbReceiveEngine)
    {
      result = mAvbReceiveEngine->destroyAvbStream(avbStreamId);
    }
  }

  if(eIasAvbProcOK == result )
  {
    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << streamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " AVB stream", ssStreamId.str() , "destroyed");
  }

  unlockApiMutex();

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::createAlsaStream(IasAvbStreamDirection direction, uint16_t numberOfChannels,
    uint32_t sampleFreq, IasAvbAudioFormat format, uint32_t clockId, uint32_t periodSize, uint32_t numPeriods, uint8_t channelLayout,
    bool hasSideChannel, std::string const &deviceName, uint16_t &streamId, IasAlsaDeviceTypes alsaDeviceType, uint32_t sampleFreqASRC)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  lockApiMutex();

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (numberOfChannels > cIasAvbMaxNumChannels)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " numberOfChannels (",
              uint16_t(numberOfChannels), " > max number of channels allowed (",
              uint16_t(cIasAvbMaxNumChannels), ")");
      result = eIasAvbProcErr;
    }
  }

  if (eIasAvbProcOK == result)
  {
    if (NULL == mAlsaEngine)
    {
      mAlsaEngine =  new (nothrow) IasAlsaEngine();

      if (NULL != mAlsaEngine)
      {
        result = mAlsaEngine->init();
        if (eIasAvbProcOK != result)
        {
          /**
           * @log Init failed: Could not initialize Alsa Engine, already initialized.
           */
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Failed to init Alsa Engine!");
        }
      }
      else
      {
        /**
         * @log Not enough memory: Couldn't create a new AlsaEngine.
         */
        DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to instantiate Alsa Engine!");
        result = eIasAvbProcNotEnoughMemory;
      }

      if ((eIasAvbProcOK == result) && isStarted())
      {
        result = mAlsaEngine->start();
      }

      if (eIasAvbProcOK != result)
      {
        delete mAlsaEngine;
        mAlsaEngine = NULL;
      }
    }
  }

  if (eIasAvbProcOK == result)
  {
    if(0 != streamId)
    {
      if ((isLocalStreamIdInUse(streamId)))
      {
        result = eIasAvbProcInvalidParam;
      }
    }
    else
    {
      streamId = getNextLocalStreamId();
    }

    if (eIasAvbProcOK == result)
    {
      IasAvbClockDomain * const clockDomain = getClockDomainById(clockId);
      if (NULL == clockDomain)
      {
        /**
         * @log Invalid param: The clockId parameter is not present in the Clock Domain map.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Clock Domain not found!!");
        result = eIasAvbProcInvalidParam;
      }
      else
      {
        result = mAlsaEngine->createAlsaStream( direction, numberOfChannels, sampleFreq, format, periodSize,
          numPeriods, channelLayout, hasSideChannel,deviceName, streamId, clockDomain, alsaDeviceType, sampleFreqASRC);
        if (mBTMEnable)
        {
          // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb created ALSA stream",NULL);
        }
      }
    }
  }

  if (eIasAvbProcOK != result)
  {
    streamId = 0u;
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Alsa stream", streamId , "created");
  }

  unlockApiMutex();

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::createTestToneStream(uint16_t numberOfChannels,
    uint32_t sampleFreq, IasAvbAudioFormat format, uint8_t channelLayout, uint16_t &streamId )
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  lockApiMutex();

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (0 != streamId)
    {
      if (NULL != getLocalAudioStreamById(streamId))
      {
        result = eIasAvbProcInvalidParam;
      }
      else if (IasAvbAudioFormat::eIasAvbAudioFormatSafFloat != format)
      {
        result = eIasAvbProcUnsupportedFormat;
      }
    }
    else
    {
      streamId = getNextLocalStreamId();
    }

    if (eIasAvbProcOK == result)
    {
      IasTestToneStream *stream = new (nothrow) IasTestToneStream(*mLog, streamId);

      if (NULL == stream)
      {
        /**
         * @log Not enough memory: Couldn't create a new TestToneStream.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to create test tone stream!");
        result = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        result = stream->init(numberOfChannels, sampleFreq, channelLayout);
        if (eIasAvbProcOK != result)
        {
          /**
           * @log Could not init a LocalAudioStream with the provided params.
           */
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Failed to init the test tone stream!");
          delete stream;
        }
        else
        {
          mTestToneStreams[streamId] = stream;
        }
      }
    }
  }

  if (eIasAvbProcOK != result)
  {
    streamId = 0u;
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Test tone stream", streamId , "created");
  }

  unlockApiMutex();

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::setStreamActive(AvbStreamId streamId, bool active)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  lockApiMutex();

  if (!isInitialized() || (NULL == mAvbTransmitEngine))
  {
    // not initialized or no TX streams created
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    IasAvbStreamId stream( streamId );

    if (active)
    {
      result = mAvbTransmitEngine->activateAvbStream( stream );
    }
    else
    {
      result = mAvbTransmitEngine->deactivateAvbStream( stream );
    }
  }

  if (eIasAvbProcOK == result)
  {
    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << streamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " AVB stream", ssStreamId.str(), "set", (active ? "active" : "decative"));
  }

  unlockApiMutex();

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::destroyLocalStream(uint16_t streamId)
{
  IasAvbProcessingResult result = eIasAvbProcNotInitialized;

  lockApiMutex();

  if (isInitialized())
  {
    result = eIasAvbProcErr;

    IasLocalAudioStream * localAudioStream = getLocalAudioStreamById(streamId);
    IasLocalVideoStream * localVideoStream = getLocalVideoStreamById(streamId);

    if (NULL != localAudioStream)
    {
      const IasLocalStreamType localStreamType = localAudioStream->getType();
      if ((NULL != mAlsaEngine) && (eIasAlsaStream == localStreamType))
      {
        result = mAlsaEngine->destroyAlsaStream(streamId, false);
      }
      else // Can really only be a TestToneStream. Check if it's present in the map, otherwise log and error.
      {
        TestToneStreamMap::const_iterator it = mTestToneStreams.find(streamId);
        if (it != mTestToneStreams.end())
        {
          AVB_ASSERT(NULL != it->second);
          IasTestToneStream * stream = it->second;
          if (!stream->isConnected())
          {
            mTestToneStreams.erase(it); // Invalidates iterator
            delete stream;
            result = eIasAvbProcOK;
            DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Local stream", streamId , "destroyed");
          }
          else
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " stream still connected");
            result = eIasAvbProcAlreadyInUse;
          }
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX,
                  " could not find any local alsa or test tone audio stream with id ",
                  streamId);

          result = eIasAvbProcInvalidParam;
        }
      }
    }
    else if(NULL != localVideoStream)
    {
      const IasLocalStreamType localStreamType = localVideoStream->getType();

      if ((NULL != mVideoStreamInterface)
          && ((eIasLocalVideoInStream == localStreamType) || (eIasLocalVideoOutStream == localStreamType)))
      {
        result = mVideoStreamInterface->destroyVideoStream(streamId);
      }
    }
    else
    {
      result = eIasAvbProcInvalidParam;
    }
  }

  if (eIasAvbProcOK == result)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Local stream", streamId, "destroyed");
  }

  unlockApiMutex();

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::connectStreams(AvbStreamId networkStreamId, uint16_t localStreamId)
{
  IasAvbProcessingResult result = eIasAvbProcInvalidParam;

  lockApiMutex();

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    IasLocalAudioStream * localAudioStream = getLocalAudioStreamById(localStreamId);
    IasLocalVideoStream * localVideoStream = getLocalVideoStreamById(localStreamId);

    if (NULL == localAudioStream && NULL == localVideoStream)
    {
      /**
       * @log Invalid param: Local stream Id parameter cannot be found in the audio or video stream lists.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Invalid local stream ID! ID=", localStreamId);
      result = eIasAvbProcInvalidParam;
    }
    else
    {
      if (NULL != localAudioStream)
      {
        IasAvbStreamId nwStreamId(networkStreamId);
        if (localAudioStream->getDirection() == IasAvbStreamDirection::eIasAvbTransmitToNetwork)
        {
          if (NULL != mAvbTransmitEngine)
          {
            result = mAvbTransmitEngine->connectAudioStreams(nwStreamId, localAudioStream);
          }
        }
        else
        {
          if (NULL != mAvbReceiveEngine)
          {
            result = mAvbReceiveEngine->connectAudioStreams(nwStreamId, localAudioStream);
          }
        }
      }

      if (NULL != localVideoStream)
      {
        IasAvbStreamId nwStreamId(networkStreamId);
        if (localVideoStream->getDirection() == IasAvbStreamDirection::eIasAvbTransmitToNetwork)
        {
          if (NULL != mAvbTransmitEngine)
          {
            result = mAvbTransmitEngine->connectVideoStreams(nwStreamId, localVideoStream);
          }
        }
        else
        {
          if (NULL != mAvbReceiveEngine)
          {
            result = mAvbReceiveEngine->connectVideoStreams(nwStreamId, localVideoStream);
          }
        }
      }

    }
  }

  if (eIasAvbProcOK != result)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Couldn't connect streams ID: ", localStreamId , " ErrorCode", int32_t(result));
  }
  else
  {
    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << networkStreamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Streams connected:", ssStreamId.str() , "-", localStreamId);
  }

  unlockApiMutex();

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::disconnectStreams(AvbStreamId networkStreamId)
{
  IasAvbProcessingResult result = eIasAvbProcNotInitialized;
  IasAvbStreamId avbStreamId(networkStreamId);

  lockApiMutex();

  if (isInitialized() && NULL != mAvbTransmitEngine)
  {
    if (mAvbTransmitEngine->isValidStreamId(avbStreamId))
    {
      result = mAvbTransmitEngine->disconnectStreams(avbStreamId);
    }
  }

  if(eIasAvbProcOK != result )
  {
    if (isInitialized() && NULL != mAvbReceiveEngine)
    {
      if (mAvbReceiveEngine->isValidStreamId(avbStreamId))
      {
        result = mAvbReceiveEngine->disconnectStreams(avbStreamId);
      }
    }
  }

  if (eIasAvbProcOK == result)
  {
    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << networkStreamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " AVB stream", ssStreamId.str() , "disconnected");
  }

  unlockApiMutex();

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::setChannelLayout(uint16_t localStreamId, uint8_t channelLayout)
{
  IasAvbResult result = IasAvbResult::eIasAvbResultInvalidParam;

  lockApiMutex();

  if (!isInitialized())
  {
    result = IasAvbResult::eIasAvbResultErr;
  }
  else
  {
    IasLocalAudioStream *stream = getLocalAudioStreamById(localStreamId);
    if (NULL != stream)
    {
      result = mapResultCode(stream->setChannelLayout(channelLayout));
    }
  }

  unlockApiMutex();

  return result;
}


IasAvbResult IasAvbStreamHandler::setTestToneParams(uint16_t localStreamId, uint16_t channel, uint32_t signalFrequency,
    int32_t level, IasAvbTestToneMode mode, int32_t userParam )
{
  IasAvbResult result = IasAvbResult::eIasAvbResultInvalidParam;

  if (!isInitialized())
  {
    result = IasAvbResult::eIasAvbResultErr;
  }
  else
  {

    IasLocalAudioStream *stream = getLocalAudioStreamById(localStreamId);
    if ((NULL != stream) && (eIasTestToneStream == stream->getType()))
    {
      lockApiMutex();

      result = mapResultCode(static_cast<IasTestToneStream*>(stream)->setChannelParams(channel, signalFrequency,
          level, mode, userParam));

      unlockApiMutex();
    }
  }

  return result;
}


IasAvbResult IasAvbStreamHandler::deriveClockDomainFromRxStream(AvbStreamId rxStreamId, uint32_t & clockId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (0u != clockId)
    {
      if (mAvbClockDomains.end() != mAvbClockDomains.find(clockId))
      {
       DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " specified clock id already in use: ", clockId);
        result = eIasAvbProcInvalidParam;
      }
    }
    else
    {
      while((mAvbClockDomains.end() != mAvbClockDomains.find(mNextClockDomainId)) && (mNextClockDomainId != 0u))
      {
        mNextClockDomainId++;
      }
      if (0u == mNextClockDomainId)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Internal error: ran out of clock id values");
        result = eIasAvbProcErr;
      }
      clockId = mNextClockDomainId;
    }

    if (eIasAvbProcOK == result)
    {
      IasAvbStream *stream = NULL;
      const IasAvbStreamId id(rxStreamId);

      if (NULL != mAvbReceiveEngine)
      {
        stream = mAvbReceiveEngine->getStreamById(id);
      }

      if (NULL == stream)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " specified stream id not found in RX streams", clockId);
         result = eIasAvbProcInvalidParam;
      }
      else
      {
        // create new clock domain object
        IasAvbRxStreamClockDomain *domain = new (nothrow) IasAvbRxStreamClockDomain;
        if (NULL == domain)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to create new clock domain!");
          result = eIasAvbProcNotEnoughMemory;
        }
        else
        {
          result = stream->hookClockDomain( domain );
        }

        if (eIasAvbProcOK != result)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Couldn't assign clock domain to streams! Result=",
              int32_t(result));
          delete domain;
        }
        else
        {
          domain->setClockDomainId(clockId);
          mAvbClockDomains[clockId] = domain;

          std::stringstream dbgStringStream;
          dbgStringStream << " Assigned clock id 0x" << std::hex << clockId << " to stream 0x" << id;
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, dbgStringStream.str());
          AVB_ASSERT(0u != mNextClockDomainId);
        }
      }
    }
  }

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::setClockRecoveryParams(uint32_t masterClockId, uint32_t slaveClockId, uint32_t driverId)
{
  IasAvbResult result = IasAvbResult::eIasAvbResultInvalidParam;

  if (!isInitialized())
  {
    result = IasAvbResult::eIasAvbResultErr;
  }
  else
  {
    IasAvbClockDomain * const master = getClockDomainById(masterClockId);
    IasAvbClockDomain * const slave = getClockDomainById(slaveClockId);

    if ((NULL != master) && (NULL != slave))
    {
      IasAvbClockController *controller = new (nothrow) IasAvbClockController;

      if (NULL == controller)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to create clock controller!");
        result = IasAvbResult::eIasAvbResultErr;
      }
      else
      {
        IasAvbProcessingResult res = controller->init(master, slave, driverId);
        if (eIasAvbProcOK != res)
        {
          DLT_LOG_CXX(*mLog,   DLT_LOG_ERROR, LOG_PREFIX, " Error initializing clock controller! Result =", int32_t(res));
          delete controller;
          result = IasAvbResult::eIasAvbResultErr;
        }
        else
        {
          mClockControllers.push_back(controller);
          result = IasAvbResult::eIasAvbResultOk;
        }
      }
    }
  }

  return result;
}

IasAvbResult IasAvbStreamHandler::getAvbStreamInfo(AudioStreamInfoList &audioStreamInfo,
                                           VideoStreamInfoList &videoStreamInfo,
                                           ClockReferenceStreamInfoList &clockRefStreamInfo)
{
  IasAvbResult result = IasAvbResult::eIasAvbResultErr;

  if (isInitialized())
  {
    lockApiMutex();

    /* Ensure our outgoing data structures are clean */
    audioStreamInfo.clear();
    videoStreamInfo.clear();
    clockRefStreamInfo.clear();

    if(mAvbReceiveEngine)
    {
      mAvbReceiveEngine->getAvbStreamInfo(IasAvbStreamId(uint64_t(0)), audioStreamInfo, videoStreamInfo, clockRefStreamInfo);
      result = IasAvbResult::eIasAvbResultOk;
    }

    if (mAvbTransmitEngine)
    {
      mAvbTransmitEngine->getAvbStreamInfo(IasAvbStreamId(uint64_t(0)), audioStreamInfo, videoStreamInfo, clockRefStreamInfo);
      result = IasAvbResult::eIasAvbResultOk;
    }

    unlockApiMutex();
  }

  return result;
}

IasAvbResult IasAvbStreamHandler::getLocalStreamInfo(LocalAudioStreamInfoList &audioStreamInfo,
                                           LocalVideoStreamInfoList &videoStreamInfo)
{
  IasAvbResult result = IasAvbResult::eIasAvbResultErr;

  if (isInitialized())
  {
    lockApiMutex();

    /* Ensure our outgoing data structures are clean */
    audioStreamInfo.clear();
    videoStreamInfo.clear();

    if(mAlsaEngine)
    {
      mAlsaEngine->getLocalStreamInfo(0u, audioStreamInfo);
      result = IasAvbResult::eIasAvbResultOk;
    }
    if(mVideoStreamInterface)
    {
      mVideoStreamInterface->getLocalStreamInfo(0u, videoStreamInfo);
      result = IasAvbResult::eIasAvbResultOk;
    }

    unlockApiMutex();
  }

  return result;
}

IasAvbResult IasAvbStreamHandler::createTransmitVideoStream(IasAvbSrClass srClass,
                                           uint16_t maxPacketRate,
                                           uint16_t maxPacketSize,
                                           IasAvbVideoFormat format,
                                           uint32_t clockId,
                                           IasAvbIdAssignMode assignMode,
                                           uint64_t &streamId,
                                           uint64_t &dmac,
                                           bool active)
{
  IasAvbProcessingResult result = eIasAvbProcOK;
  IasAvbStreamId avbStreamId(streamId);

  lockApiMutex();

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (NULL == mAvbTransmitEngine)
    {
      result = createTransmitEngine();
    }
  }

  if (eIasAvbProcOK == result)
  {
    AVB_ASSERT(NULL != mAvbTransmitEngine);

    IasAvbClockDomain * const clockDomain = getClockDomainById( clockId );
    if (NULL == clockDomain)
    {
      /**
       * @log Invalid param: The clockId parameter is not present in the Clock Domain map.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Clock Domain not found!!");
      result = eIasAvbProcInvalidParam;
    }

    if (eIasAvbProcOK == result)
    {
      // set streamId and DMAC depending on assign mode
      // at the moment only static mode is supported!
      if (IasAvbIdAssignMode::eIasAvbIdAssignModeStatic == assignMode)
      {
        IasAvbMacAddress mac;
        for (uint32_t i = 0u; i < cIasAvbMacAddressLength; i++)
        {
          mac[i] = uint8_t( dmac >> ((cIasAvbMacAddressLength-i-1) * 8) );
        }
        result = mAvbTransmitEngine->createTransmitVideoStream(srClass, maxPacketRate, maxPacketSize, format,
            clockDomain, avbStreamId, mac, mPreConfigurationInProgress);
      }
      else
      {
        /**
         * @log Not implemented: The assignMode parameter type has not yet been implemented.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Assign mode other than 'eIasAvbIdAssignModeStatic' not implemented!");
        result = eIasAvbProcNotImplemented;
      }
    }
  }

  if ((eIasAvbProcOK == result) && (active))
  {
      result = mAvbTransmitEngine->activateAvbStream(avbStreamId);

    if (eIasAvbProcOK != result)
    {
      // stream has been created but couldn't been activated, so remove it
      (void) mAvbTransmitEngine->destroyAvbStream(avbStreamId);
    }
  }

  if (eIasAvbProcOK == result)
  {
    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << streamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Transmit video stream", ssStreamId.str(), "created",
                (active ? "(active)" : "(decative)"));
  }

  unlockApiMutex();

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::createReceiveVideoStream(IasAvbSrClass srClass,
                                          uint16_t maxPacketRate,
                                          uint16_t maxPacketSize,
                                          IasAvbVideoFormat format,
                                          uint64_t streamId,
                                          uint64_t destMacAddr)
{
  // handling by enabled SRP is missing

  IasAvbProcessingResult result = eIasAvbProcOK;
  IasAvbStreamId avbStreamId(streamId);

  lockApiMutex();

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (NULL == mAvbReceiveEngine)
    {
      // if receive engine is not already available, create and initialize it
      mAvbReceiveEngine = new (nothrow) IasAvbReceiveEngine();
      if (NULL == mAvbReceiveEngine)
      {
        /**
         * @log Not enough memory: Could not create a new instance of ReceiveEngine.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to instantiate ReceiveEngine!");
        result = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        result = mAvbReceiveEngine->init();
        if (eIasAvbProcOK != result)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to initialize ReceiveEngine! Result=", int32_t(result));
        }
        else
        {
          result = mAvbReceiveEngine->registerEventInterface(this);
          if (eIasAvbProcOK != result)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to register at ReceiveEngine!", int32_t(result));
          }
        }
        if ((eIasAvbProcOK == result) && isStarted())
        {
          result = mAvbReceiveEngine->start();
        }
      }
    }

    if (eIasAvbProcOK != result)
    {
      delete mAvbReceiveEngine;
      mAvbReceiveEngine = NULL;
    }
  }

  if (eIasAvbProcOK == result)
  {
    IasAvbMacAddress mac;
    for (uint32_t i = 0u; i < cIasAvbMacAddressLength; i++)
    {
      mac[i] = uint8_t(destMacAddr >> ((cIasAvbMacAddressLength - i - 1) * 8));
    }
    result = mAvbReceiveEngine->createReceiveVideoStream(srClass, maxPacketRate, maxPacketSize, format, avbStreamId,
                                                         mac, mPreConfigurationInProgress);
  }

  if (eIasAvbProcOK == result)
  {
    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << streamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Video receive stream", ssStreamId.str() , "created");
  }

  unlockApiMutex();

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::createLocalVideoStream(IasAvbStreamDirection direction,
                                       uint16_t maxPacketRate,
                                       uint16_t maxPacketSize,
                                       IasAvbVideoFormat format,
                                       const std::string &ipcName,
                                       uint16_t &streamId )
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  lockApiMutex();

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (NULL == mVideoStreamInterface)
    {
      mVideoStreamInterface =  new (nothrow) IasVideoStreamInterface();

      if (NULL == mVideoStreamInterface)
      {
        /**
         * @log Not enough memory: Unable to create a new instance of video interface factory.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to instantiate video interface!");
        result = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        result = mVideoStreamInterface->init();
        if (eIasAvbProcOK != result)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Failed to init the video stream interface!");
        }
      }

      if ((eIasAvbProcOK == result))
      {
        result = mVideoStreamInterface->start();
      }

      if (eIasAvbProcOK != result)
      {
        delete mVideoStreamInterface;
        mVideoStreamInterface = NULL;
      }
    }
  }

  if (eIasAvbProcOK == result)
  {
    if(0 != streamId)
    {
      if (isLocalStreamIdInUse(streamId))
      {
        result = eIasAvbProcInvalidParam;
      }
    }
    else
    {
      streamId = getNextLocalStreamId();
    }

    if (eIasAvbProcOK == result)
    {
      result = mVideoStreamInterface->createVideoStream(direction, maxPacketRate, maxPacketSize, format, ipcName, streamId);
    }
  }

  if (eIasAvbProcOK != result)
  {
    streamId = 0u;
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Local video stream", streamId , "created");
  }

  unlockApiMutex();

  return mapResultCode(result);
}


IasAvbResult IasAvbStreamHandler::createTransmitClockReferenceStream(IasAvbSrClass srClass,
                                                                     IasAvbClockReferenceStreamType type,
                                                                     uint16_t crfStampsPerPdu,
                                                                     uint16_t crfStampInterval, uint32_t baseFreq,
                                                                     IasAvbClockMultiplier pull, uint32_t clockId,
                                                                     IasAvbIdAssignMode assignMode,
                                                                     uint64_t &streamId, uint64_t &dmac,
                                                                     bool active)
{
  IasAvbProcessingResult result = eIasAvbProcOK;
  IasAvbStreamId avbStreamId(streamId);

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (NULL == mAvbTransmitEngine)
    {
      result = createTransmitEngine();
    }
  }

  if (eIasAvbProcOK == result)
  {
    AVB_ASSERT(NULL != mAvbTransmitEngine);

    IasAvbClockDomain * const clockDomain = getClockDomainById( clockId );
    if (NULL == clockDomain)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Clock Domain not found!!");
      result = eIasAvbProcInvalidParam;
    }

    if (eIasAvbProcOK == result)
    {
      // set streamId and DMAC depending on assign mode
      // at the moment only static mode is supported!
      if (IasAvbIdAssignMode::eIasAvbIdAssignModeStatic == assignMode)
      {
        IasAvbMacAddress mac;
        for (uint32_t i = 0u; i < cIasAvbMacAddressLength; i++)
        {
          mac[i] = uint8_t(dmac >> ((cIasAvbMacAddressLength-i-1) * 8));
        }
        result = mAvbTransmitEngine->createTransmitClockReferenceStream(srClass, type, crfStampsPerPdu,
                                                     crfStampInterval, baseFreq, pull, clockDomain, avbStreamId, mac);
        if (mBTMEnable)
        {
          // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb created CRS stream",NULL);
        }
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Assign mode other than 'eIasAvbIdAssignModeStatic' not implemented!");
        result = eIasAvbProcNotImplemented;
      }
    }
  }

  if ((eIasAvbProcOK == result) && (active))
  {
      result = mAvbTransmitEngine->activateAvbStream(avbStreamId);

    if (eIasAvbProcOK != result)
    {
      // stream has been created but couldn't been activated, so remove it
      (void) mAvbTransmitEngine->destroyAvbStream(avbStreamId);
    }

  }

  if (eIasAvbProcOK == result)
  {
    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << streamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Transmit clock reference stream", ssStreamId.str() , "created");
  }

  return mapResultCode(result);
}

IasAvbResult IasAvbStreamHandler::createReceiveClockReferenceStream(IasAvbSrClass srClass,
                                                                    IasAvbClockReferenceStreamType type,
                                                                    uint16_t maxCrfStampsPerPdu, uint64_t streamId,
                                                                    uint64_t dmac, uint32_t &clockId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;
  IasAvbStreamId avbStreamId(streamId);

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (NULL == mAvbReceiveEngine)
    {
      result = createReceiveEngine();
    }
  }

  if (eIasAvbProcOK == result)
  {
    AVB_ASSERT(NULL != mAvbReceiveEngine);

    IasAvbMacAddress mac;
    for (uint32_t i = 0u; i < cIasAvbMacAddressLength; i++)
    {
      mac[i] = uint8_t(dmac >> ((cIasAvbMacAddressLength - i - 1) * 8));
    }
    result = mAvbReceiveEngine->createReceiveClockReferenceStream(srClass, type, maxCrfStampsPerPdu, avbStreamId, mac);
  }

  IasAvbResult res = mapResultCode(result);

  if (IasAvbResult::eIasAvbResultOk == res)
  {
    res = deriveClockDomainFromRxStream(streamId, clockId);
  }

  if (IasAvbResult::eIasAvbResultOk == res)
  {
    uint64_t streamIdMcr = 0u;
    if (mEnvironment->queryConfigValue(IasRegKeys::cClkRecoverFrom, streamIdMcr)
        && (streamIdMcr == streamId)
        )
    {
      uint64_t slaveClockId = cIasAvbHwCaptureClockDomainId;
      mEnvironment->queryConfigValue(IasRegKeys::cClkRecoverUsing, slaveClockId);
      res = setClockRecoveryParams(clockId, uint32_t(slaveClockId), 0u);
    }
  }

  if (eIasAvbProcOK == result)
  {
    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << streamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Clock reference stream", ssStreamId.str() , "created");
  }

  return res;
}

IasAvbProcessingResult IasAvbStreamHandler::createTransmitEngine()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  AVB_ASSERT(NULL == mAvbTransmitEngine);
  mAvbTransmitEngine = new (nothrow) IasAvbTransmitEngine();
  if (NULL == mAvbTransmitEngine)
  {
    /**
     * @log Not enough memory: Could not create new TransmitEngine
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to instantiate TransmitEngine!");
    result = eIasAvbProcNotEnoughMemory;
  }
  else
  {
    result = mAvbTransmitEngine->init();
    if (eIasAvbProcOK != result)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Failed to initialize TransmitEngine! Result=", int32_t(result));
    }
    else
    {
      result = mAvbTransmitEngine->registerEventInterface(this);
      if (eIasAvbProcOK != result)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Failed to register at TransmitEngine! Result=", int32_t(result));
      }
    }

    if ((eIasAvbProcOK == result) && isStarted())
    {
      result = mAvbTransmitEngine->start();
    }

    if (eIasAvbProcOK != result)
    {
      delete mAvbTransmitEngine;
      mAvbTransmitEngine = NULL;
    }
  }

  return result;
}

IasAvbProcessingResult IasAvbStreamHandler::createReceiveEngine()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  AVB_ASSERT(NULL == mAvbReceiveEngine);
  mAvbReceiveEngine = new (nothrow) IasAvbReceiveEngine();
  if (NULL == mAvbReceiveEngine)
  {
    /**
     * @log Not enough memory: Could not create new ReceiveEngine.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to instantiate ReceiveEngine!");
    result = eIasAvbProcNotEnoughMemory;
  }
  else
  {
    result = mAvbReceiveEngine->init();
    if (eIasAvbProcOK != result)
    {
      DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, " Failed to initialize ReceiveEngine! Result=", int32_t(result));
    }
    else
    {
      result = mAvbReceiveEngine->registerEventInterface(this);
      if (eIasAvbProcOK != result)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Failed to register at ReceiveEngine!", int32_t(result));
      }
    }
    if ((eIasAvbProcOK == result) && isStarted())
    {
      result = mAvbReceiveEngine->start();
    }

    if (eIasAvbProcOK != result)
    {
      delete mAvbReceiveEngine;
      mAvbReceiveEngine = NULL;
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbStreamHandler::start( bool resume)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  lockApiMutex();

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (isStarted())
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, " already started, ignoring request");
    }
    else
    {
      if ( resume )
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Resume: called during resume from suspend");
        // resume i210
        device_t * pIgbDev = IasAvbStreamHandlerEnvironment::getIgbDevice();
        if (pIgbDev)
        {
          int32_t err = igb_init(pIgbDev);
          if (err)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Resume: igb_init failed (", int32_t(err), ")");
            result = eIasAvbProcInitializationFailed;
          }
        }
        else
        {
          result = eIasAvbProcErr;
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Resume: pIgbDev == NULL!");
        }

        // init ptp
        if (eIasAvbProcOK == result)
        {
          IasLibPtpDaemon* ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
          if (NULL != ptp)
          {
            ptp->init();
          }
          else
          {
            result = eIasAvbProcErr;
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Resume: ptp proxy == NULL!");
          }
        }

        // init Akm pll in case of slave functionality
        if (eIasAvbProcOK == result)
        {
          IasAvbClockDriverInterface* driver = IasAvbStreamHandlerEnvironment::getClockDriver();
          if(NULL != driver)
          {
            if (IasAvbResult::eIasAvbResultOk != driver->init(*mEnvironment))
            {
              result = eIasAvbProcInitializationFailed;
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Resume: Init of clock driver failed!");
            }
          }
        }
      } // resume

      if ((eIasAvbProcOK == result) && (NULL != mAvbTransmitEngine))
      {
        result = mAvbTransmitEngine->start();
      }

      if ((eIasAvbProcOK == result) && (NULL != mAvbReceiveEngine))
      {
        result = mAvbReceiveEngine->start();
      }

      if ((eIasAvbProcOK == result) && (NULL != mAlsaEngine))
      {
        result = mAlsaEngine->start();
      }

      if (eIasAvbProcOK == result)
      {
        mState = eIasStarted;
      }
      else
      {
        (void) stop(false);
      }
    }
  }

  unlockApiMutex();

  return result;
}


IasAvbProcessingResult IasAvbStreamHandler::stop( bool suspend)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  lockApiMutex();

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (!isStarted())
    {
      DLT_LOG_CXX(*mLog,  DLT_LOG_WARN, LOG_PREFIX, " already stopped, ignoring request");
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Stop called (suspend = ", suspend , ")");

      if(!suspend)
      {
        if (NULL != mVideoStreamInterface)
        {
          (void) mVideoStreamInterface->stop();
        }
      }

      if (NULL != mAlsaEngine)
      {
        (void) mAlsaEngine->stop();
      }

      if (NULL != mAvbTransmitEngine)
      {
        (void) mAvbTransmitEngine->stop();
      }

      if (NULL != mAvbReceiveEngine)
      {
        (void) mAvbReceiveEngine->stop();
      }

      // change state from 'started' back to 'initialized'
      mState = eIasInitialized;

      if(suspend)
      {
        // cleanup ptp ( shared memory handling, clock handling)
        IasLibPtpDaemon* ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
        if (NULL != ptp)
        {
          ptp->cleanUp();
        }
        else
        {
          DLT_LOG_CXX(*mLog,   DLT_LOG_ERROR, LOG_PREFIX, " ptp proxy == NULL!");
        }

        IasAvbClockDriverInterface* driver = IasAvbStreamHandlerEnvironment::getClockDriver();
        if(NULL != driver)
        {
          if (IasAvbResult::eIasAvbResultOk != driver->init(*mEnvironment))
          {
            result = eIasAvbProcInitializationFailed;
          }
        }
      }
    }
  }

  unlockApiMutex();

  return result;
}

void IasAvbStreamHandler::emergencyStop()
{
  if (NULL != mAvbReceiveEngine)
  {
    mAvbReceiveEngine->emergencyShutdown();
  }

  if (NULL != mEnvironment)
  {
    mEnvironment->emergencyShutdown();
  }
  DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " IGB emergency shutdown!");
}

IasAvbProcessingResult IasAvbStreamHandler::triggerStorePersistenceData()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  IasLibPtpDaemon* const ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();

  if (NULL != ptp)
  {
    result = ptp->triggerStorePersistenceData();
  }
  else
  {
    result = eIasAvbProcNullPointerAccess;
    DLT_LOG_CXX(*mLog,   DLT_LOG_WARN, LOG_PREFIX, " PTP proxy not available!");
  }

  return result;
}

IasLocalAudioStream * IasAvbStreamHandler::getLocalAudioStreamById(uint16_t id)
{
  IasLocalAudioStream * ret = NULL;

  // ask each local stream provider if it knows the stream with the specified id
  if ((NULL != mAlsaEngine) && (NULL == ret))
  {
    ret = mAlsaEngine->getLocalAudioStream( id );
  }

  if (NULL == ret)
  {
    TestToneStreamMap::iterator it = mTestToneStreams.find(id);
    if (mTestToneStreams.end() != it)
    {
      ret = it->second;
      AVB_ASSERT(NULL != ret);
    }
  }

  return ret;
}


IasLocalVideoStream * IasAvbStreamHandler::getLocalVideoStreamById(uint16_t id)
{
  IasLocalVideoStream * ret = NULL;

  // ask each local stream provider if it knows the stream with the specified id
  if (NULL != mVideoStreamInterface)
  {
    ret = mVideoStreamInterface->getLocalVideoStream( id );
  }

  return ret;
}

bool IasAvbStreamHandler::isLocalStreamIdInUse(uint16_t streamId)
{
  return (NULL != getLocalAudioStreamById(streamId)) || (NULL != getLocalVideoStreamById(streamId));
}

IasAvbClockDomain * IasAvbStreamHandler::getClockDomainById(uint32_t id)
{
  IasAvbClockDomain * ret = NULL;
  AvbClockDomains::iterator it = mAvbClockDomains.find( id );

  if (mAvbClockDomains.end() != it)
  {
    ret = it->second;
  }

  return ret;
}

uint16_t IasAvbStreamHandler::getNextLocalStreamId()
{
  const uint16_t start = mNextLocalStreamId;
  while ((0u == mNextLocalStreamId) || (isLocalStreamIdInUse(mNextLocalStreamId)))
  {
    mNextLocalStreamId++;

    // if we reach start again, all IDs are exhausted (we don't expect this ever to happen)
    AVB_ASSERT( start != mNextLocalStreamId );
    (void) start; // for NDEBUG builds
  }

  return mNextLocalStreamId++;
}

void IasAvbStreamHandler::updateLinkStatus(const bool linkIsUp)
{
  if (NULL != mClient)
  {
    mClient->updateLinkStatus(linkIsUp);
  }
}

void IasAvbStreamHandler::updateStreamStatus(uint64_t streamId, IasAvbStreamState status)
{
  if (NULL != mClient)
  {
    mClient->updateStreamStatus(streamId, status);
  }
}

IasAvbResult IasAvbStreamHandler::mapResultCode(IasAvbProcessingResult code)
{
  switch (code)
  {
  case eIasAvbProcOK:
    return IasAvbResult::eIasAvbResultOk;

  case eIasAvbProcErr:
  case eIasAvbProcInvalidParam:
  case eIasAvbProcOff:
  case eIasAvbProcInitializationFailed:
  case eIasAvbProcNotInitialized:
  case eIasAvbProcNoSpaceLeft:
  case eIasAvbProcNotEnoughMemory:
  case eIasAvbProcAlreadyInUse:
  case eIasAvbProcCallbackError:
  case eIasAvbProcThreadStartFailed:
  case eIasAvbProcThreadStopFailed:
  case eIasAvbProcNullPointerAccess:
    return IasAvbResult::eIasAvbResultErr;

  case eIasAvbProcUnsupportedFormat:
    return IasAvbResult::eIasAvbResultNotSupported;

  case eIasAvbProcNotImplemented:
    return IasAvbResult::eIasAvbResultNotImplemented;

  default:
    return IasAvbResult::eIasAvbResultErr;
  }

  return IasAvbResult::eIasAvbResultErr;
}

} /* namespace IasMediaTransportAvb */
