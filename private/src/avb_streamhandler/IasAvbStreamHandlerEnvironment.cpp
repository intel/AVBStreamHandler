/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbStreamHandlerEnvironment.cpp
 * @brief   This is the implementation of the IasAvbStreamHandlerEnvironment class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasDiaLogger.hpp"
#include "avb_streamhandler/IasAvbTSpec.hpp"
#include "avb_streamhandler/IasLocalAudioBufferDesc.hpp"

#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "avb_helper/ias_safe.h"

#include <cerrno>
#include <cstring>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <algorithm>
#include <sstream>
#include <dlt/dlt_cpp_extension.hpp>
#include <fstream>

#ifndef ETH_P_IEEE1722
#define ETH_P_IEEE1722 0x22F0
#endif

#ifdef __ANDROID__
#include <ethtool.hpp>
#endif

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbStreamHandlerEnvironment::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"


IasAvbStreamHandlerEnvironment* IasAvbStreamHandlerEnvironment::mInstance = NULL;
DltContext* IasAvbStreamHandlerEnvironment::mDltCtxDummy = NULL;

/**
 * List of short and long names for DLT contexts. Must match with contexts specified in manifest file.
 */
const IasAvbStreamHandlerEnvironment::DltCtxNamesStruct IasAvbStreamHandlerEnvironment::dltContextNames[] =
{
/* The following contexts are created separately but listed here for the sake of completeness:
  { "_AMN", "main" },
  { "_ASH", "AVB Streamhandler" },
  { "_ENV", "AVB Environment" },
  { "_DMY", "context not found, using dummy one" },
  the rest are administered in an STL map */
  { "_RXE", "Receive Engine" },
  { "_TXE", "Transmit Engine" },
  { "_TX1", "Transmit Engine class A" },
  { "_TX2", "Transmit Engine class B" },
  { "_THX", "Thread (generic)" },
  { "_AAS", "AVB Audio Stream" },
  { "_ACS", "AVB Clock Reference Stream" },
  { "_AVS", "AVB Video Stream" },
  { "_LAB", "Local Audio Buffer" },
  { "_LVB", "Local Video Buffer" },
  { "_AEN", "Alsa Engine" },
  { "_PTP", "PTP Proxy" },
  { "_SCD", "Software Clock Domain" },
  { "_HCD", "H/W Capture Clock Domain" },
  { "_RCD", "Rx Stream Clock Domain" },
  { "_PCD", "PTP Clock Domain" },
  { "_MCD", "MONOTONIC_RAW Clock Domain" },
  { "_ACC", "AVB Clock Controller" },
  { "_COC", "AVB Component Control" },
  { "_AVI", "AVB Video Interface" },
  { "_RBF", "Ringbuffer Factory" },
  { "_SHM", "AVB SHM Provider" }
};

/**
 * @brief number of context entries in dltContextNames
 */
const size_t IasAvbStreamHandlerEnvironment::numDltContexts = sizeof dltContextNames / sizeof dltContextNames[0];


IasAvbStreamHandlerEnvironment::IasAvbStreamHandlerEnvironment(DltLogLevelType dltLogLevel)
  : mInterfaceName()
  , mPtpProxy(NULL)
  , mMrpProxy(NULL)
  , mIgbDevice(NULL)
  , mStatusSocket(-1)
  , mRegistryLocked(false)
  , mTestingProfileEnabled(false)
  , mClockDriver(NULL)
  , mDltContexts(NULL)
  , mLog(NULL)
  , mDltLogLevel(dltLogLevel)
  , mLibHandle(NULL)
  , mDiaLogger(NULL)
  , mTxRingSize(0)
  , mLastLinkState(0)
  , mArmed(true)
  , mUseWatchdog(false)
  , mWdTimeout(0u)
// TO BE REPLACED  , mWdMainLoop(NULL)
// TO BE REPLACED  , mWdThread(NULL)
// TO BE REPLACED  , mWdManager(NULL)
// TO BE REPLACED  , mWdMainLoopContext(NULL)
#if defined(PERFORMANCE_MEASUREMENT)
  , mAudioFlowLogEnabled(-1)
  , mAudioFlowLoggingState(0)
  , mAudioFlowLoggingTimestamp(0u)
#endif
{
  mLog = new DltContext();
  // register own context for DLT
  if (DLT_LOG_DEFAULT == mDltLogLevel)
  {
    DLT_REGISTER_CONTEXT(*mLog, "_ENV", "Environment");
  }
  else
  {
    DLT_REGISTER_CONTEXT_LL_TS(*mLog, "_ENV", "Environment", mDltLogLevel, DLT_TRACE_STATUS_OFF);
  }

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  AVB_ASSERT(NULL == mInstance);
  mInstance = this;
}


IasAvbStreamHandlerEnvironment::~IasAvbStreamHandlerEnvironment()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  delete mPtpProxy;
  mPtpProxy = NULL;

  AVB_ASSERT(NULL == mMrpProxy); // not yet implemented, should not be set

  if (NULL != mIgbDevice)
  {
    DLT_LOG_CXX(*mLog,  DLT_LOG_INFO, LOG_PREFIX, "igb_detach");
    igb_detach(mIgbDevice); // @@WARN: This will seg fault without the correct capabilities
  }
  delete mIgbDevice;
  mIgbDevice = NULL;

  delete mDiaLogger;
  mDiaLogger = NULL;

  if (mStatusSocket >= 0)
  {
    (void) ::close(mStatusSocket);
    mStatusSocket = -1;
  }

  mClockDriver = NULL;
  if (NULL != mLibHandle)
  {
    (void) ::dlclose(mLibHandle);
    mLibHandle = NULL;
  }

  if (NULL != mDltContexts)
  {
    unregisterDltContexts();
  }

  // unregister and delete contexts

  if ( NULL != mDltCtxDummy )
  {
    DLT_UNREGISTER_CONTEXT(*mDltCtxDummy);
    delete mDltCtxDummy;
    mDltCtxDummy = NULL;

  }

  destroyWatchdog();

  DLT_UNREGISTER_CONTEXT(*mLog);
  delete mLog;
  mLog = NULL;

  mInstance = NULL;
}

void IasAvbStreamHandlerEnvironment::emergencyShutdown()
{
  // helper flag for coverage testing
  if (mArmed)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "igb device might be unstable");
    if (NULL != mIgbDevice)
    {
      DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "igb_detach");
      igb_detach(mIgbDevice); // @@WARN: This will seg fault without the correct capabilities
      delete mIgbDevice;
      mIgbDevice = NULL;
    }
  }
}

IasAvbProcessingResult IasAvbStreamHandlerEnvironment::createPtpProxy()
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if (NULL == mPtpProxy)
  {
    // shared memory object name can remain hard-coded for the time being
    mPtpProxy = new (nothrow) IasLibPtpDaemon("/ptp", static_cast<uint32_t>(SHM_SIZE));
    if (NULL != mPtpProxy)
    {
      if (NULL == mIgbDevice)
      {
        // must create igb device first
        ret = eIasAvbProcInitializationFailed;
      }
      else
      {
        ret = mPtpProxy->init(mIgbDevice);
      }
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to allocate IasLibPtpDaemon");
      ret = eIasAvbProcNotEnoughMemory;
    }

    if (eIasAvbProcOK != ret)
    {
      delete mPtpProxy;
      mPtpProxy = NULL;
    }
  }

  return ret;
}


IasAvbProcessingResult IasAvbStreamHandlerEnvironment::createMrpProxy()
{
  return eIasAvbProcNotImplemented;
}


IasAvbProcessingResult IasAvbStreamHandlerEnvironment::setTxRingSize()
{
  IasAvbProcessingResult result = eIasAvbProcOK;
  const std::string* ifname = getNetworkInterfaceName();
  AVB_ASSERT(NULL != ifname);

  if (ifname->empty())
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Network interface name missing!");
  }
  else if (NULL != mIgbDevice)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "igb device already created!");
  }
  else
  {
    int32_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't open ring size control socket!");
      result = eIasAvbProcErr;
    }
    else
    {
      uint32_t current = 0u;
      struct ifreq ifr;
      struct ethtool_ringparam param;
      (void) ::memset(&ifr, 0, sizeof ifr);
      (void) ::strncpy(ifr.ifr_name, ifname->c_str(), (sizeof ifr.ifr_name) - 1u );
      ifr.ifr_name[(sizeof ifr.ifr_name) - 1u] = '\0';

      param.cmd = ETHTOOL_GRINGPARAM;
      ifr.ifr_data = (char*) &param;
      if (::ioctl(fd, SIOCETHTOOL, &ifr) < 0)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't query ring sizes! (", int32_t(errno), ", ", strerror(errno), ")");
      }
      else
      {
        mTxRingSize = param.tx_pending;
        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Current ring sizes on ", ifr.ifr_name,
            ", TX max =", param.tx_max_pending,
            ", TX current =", param.tx_pending,
            ", RX max =", param.rx_max_pending,
            ", RX current =", param.rx_pending);
        current = param.tx_pending;
      }

      uint64_t ring = 0u;
      if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cDebugNwIfTxRingSize, ring) && (ring != current))
      {
        /*
         * ethtool set commands needs root privileges or CAP_NET_ADMIN capability.
         */

        param.tx_pending = uint32_t(ring);
        param.cmd = ETHTOOL_SRINGPARAM;

        if (::ioctl(fd, SIOCETHTOOL, &ifr) < 0)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't set TX ring size! (", int32_t(errno),
              ", ", strerror(errno), ")");
          result = eIasAvbProcErr;
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Set TX ring size for", ifr.ifr_name,
              "to", uint32_t(param.tx_pending));
          mTxRingSize = param.tx_pending;
        }
      }

      close(fd);
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbStreamHandlerEnvironment::createIgbDevice()
{
  IasAvbProcessingResult ret = eIasAvbProcOK;
  int32_t err = -1;

  if (NULL == mIgbDevice)
  {
    mIgbDevice = new (nothrow) device_t;

    if (NULL == mIgbDevice)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to allocate device_t");
      ret = eIasAvbProcNotEnoughMemory;
    }
    else
    {
      bool result = false;
      const uint16_t cPciPathMaxLen = 24u;
      char devPath[cPciPathMaxLen]  = {0};

      mIgbDevice->private_data = NULL;

      std::ostringstream filePath;
      getNetworkInterfaceName();
      filePath << "/sys/class/net/" << mInterfaceName.c_str() << "/device/uevent";
      std::cout << "Interface name: " << mInterfaceName.c_str() << std::endl;

      std::string fileName = filePath.str();
      std::ifstream configFile(fileName);
      std::string configLine;

      if (configFile.good())
      {
        while (std::getline(configFile, configLine))
        {
          std::stringstream line(configLine);
          std::string value;
          std::vector<std::string> values;

          if (configLine.find("PCI_ID") != std::string::npos)
          {
            while (std::getline(line, value, '='))
            {
              while (std::getline(line, value, ':'))
              {
                values.push_back(value);
              }
            }

            mIgbDevice->pci_device_id = uint16_t(std::stoi(values.back(), nullptr, 16));
            values.pop_back();

            mIgbDevice->pci_vendor_id = uint16_t(std::stoi(values.back(), nullptr, 16));
            values.pop_back();
          }
          else if (configLine.find("PCI_SLOT_NAME") != std::string::npos)
          {
            while (std::getline(line, value, '='))
            {
              while (std::getline(line, value, ':'))
              {
                std::stringstream tmpLine(value);
                while (std::getline(tmpLine, value, '.'))
                {
                  values.push_back(value);
                }
              }
            }

            mIgbDevice->func = uint8_t(std::stoi(values.back(), nullptr, 16));
            values.pop_back();

            mIgbDevice->dev = uint8_t(std::stoi(values.back(), nullptr, 16));
            values.pop_back();

            mIgbDevice->bus = uint8_t(std::stoi(values.back(), nullptr, 16));
            values.pop_back();

            mIgbDevice->domain = uint16_t(std::stoi(values.back(), nullptr, 16));
            values.pop_back();
          }
          result = true;
        }
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not find configuration file for interface");
        ret = eIasAvbProcInitializationFailed;
      }

      if (result)
      {
        // workaround: try to query own mac address. if that fails, we won't have sufficient permissions anyway
        // note: querySourceMac() also sets mInterfaceName, if not already done
        if (eIasAvbProcOK != querySourceMac())
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "igb device cannot be attached (check permissions and net interface name");
          ret = eIasAvbProcInitializationFailed;
        }
        else
        {
          (void) ::snprintf(devPath, cPciPathMaxLen, "%04x:%02x:%02x.%d", mIgbDevice->domain, mIgbDevice->bus, mIgbDevice->dev, mIgbDevice->func);

          err = igb_attach(devPath, mIgbDevice);
          if (err)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "igb_attach for",
                mInterfaceName.c_str(), "at",
                devPath, "failed (", int32_t(err),
                ",", strerror(err), ")");
            ret = eIasAvbProcInitializationFailed;
          }
          else
          {
            DLT_LOG_CXX(*mLog,  DLT_LOG_INFO, LOG_PREFIX, "igb_attach OK");
#if defined(DIRECT_RX_DMA)
            err = igb_attach_rx(mIgbDevice);
            if (err)
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "igb_attach_rx failed (",
                  int32_t(err), ")");
              ret = eIasAvbProcInitializationFailed;
            }
            else
            {
#endif /* DIRECT_RX_DMA */
              err = igb_attach_tx(mIgbDevice);
              if (err)
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "igb_attach_tx failed (",
                    int32_t(err), ")");
                ret = eIasAvbProcInitializationFailed;
              }
#if defined(DIRECT_RX_DMA)
            }

            if (eIasAvbProcOK == ret)
#else
            else
#endif /* DIRECT_RX_DMA */
            {
              err = igb_init(mIgbDevice);
              if (err)
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "igb_init failed (",
                    int32_t(err), ")");
                ret = eIasAvbProcInitializationFailed;
              }
            }
            if (eIasAvbProcOK != ret)
            {
              DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "igb_detach (init failed)");
              igb_detach(mIgbDevice);
            }
          }
        }
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "could not load config values");
        ret = eIasAvbProcInitializationFailed;
      }

      if (eIasAvbProcOK != ret)
      {
        delete mIgbDevice;
        mIgbDevice = NULL;
      }
    }
  }

  return ret;
}


IasAvbProcessingResult IasAvbStreamHandlerEnvironment::querySourceMac()
{
  IasAvbProcessingResult ret = eIasAvbProcOK;
  struct ::ifreq if_request;

  int32_t rc = 0;
  const std::string* interfaceName = getNetworkInterfaceName();

  if ((NULL == interfaceName) || (interfaceName->empty()))
  {
    ret = eIasAvbProcErr;
  }
  else
  {
    if (mStatusSocket < 0)
    {
      ret = openRawSocket();
    }

    if (eIasAvbProcOK == ret)
    {
      std::memset(&if_request, 0, sizeof(if_request));
      std::strncpy(if_request.ifr_name, interfaceName->c_str(), sizeof (if_request.ifr_name) - 1u);
      if_request.ifr_name[(sizeof if_request.ifr_name) - 1u] = '\0';
      rc = ::ioctl(mStatusSocket, SIOCGIFHWADDR, &if_request);

      if (rc < 0)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "could not query network i/f [",
            interfaceName->c_str(), "] for MAC address");
        ret = eIasAvbProcErr;
      }
      else
      {
        avb_safe_result copyResult = avb_safe_memcpy(&mSourceMac, sizeof(IasAvbMacAddress), if_request.ifr_hwaddr.sa_data, cIasAvbMacAddressLength);
        AVB_ASSERT(e_avb_safe_result_ok == copyResult);
        (void) copyResult;
      }

      // keep socket open for link status queries
    }
  }

  return ret;
}

bool IasAvbStreamHandlerEnvironment::queryLinkState()
{
  bool ret = false;
  if (mStatusSocket < 0)
  {
    if (eIasAvbProcOK != openRawSocket())
    {
       DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "could not open socket to query network i/f status");
       return ret;
    }
  }

  int32_t rc = 0;
  struct ::ifreq if_request;

  std::memset(&if_request, 0, sizeof(if_request));
  std::strncpy(if_request.ifr_name, mInterfaceName.c_str(), (sizeof if_request.ifr_name) - 1u);
  if_request.ifr_name[(sizeof if_request.ifr_name) - 1u] = '\0';
  rc = ::ioctl(mStatusSocket, SIOCGIFFLAGS, &if_request);

  if (rc < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "could not query network i/f status");
    ret = false;
  }
  else
  {
    ret = ((if_request.ifr_flags & IFF_RUNNING) != 0);
  }

  // Link up / down counters, memorize last state and act upon change
  // also, if link goes "up", trigger emission of "Ethernet ready" status message
  if ( (mLastLinkState & IFF_UP) != (if_request.ifr_flags & IFF_UP) )
  {
    // Create if it doesn't already exist.
    if (NULL == mDiaLogger)
    {

      if (eIasAvbProcOK != createDiaLogger())
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to create DiaLogger.");

        delete mDiaLogger;
        mDiaLogger = NULL;
      }
    }

    mLastLinkState = if_request.ifr_flags;

    if ((mLastLinkState & IFF_UP) == 0)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Link down");

      if ((NULL != mInstance) && (NULL != mInstance->mDiaLogger))
      {
        mInstance->mDiaLogger->incLinkDown();
      }
    }
    else
    {
      // Inc Link Up
      if ((NULL != mInstance) && (NULL != mInstance->mDiaLogger))
      {
        // The timestamp should be the gPtp timestamp at which the state was entered
        // Not sure if this is the right one.
        uint64_t timestamp = mInstance->mPtpProxy->getPtpTime();
        mInstance->mDiaLogger->setTimestampField(timestamp);

        mInstance->mDiaLogger->incLinkUp();
        mInstance->mDiaLogger->triggerEthReadyPacket(mStatusSocket);
      }
    }
  }

  return ret;
}


DltContext &IasAvbStreamHandlerEnvironment::getDltContext(const std::string &dltContextName)
{
  DltContext* retContext = NULL;

  if ((NULL != mInstance) && (NULL != mInstance->mDltContexts))
  {
    uint32_t i = 0u;
    for (; i < numDltContexts; i++)
    {
      if (0u == strncmp(mInstance->mDltContexts[i].contextID, dltContextName.c_str(), 4))
      {
        break;
      }
    }

    if (i < numDltContexts)
    {
      retContext = mInstance->mDltContexts + i;
    }
  }

  if (NULL == retContext)
  {
    if (NULL == mDltCtxDummy)
    {
      mDltCtxDummy = new DltContext();
      DLT_REGISTER_CONTEXT_LL_TS(*mDltCtxDummy, "_DMY", "context not found, using dummy one", DLT_LOG_INFO,
                                 DLT_TRACE_STATUS_OFF);
    }
    retContext = mDltCtxDummy;
    DLT_LOG_CXX(*mDltCtxDummy, DLT_LOG_WARN, LOG_PREFIX, "WARNING: Context", dltContextName, "not found, using dummy context");
  }
  AVB_ASSERT(NULL != retContext);
  return *retContext;
}


bool IasAvbStreamHandlerEnvironment::doGetConfigValue(const std::string &key, uint64_t &value)
{
  bool ret = false;

  if (NULL != mInstance)
  {
    RegistryMapNumeric::iterator it = mInstance->mRegistryNumeric.find(key);

    if (mInstance->mRegistryNumeric.end() != it)
    {
      value = it->second;
      ret = true;
    }
    else
    {
      RegistryMapTextual::iterator it = mInstance->mRegistryTextual.find(key);

      if (mInstance->mRegistryTextual.end() != it)
      {
        DLT_LOG_CXX(*(mInstance->mLog), DLT_LOG_WARN, LOG_PREFIX, "Warning: apparent type mismatch (text instead of numeric) for registry key=", key.c_str());
      }
    }
  }

  return ret;
}


bool IasAvbStreamHandlerEnvironment::getConfigValue(const std::string &key, std::string &value)
{
  bool ret = false;

  if (NULL != mInstance)
  {
    RegistryMapTextual::iterator it = mInstance->mRegistryTextual.find(key);

    if (mInstance->mRegistryTextual.end() != it)
    {
      value = it->second;
      ret = true;
    }
    else
    {
      RegistryMapNumeric::iterator it = mInstance->mRegistryNumeric.find(key);

      if (mInstance->mRegistryNumeric.end() != it)
      {
        DLT_LOG_CXX(*(mInstance->mLog), DLT_LOG_WARN, LOG_PREFIX, "Warning: apparent type mismatch (numeric instead of text) for registry key=", key.c_str());
      }
    }
  }

  return ret;
}


IasAvbResult IasAvbStreamHandlerEnvironment::setConfigValue(const std::string& key, uint64_t value)
{
  IasAvbResult ret = IasAvbResult::eIasAvbResultOk;

  if (mRegistryLocked)
  {
    ret = IasAvbResult::eIasAvbResultErr;
  }
  else if (key.empty())
  {
    ret = IasAvbResult::eIasAvbResultInvalidParam;
  }
  else
  {
    mRegistryNumeric[key] = value;
  }

  return ret;
}


IasAvbResult IasAvbStreamHandlerEnvironment::setConfigValue(const std::string& key, const std::string& value)
{
  IasAvbResult ret = IasAvbResult::eIasAvbResultOk;

  if (mRegistryLocked)
  {
    ret = IasAvbResult::eIasAvbResultErr;
  }
  else if (key.empty())
  {
    ret = IasAvbResult::eIasAvbResultInvalidParam;
  }
  else
  {
    mRegistryTextual[key] = value;
  }

  return ret;
}


void IasAvbStreamHandlerEnvironment::setDefaultConfigValues()
{
  /**
   * In general, classes will set default values internally and overwrite
   * them with config values only when config values are provided.
   * However, there are values that are shared among classes, so
   * the (common) default value needs to be defined here.
   */

  // default values apply to I210 on Crestview Hills module
  setConfigValue(IasRegKeys::cNwIfName, "eth0");

  // ALSA section
  setConfigValue(IasRegKeys::cAlsaNumFrames, 256u);
  setConfigValue(IasRegKeys::cAlsaRingBuffer, 12u);
  setConfigValue(IasRegKeys::cAlsaBaseFreq, 48000u);
  setConfigValue(IasRegKeys::cAlsaBasePeriod, 128u);

  // Audio time-aware buffer section
  setConfigValue(IasRegKeys::cAudioBaseFillMultiplier, 15u);
  setConfigValue(IasRegKeys::cAudioTstampBuffer,
      static_cast<uint64_t>(IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeFailSafe));

  // AVB RX
  setConfigValue(IasRegKeys::cRxClkUpdateInterval, 10000u);

  // scheduler parameter
  setConfigValue(IasRegKeys::cSchedPolicy, "fifo");
  setConfigValue(IasRegKeys::cSchedPriority, 20u);

  // Timeout:cIgbAccessSleep (in us: 100 ms) * cIgbAccessTimeoutCnt
  setConfigValue(IasRegKeys::cIgbAccessTimeoutCnt, 100u);
}

bool IasAvbStreamHandlerEnvironment::validateRegistryEntries()
{
  uint64_t val = 0u;
  bool ret = true;

#if !IAS_PREPRODUCTION_SW
  {
    RegistryMapNumeric::iterator it = mRegistryNumeric.begin();
    while(it != mRegistryNumeric.end())
    {
      if (it->first.find("debug.") == 0u)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "removed registry entry ", it->first.c_str());
        it = mRegistryNumeric.erase(it);
      }
      else
      {
        it++;
      }
    }
  }
#endif

  (void) getConfigValue(IasRegKeys::cDebugDumpRegistry, val);
  if (0u != val)
  {
    for (RegistryMapNumeric::iterator it = mRegistryNumeric.begin(); it != mRegistryNumeric.end(); it++)
    {
      /**
       * @log Registry entry
       */

      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "reg[", it->first.c_str(),
          "]=", (it->second));
    }
    for (RegistryMapTextual::iterator it = mRegistryTextual.begin(); it != mRegistryTextual.end(); it++)
    {
      /**
       * @log Registry entry
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "reg[", it->first.c_str(),
          "]=", it->second.c_str());
    }
  }

  /**
   * Here, sanity checks for configuration checks can be added.
   * But in many cases, only the class using the values has got the background knowledge
   * to actually do the checks.
   */

#if !IAS_PREPRODUCTION_SW
  {
    uint64_t val = 0u;
    if (queryConfigValue(IasRegKeys::cRxValidationMode, val) && (0u == val))
    {
      mRegistryNumeric[IasRegKeys::cRxValidationMode] = 1u;
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "corrected validation mode");
    }
  }
#endif

  // lock registry against further changes
  mRegistryLocked = true;

  return ret;
}

bool IasAvbStreamHandlerEnvironment::queryConfigValue(const std::string& key, std::string& value) const
{
  bool ret = false;

  RegistryMapTextual::const_iterator it = mInstance->mRegistryTextual.find(key);

  if (mInstance->mRegistryTextual.end() != it)
  {
    value = it->second;
    ret = true;
  }

  return ret;
}

bool IasAvbStreamHandlerEnvironment::queryConfigValue(const std::string& key, uint64_t& value) const
{
  bool ret = false;

  RegistryMapNumeric::const_iterator it = mInstance->mRegistryNumeric.find(key);

  if (mInstance->mRegistryNumeric.end() != it)
  {
    value = it->second;
    ret = true;
  }

  return ret;
}


IasAvbProcessingResult IasAvbStreamHandlerEnvironment::loadClockDriver(const std::string& fileName)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  /* verify fileName: must not contain a path, so dlopen will use the predefined paths to look
   * for the shared lib. For security reasons, the lib must not reside in any other directory
   * than those searched by dlopen().
   */

  if (fileName.empty())
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "no clock driver file name specified");
    ret = eIasAvbProcInvalidParam;
  }
  else if (std::string::npos != fileName.find("/"))
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "clock driver file name must not include a path");
    ret = eIasAvbProcInvalidParam;
  }
  else
  {
    // nothing to do
  }

  if (eIasAvbProcOK == ret)
  {
    const char* lastErr = NULL;
    mLibHandle = ::dlopen(fileName.c_str(), RTLD_LAZY);

    if (NULL == mLibHandle)
    {
      lastErr = ::dlerror();
      AVB_ASSERT(NULL != lastErr);
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error:", lastErr);
      ret = eIasAvbProcErr;
    }
    else
    {
      typedef IasAvbClockDriverInterface& (*InterfaceFunctionGetter)();

      // C-style cast is about the only form that allows void*-to-function pointer
      InterfaceFunctionGetter getter = (InterfaceFunctionGetter) ::dlsym( mLibHandle, "getIasAvbClockDriverInterfaceInstance");
      lastErr = ::dlerror();
      if (NULL != lastErr)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error:", fileName.c_str(),
            "does not seem to be a valid clock driver library (",
            lastErr, ")");

        ret = eIasAvbProcErr;
        (void) ::dlclose(mLibHandle);
        mLibHandle = NULL;
      }
      else
      {
        AVB_ASSERT(NULL != getter);
        mClockDriver = &getter();
      }
    }
  }

  return ret;
}


IasAvbProcessingResult IasAvbStreamHandlerEnvironment::registerDltContexts()
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  mDltContexts = new (nothrow) DltContext[numDltContexts];

  if (NULL == mDltContexts)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "couldn't allocate context list");
    ret = eIasAvbProcNotEnoughMemory;
  }
  else
  {
    // verify if log level for environment has been updated by -k command line option
    std::string logLevelKey = IasRegKeys::cDebugLogLevelPrefix;
    logLevelKey += "_env";
    int32_t logLevel = mDltLogLevel;
    if (IasAvbStreamHandlerEnvironment::getConfigValue(logLevelKey, logLevel) && logLevel != mDltLogLevel)
    {
      DLT_UNREGISTER_CONTEXT(*mLog);
      if (DLT_LOG_DEFAULT == logLevel)
      {
        DLT_REGISTER_CONTEXT(*mLog, "_ENV", "Environment");
      }
      else
      {
        DLT_REGISTER_CONTEXT_LL_TS(*mLog, "_ENV", "Environment", logLevel, DLT_TRACE_STATUS_OFF);
      }
    }

    // register other contexts
    for (uint32_t i = 0u; i < numDltContexts; i++)
    {
      // get log level from registry
      std::string logLevelKey = IasRegKeys::cDebugLogLevelPrefix;
      logLevelKey += dltContextNames[i].shortName;
      std::transform(logLevelKey.begin(), logLevelKey.end(), logLevelKey.begin(), ::tolower);
      int32_t logLevel = mDltLogLevel;
      if (IasAvbStreamHandlerEnvironment::getConfigValue(logLevelKey, logLevel) || DLT_LOG_DEFAULT != mDltLogLevel)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "registering DLT context. Key=",
            dltContextNames[i].shortName, "LogLevel=", int32_t(logLevel));

        // register DLT context
        DLT_REGISTER_CONTEXT_LL_TS(mDltContexts[i],
            dltContextNames[i].shortName,
            dltContextNames[i].longName,
            static_cast<DltLogLevelType> (logLevel),
            DLT_TRACE_STATUS_OFF);
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "registering DLT context. Key=",
            dltContextNames[i].shortName, "with default LogLevel");

        // register DLT context
        DLT_REGISTER_CONTEXT(mDltContexts[i],
            dltContextNames[i].shortName,
            dltContextNames[i].longName);
      }
    }
  }

  DLT_LOG_CXX(*mLog,   DLT_LOG_DEBUG, LOG_PREFIX, "mDltContexts size=", numDltContexts);

  return ret;
}


IasAvbProcessingResult IasAvbStreamHandlerEnvironment::unregisterDltContexts()
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if (NULL != mDltContexts)
  {
    for (uint32_t i = 0; i < numDltContexts; i++)
    {
      char id[DLT_ID_SIZE + 1u] = { '\0' };
      strncpy(id, mDltContexts[i].contextID, DLT_ID_SIZE);

      DLT_UNREGISTER_CONTEXT(mDltContexts[i]);
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "DLT context:", id, "unregistered");
    }
    delete[] mDltContexts;
    mDltContexts = NULL;
  }

  return ret;
}


IasAvbProcessingResult IasAvbStreamHandlerEnvironment::openRawSocket()
{
  /*
   * Opening a raw socket needs root privileges or CAP_NET_RAW capability.
   * This could be avoided here by using a UDP socket instead. However, the
   * receive engine uses raw sockets anyway, so it does not really make a difference.
   */

  mStatusSocket = ::socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IEEE1722));
  if (mStatusSocket < 0)
  {
    perror("sock fail - ");
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to open socket.");
    return eIasAvbProcErr;
  }

  // Enable broadcast on the socket.
  int32_t optVal = 1;
  int32_t result = ::setsockopt(mStatusSocket, SOL_SOCKET, SO_BROADCAST, &optVal, sizeof(1));
  if (result < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to set broadcast flag.");
    return eIasAvbProcErr;
  }

  if (!getConfigValue(IasRegKeys::cTestingProfileEnable, mTestingProfileEnabled))
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Testing Profile Enable config value not set,"
        " DiaLogger disabled as default.");
  }

  return eIasAvbProcOK;
}


IasAvbProcessingResult IasAvbStreamHandlerEnvironment::createDiaLogger()
{
  mDiaLogger = new (std::nothrow) IasDiaLogger();
  if (NULL == mDiaLogger)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to create DiaLogger.");
    return eIasAvbProcErr;
  }

  IasAvbProcessingResult result = mDiaLogger->init(mInstance);
  switch (result)
  {
    case eIasAvbProcNotEnoughMemory:
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to create DiagnosticPacket.");
      break;

    case eIasAvbProcInitializationFailed:
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Initialisation failed.");
      break;

    default:
      break;
  }

  return result;
}

void IasAvbStreamHandlerEnvironment::notifySchedulingIssue(DltContext &dltContext, const std::string &text, const uint64_t elapsed, const uint64_t limit)
{
  DLT_LOG_CXX(dltContext, DLT_LOG_WARN, LOG_PREFIX, "thread scheduling exception: ", text.c_str());
  DLT_LOG_CXX(dltContext, DLT_LOG_WARN, LOG_PREFIX, "slept for:", double(elapsed)/1e6, "ms, limit was",
	double(limit)/1e6, "ms");
}

int32_t IasAvbStreamHandlerEnvironment::queryLinkSpeed()
{
  int32_t speed = -1;
  if (mStatusSocket < 0)
  {
    if (eIasAvbProcOK != openRawSocket())
    {
       DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "could not open socket to query network i/f status");
       return speed;
    }
  }

  int32_t rc = 0;
  struct ::ifreq if_request;
  struct ::ethtool_cmd ecmd;

  std::memset(&if_request, 0, sizeof(if_request));
  std::strncpy(if_request.ifr_name, mInterfaceName.c_str(), (sizeof if_request.ifr_name) - 1u);
  if_request.ifr_name[(sizeof if_request.ifr_name) - 1u] = '\0';

  if_request.ifr_data = reinterpret_cast<char*>(&ecmd);
  ecmd.cmd = ETHTOOL_GSET;

  rc = ::ioctl(mStatusSocket, SIOCETHTOOL, &if_request);

  if (rc < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "could not query network i/f speed");
  }
  else
  {
    switch (ethtool_cmd_speed(&ecmd))
    {
    case SPEED_100:
      speed = 100;
      break;

    case SPEED_1000:
      speed = 1000;
      break;

    default:
      break;

    }
  }

  return speed;
}


IasAvbProcessingResult IasAvbStreamHandlerEnvironment::createWatchdog()
{
  IasAvbProcessingResult ret = eIasAvbProcInitializationFailed;

#if 0 // TODO KSL: to be replace by sufficient implementation
  if (NULL != mWdManager)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "Already created watchdog");
    ret = eIasAvbProcOK;
  }
  else
  {
    char const * const envStr = getenv("WATCHDOG_USEC");
    if (envStr != NULL)
    {
      uint32_t timeout = 0;
      std::stringstream stream;

      stream << envStr;
      if (true != stream.bad())
      {
        stream >> timeout;  // valid timeout range will be checked by IasWatchdog::createWatchdogManager
        mWdTimeout = timeout / 1000u; // us -> ms
        mUseWatchdog = true;
      }
    }
    else
    {
      /*
       * no watchdog configuration found in env 'WATCHDOG_USEC'.
       * Treat this as a normal use case since users might now want to use either systemd or
       * systemd's watchdog feature.
       */
      mUseWatchdog = false;
      ret = eIasAvbProcOK; // success
    }

    if (mUseWatchdog)
    {
      mWdMainLoop = new (std::nothrow) Ias::IasCommonApiMainLoop(*mLog);
      if (NULL == mWdMainLoop)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to allocate IasCommonApiMainLoop!");
        ret = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        mWdMainLoopContext.reset(new (std::nothrow) CommonAPI::MainLoopContext());
        if (NULL == mWdMainLoopContext)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to allocate MainLoopContext!");
          ret = eIasAvbProcNotEnoughMemory;
        }
        else
        {
          if (IasResult::cOk != mWdMainLoop->init(mWdMainLoopContext))
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Watchdog MainLoop init failed!");
            ret = eIasAvbProcInitializationFailed;
          }
          else
          {
            mWdThread = new (std::nothrow) IasThread(mWdMainLoop);
            if (NULL == mWdThread)
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to allocate IasThread!");
              ret = eIasAvbProcNotEnoughMemory;
            }
            else
            {
              mWdManager = IasWatchdog::createWatchdogManager(mWdMainLoopContext, *mLog);
              if (NULL == mWdManager)
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "failed to create watchdog manager");
                ret = eIasAvbProcInitializationFailed;
              }
              else
              {
                if (IAS_FAILED(mWdThread->start(true)))
                {
                  DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "failed to start watchdog thread");
                  ret = eIasAvbProcInitializationFailed;
                }
                else
                {
                  // success
                  ret = eIasAvbProcOK;
                }
              }
            }
          }
        }
      }
    }
  }

  if (eIasAvbProcOK != ret)
  {
    destroyWatchdog();
  }
#endif

  return ret;
}


void IasAvbStreamHandlerEnvironment::destroyWatchdog()
{
#if 0 // TODO KSL: to be replace by sufficient implementation  if (NULL != mWdThread)
  {
    if (mWdThread->isRunning())
    {
      (void) mWdThread->stop();
    }
    delete mWdThread;
    mWdThread = NULL;
  }

  if (NULL != mWdManager)
  {
    IasWatchdog::destroyWatchdogManager(mWdManager);
    mWdManager = NULL;
  }

  if (NULL != mWdMainLoopContext)
  {
    mWdMainLoopContext.reset();
  }

  if (NULL != mWdMainLoop)
  {
    delete mWdMainLoop;
    mWdMainLoop = NULL;
  }
#endif

  mWdTimeout = 0u;
  mUseWatchdog = false;
}


} /* namespace IasMediaTransportAvb */
