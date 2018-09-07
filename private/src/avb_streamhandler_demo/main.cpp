/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    main.cpp
 * @brief   THE start of AVB Stream Handler
 * @details
 *
 *
 * @date    2013
 */

#include <stdio.h>
#include <getopt.h>
#include <iostream>
#include <dlt/dlt_cpp_extension.hpp>

#include "avb_streamhandler/IasAvbStreamHandler.hpp"

// TODO REPLACE #include "avb_control/IasAvbControlConfig.hpp"
// TO BE REPLACED #include "core_libraries/btm/ias_dlt_btm.h"
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "version.h"

#ifndef TMP_PATH
#define TMP_PATH "/tmp/"
#endif

using namespace IasMediaTransportAvb;
int32_t verbosity = 0;
volatile sig_atomic_t shutdownReason;

namespace {

#define FULL_VERSION_STRING "Version -P- " VERSION_STRING

static const std::string cClassName = "Main::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

void handleSignal(int sig, siginfo_t *siginfo, void *context)
{
  (void)siginfo;
  (void)context;

  shutdownReason = sig;
}

void installSignals()
{
  struct sigaction act;
  memset (&act, '\0', sizeof(act));
  act.sa_sigaction = &handleSignal;
  act.sa_flags = SA_SIGINFO;

  (void)sigaction(SIGINT,  &act, NULL);
  (void)sigaction(SIGABRT, &act, NULL);
  (void)sigaction(SIGTERM, &act, NULL);
  (void)sigaction(SIGSEGV, &act, NULL);
  (void)sigaction(SIGFPE,  &act, NULL);
  (void)sigaction(SIGUSR1, &act, NULL),
  (void)sigaction(SIGUSR2, &act, NULL);
}

/*
class MySignalHandler : public Ias::IasSignalHandler
{
  virtual void handleSignal(int signum, siginfo_t *info, void *ptr);
public:
  explicit MySignalHandler(IasAvbStreamHandler *streamHandler);
  Ias::IasSignal mShutdown;
  void setStreamhandler(IasAvbStreamHandler *streamHandler) { mStreamHandler = streamHandler; }
  void removeStreamhandler() { mStreamHandler = NULL; }
private:
  IasAvbStreamHandler *mStreamHandler;
};

MySignalHandler::MySignalHandler(IasAvbStreamHandler *streamHandler)
  : mShutdown(Ias::IasSignal::eIasSignalDecreaseModeSetToZero)
  , mStreamHandler(streamHandler)
{
  (void) install(7,
      SIGINT,
      SIGABRT,
      SIGTERM,
      SIGSEGV,
      SIGFPE,
      SIGUSR1,
      SIGUSR2
   );
}

void MySignalHandler::handleSignal(int signum, siginfo_t *info, void *ptr)
{
  std::cerr << "RECEIVED SIGNAL " << signum << " from " << info->si_pid << " / my pid " << getpid() << std::endl;

  (void)ptr;

  shutdownReason = signum;

  switch (signum)
  {
  case SIGINT:
    if (getpid() == info->si_pid)
    {
      // our own process raised SIGINT, so this is probably an assertion failure
      // there is no chance of safely leaving the signal handler => try to close igb
      if (NULL != mStreamHandler)
      {
        mStreamHandler->emergencyStop();
        exit(-1);
      }
    }
    // no break
  case SIGTERM:
    // non-critical shutdown
    break;

  case SIGUSR1:
    std::cout << "stop avb_streamhandler due to SIGUSR1 " << verbosity << "\n" << std::endl;
    break;

  case SIGUSR2:
    std::cout << "start avb_streamhandler due to SIGUSR2 " << verbosity << "\n" << std::endl;
    break;

  case SIGABRT: // watchdog reset - try to cleanup as much as possible and restore default signal handling
  default:
    // critical shutdowns (e.g. SIGSEGV)
    // there is no chance of safely leaving the signal handler => try to close igb
    if (NULL != mStreamHandler)
    {
      mStreamHandler->emergencyStop();
    }

    // unregister this signal handler to avoid endless loops
    struct sigaction actSigInfo;
    memset(&actSigInfo, 0, sizeof actSigInfo);

    actSigInfo.sa_handler = SIG_DFL;
    (void) sigaction(signum, &actSigInfo, NULL);

    if (SIGABRT == signum)
    {
      abort(); // dump a core
    }

    break;
  }

  mShutdown.signal();
}
*/
} // end anon namespace

std::string instanceID = "KSL_DEMO_APPLICATION";
static const char* const cReadyFileName = TMP_PATH "avb_streamhandler.lock";
static DltContext* dltCtx = NULL;

enum IasAvbServiceState
{
    eIasAvbServiceStopped = 0,
    eIasAvbServiceStarting,
    eIasAvbServiceReady
};

//
// Prototypes
//

// TODO REPLACE static IasAvbResult createAvbController(IasAvbStreamHandler *avbStreamHandler, std::shared_ptr<IasAvbControlImp> &avbController);
// TODO REPLACE static void destroyAvbController(std::shared_ptr<IasAvbControlImp> &avbController);


static void setAvbServiceState(IasAvbServiceState state);
static void writeReadyIndicator();

//
// Implementation
//

int main(int argc, char* argv[])
{
  IasMediaTransportAvb::IasAvbStreamHandler* avbStreamHandler = NULL;
  // TODO REPLACE std::shared_ptr<IasAvbControlImp> avbController;
  DltLogLevelType dltLogLevel = DLT_LOG_DEFAULT;
  IasAvbProcessingResult result = eIasAvbProcOK;
  int32_t c         = 0;
  int32_t daemonize = 0;
  int32_t runSetup  = 1;
  int32_t startIpc  = 1;
  int32_t setupArgc = 0;
  char** setupArgv  = NULL;
  int32_t optIdx    = 0;
  bool showUsage    = false;
  bool localPrint   = false;
  volatile int32_t debugSpin = 0;
  std::string configName = "pluginias-media_transport-avb_configuration_reference.so";
  std::string commandline;

  for ( int i = 1; i < argc; i++)
  {
    commandline += argv[i];
    commandline += " ";
  }

  static const struct option options[] =
  {
    { "fg",         no_argument, &daemonize, 0 },
    { "foreground", no_argument, &daemonize, 0 },
    { "bg",         no_argument, &daemonize, 1 },
    { "background", no_argument, &daemonize, 1 },
    { "quiet",      no_argument, &verbosity, -1 },
    { "default",    no_argument, &verbosity, -2 },
    { "verbose",    no_argument, &verbosity, 1 },
    { "nosetup",    no_argument, &runSetup,  0 },
    { "noipc",      no_argument, &startIpc,  0 },
#if IAS_PREPRODUCTION_SW
    { "spin",       no_argument, const_cast<int*>(&debugSpin), 1 },
#endif
    { "config",     required_argument, NULL, 's' },
    { "instance",   required_argument, NULL, 'I' },
    { "help",       no_argument, 0, 'h' },
    { NULL,         0,           0,  0  }
  };

  dltCtx = new DltContext();

  setAvbServiceState(eIasAvbServiceStarting);

  for (;;)
  {
    optIdx = 0;
    c = getopt_long(argc, argv, "+qdv::cs:I:", options, &optIdx);

    if (-1 == c)
    {
      break;
    }

    switch (c)
    {
      case 'q':
      {
        verbosity = -1;
        break;
      }
      case 'd':
      {
        verbosity = -2;
        break;
      }
      case 'v':
      {
        verbosity = 1;
        if (NULL != optarg)
        {
          char* o = optarg;
          while (*o)
          {
            if ('v' == *o)
            {
              verbosity++;
            }
            else
            {
            }
            o++;
          }
        }
        std::cout << "verbosity set to level " << verbosity << "\n" << std::endl;
        break;
      }
      case 'c':
      {
        // enable local console prints
        localPrint = true;
        break;
      }
      case 's':
      {
        if (NULL != optarg)
        {
          if (NULL != std::strchr(optarg, '/'))
          {
            std::cerr << "config plugin file name must not include a path" << std::endl;
            result = eIasAvbProcInvalidParam;
            showUsage = true;
          }
          else
          {
            configName = optarg;
          }
        }
        break;
      }
      case 'I':
      {
        if (NULL != optarg)
        {
          instanceID = optarg;
        }
        break;
      }
      case 'h':
      {
        showUsage = true;
        break;
      }
      case '?':
      {
        showUsage = true;
        result = eIasAvbProcInitializationFailed;
        break;
      }
      default:
      {
        // do nothing
        break;
      }
    }
  }

  if (optind < argc)
  {
    if (std::string("setup") == argv[optind])
    {
      // pass remaining args to config object
      setupArgc = argc - optind;
      setupArgv = argv + optind;
      optind = 0;
    }
    else
    {
      std::cerr << "unrecognized argument: " << argv[optind] << "\n" << std::endl;
      showUsage = true;
    }
  }

  if (showUsage)
  {
    std::cout <<
        "Usage: avb_streamhandler [options] [setup setup-opts]\n"
        "\n"
        "Options:\n"
        "\n"
        "\t--fg or --foreground   puts the streamhandler in foreground mode (default)\n"
        "\t--bg or --background   puts the streamhandler in background mode\n"
        "\t--quiet or -q          do not generate any output to the console\n"
        "\t--verbose              generate more verbose output (same as -v)\n"
        "\t--default              DLT log level will be set to default. This level can be adapted in /etc/dlt.conf\n"
        "\t-v [code]              be more verbose\n"
        "\t-c                     show DLT messages on console\n"
        "\t--nosetup              do not call the configurator object's setup() method\n"
        "\t--noipc                do not start the IPC interfaces\n"
        "\t-s [filename]          specify the plugin containing the configuration\n"
        "\t-I [instance name]     specify the instance name used for communication\n"
        "\t--help                 displays this usage info and exit\n"
        "\n"
        "setup-opts:"
        "\n"
        "\t If the word 'setup' is given in the command line, all subsequent arguments are passed\n"
        "\t to the passArguments() method of the configuration object. See the configuration\n"
        "\t programming documentation for more details.\n"
        "\n"
    << std::endl;
  }
  else
  {
    // showing this copyright notice is legally required due to libigb's BSD license
    std::cout <<
        "AVB StreamHandler\n"
        "Copyright (C) 2018 Intel Corporation. All rights reserved.\n"
        "\n"
        "SPDX-License-Identifier: BSD-3-Clause\n"
        "Version " << FULL_VERSION_STRING
#if IAS_PREPRODUCTION_SW
        << " --PREPRODUCTION--"
#endif
        << "\n" << "Parameters: " << commandline
        << "\n" << std::endl;

    // convert 'verbosity' to DLT log level
    switch (verbosity)
    {
      case -2: dltLogLevel = DLT_LOG_DEFAULT; break;
      case -1: dltLogLevel = DLT_LOG_OFF;     break;
      case  0: dltLogLevel = DLT_LOG_WARN;    break;
      case  1: dltLogLevel = DLT_LOG_INFO;    break;
#if IAS_PREPRODUCTION_SW
      case  2: dltLogLevel = DLT_LOG_DEBUG;   break;
      case  3: dltLogLevel = DLT_LOG_VERBOSE; break;
#endif
      default :
        std::cout << "Invalid verbosity. Using log level DLT_LOG_WARN" << "\n" << std::endl;
        dltLogLevel = DLT_LOG_WARN;
        break;
    }

    while (debugSpin)
    {
      /* This loop never ends until debugSpin is set to 0 by an attached debugger.
       * Use this to attach to a running streamhandler process. It is very
       * difficult to start a streamhandler process in gdb since it will lose
       * the privileges associated with the executable.
       *
       * Try adding special capabilities to gdb using:
       * 'sudo setcap cap_net_raw,cap_sys_nice,cap_sys_ptrace=eip /usr/bin/gdb'
       */
    }

    if (eIasAvbProcOK == result)
    {
      if (1 == daemonize)
      {
        // Put the program in the background. The first param 'nochdir' is used to keep the console at the current directory
        // after the program is put in the background. The second param 'noclose' is used to let the connection between stdin,
        // stdout and stderr open, i.e. all printf of the daemon still go to the current konsole.
        int status = daemon(true, (verbosity >= 0));
        if (0 != status)
        {
          result = eIasAvbProcInitializationFailed;
          if (verbosity >= 0)
          {
            std::cerr << "[" << instanceID << "]" << " ERROR: Couldn't daemonize!" << std::endl;
          }
        }
      }
    }
    while (debugSpin)
    {
      // 2nd debug spin loop, needed when daemon() has been used
    }

    // register for Log and Trace
    if (eIasAvbProcOK == result)
    {
      // AH We have to apply for an own subsystem letter (since 'M' and 'T' are already used we could
      // use 'N' for 'networking' )
      DLT_REGISTER_APP("INAS", "AVB Streamhandler");

      // switch to verbose mode
      DLT_VERBOSE_MODE();

      // disable console prints. Can be enabled by option '-c'
      if (localPrint)
      {
        dlt_enable_local_print();
      }
      else
      {
        dlt_disable_local_print();
      }

      // register context for log and trace
      if (DLT_LOG_DEFAULT == dltLogLevel)
      {
        DLT_REGISTER_CONTEXT(*dltCtx, "_AMN", "AVB streamhandler main");
      }
      else
      {
        DLT_REGISTER_CONTEXT_LL_TS(*dltCtx, "_AMN", "AVB streamhandler main", dltLogLevel, DLT_TRACE_STATUS_OFF);
      }
    }
    // reset atomic variable holding received signal number
    shutdownReason = 0;
    // create a signal handler
    installSignals();

    sigset_t myset;
    (void) sigemptyset(&myset);

    DLT_LOG_CXX(*dltCtx, DLT_LOG_WARN,  "Create Streamhandler ***", FULL_VERSION_STRING );
    DLT_LOG_CXX(*dltCtx, DLT_LOG_WARN,  "Parameters: ", commandline );

    // Create streamhandler
    if (eIasAvbProcOK == result)
    {
      avbStreamHandler = new (nothrow) IasMediaTransportAvb::IasAvbStreamHandler(dltLogLevel);

      if (NULL != avbStreamHandler)
      {
        // register avbstreamhandler at signal handler
        // TO DO handler.setStreamhandler(avbStreamHandler);
      }
      else
      {
        DLT_LOG_CXX(*dltCtx, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't instantiate Streamhandler!");
        result = eIasAvbProcNotEnoughMemory;
      }
    }
    // Initialize the streamhandler
    if (eIasAvbProcOK == result)
    {
      result = avbStreamHandler->init(configName, (runSetup > 0), setupArgc, setupArgv, argv[0]);

      if (eIasAvbProcOK != result)
      {
        DLT_LOG_CXX(*dltCtx, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't initialize Streamhandler!");
      }
    }
    bool abortStartup = true;
    switch(shutdownReason)
    {
      case 0:
        abortStartup = false;
        break;
      case SIGUSR1:
        abortStartup = false;
        break;
      case SIGUSR2:
        abortStartup = false;
        break;
      default:
        abortStartup = true;
        break;
    }

    if (eIasAvbProcOK == result && !abortStartup)
    {
      IasAvbResult avbResult =  IasAvbResult::eIasAvbResultOk;

      //
      // Check if IPC is requested, establish communication
      //
      if (startIpc)
      {
        // Start streamhandler
        result = avbStreamHandler->start(false);
        if ( eIasAvbProcOK == result )
        {
          avbStreamHandler->activateMutexHandling();
          // TODO REPLACE avbResult = createAvbController(avbStreamHandler, avbController);
        }
      }
      else // w/o IPC
      {
        // Start streamhandler
        result = avbStreamHandler->start(false);
      }

      if (eIasAvbProcOK != result)
      {
        DLT_LOG_CXX(*dltCtx, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't start Streamhandler!");
      }
      else if (IasAvbResult::eIasAvbResultOk != avbResult)
      {
        DLT_LOG_CXX(*dltCtx, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't start IPC (AvbController!)");
      }
      else
      {

        setAvbServiceState(eIasAvbServiceReady);

        if (1 == daemonize)
        {
          DLT_LOG_CXX(*dltCtx, DLT_LOG_INFO, LOG_PREFIX, "Waiting for SIGINT or SIGTERM (pid=",
            int32_t(getpid()), ")");
        }
        else
        {
          DLT_LOG_CXX(*dltCtx, DLT_LOG_INFO, LOG_PREFIX, "Waiting for Ctrl-C...");
        }

        bool isActive = true;
        IasAvbResult res = IasAvbResult::eIasAvbResultOk;
        while(isActive)
        {
          //TODO handler.mShutdown.wait();
          (void)sigsuspend(&myset);

          DLT_LOG_CXX(*dltCtx,  DLT_LOG_WARN, LOG_PREFIX, "Signal received: ", int32_t(shutdownReason));

          switch(shutdownReason)
          {
          case SIGUSR1:
            if (startIpc)
            {
              // TODO REPLACE res = avbController->suspend();
              if (IasAvbResult::eIasAvbResultOk != res)
              {
                result = IasAvbProcessingResult::eIasAvbProcErr;
              }
            }
            else
            {
              result = avbStreamHandler->stop(true);
            }

            if (eIasAvbProcOK != result)
            {
              DLT_LOG_CXX(*dltCtx,  DLT_LOG_ERROR, LOG_PREFIX, "Failed to stop Streamhandler on suspend / result=", uint32_t(result));
              isActive = false;
            }
            break;
          case SIGUSR2:
            if (startIpc)
            {
              // TODO REPLACE res = avbController->resume();
              if (IasAvbResult::eIasAvbResultOk != res)
              {
                result = IasAvbProcessingResult::eIasAvbProcErr;
              }
            }
            else
            {
              result = avbStreamHandler->start(true);
            }

            if (eIasAvbProcOK != result)
            {
              DLT_LOG_CXX(*dltCtx,  DLT_LOG_ERROR, LOG_PREFIX, "Failed to start Streamhandler on resume / result=", uint32_t(result));
              isActive = false;
            }
            break;
          default:
            // proceed with standard shutdown procedure
            DLT_LOG_CXX(*dltCtx,  DLT_LOG_WARN, LOG_PREFIX, "shutdown avb streamhandler");
            isActive = false;
            break;
          }
        }

        // signal has been received, stop streamhandler
        if (startIpc)
        {
          // TODO REPLACE destroyAvbController(avbController);
        }
      }
      // TODO REPLACE avbController.reset();
      (void)avbStreamHandler->stop(false);
    }
    setAvbServiceState(eIasAvbServiceStopped);

    // remove the streamhandler from signal handler
    //TODO handler.removeStreamhandler();

    delete avbStreamHandler;
    avbStreamHandler = NULL;
  }

  DLT_UNREGISTER_CONTEXT(*dltCtx);
  delete dltCtx;
  dltCtx = NULL;
  DLT_UNREGISTER_APP();

  if (verbosity >= 0)
  {
    std::cout <<
        "\nStreamhandler exiting. (" << -result << ")"
    << std::endl;
  }

  return -result;
}

/*
static IasAvbResult createAvbController(IasAvbStreamHandler *avbStreamHandler, std::shared_ptr<IasAvbControlImp> &avbController)
{
  IasAvbResult result = IasAvbResult::eIasAvbResultErr;

  AVB_ASSERT(NULL != avbStreamHandler);

  avbController.reset( new (nothrow) IasAvbControlImp(*dltCtx));
  if (NULL != avbController)
  {
    result = avbController->registerService(instanceID);
    if (result != IasAvbResult::eIasAvbResultOk)
    {
      DLT_LOG_CXX(*dltCtx, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't register Service");
    }
    else
    {
      DLT_LOG_CXX(*dltCtx, DLT_LOG_INFO, LOG_PREFIX, "Registered with instance name", instanceID.c_str());
      result = avbController->init(avbStreamHandler);
      if (result != IasAvbResult::eIasAvbResultOk)
      {
        DLT_LOG_CXX(*dltCtx, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't init AVB Controller (", int32_t(result), ")");
      }
    }
  }
  else
  {
    DLT_LOG_CXX(*dltCtx, DLT_LOG_ERROR, LOG_PREFIX, "AVB Controller = NULL!");
  }

  return result;
}
*/

/*
static void destroyAvbController(std::shared_ptr<IasAvbControlImp> &avbController)
{

  (void)avbController->cleanup();
  (void)avbController->unregisterService();
}
*/

void setAvbServiceState(IasAvbServiceState state)
{
#ifdef __ANDROID__
    static const std::string cAvbServiceParameter("avb_streamhandler_state");
    static const std::string cAvbReadyPropName("avb.streamhandler.ready");

    audio_comms::utilities::Property<bool>(cAvbReadyPropName).setValue(state == eIasAvbServiceReady);
    AudioSystem::setParameters(String8(String8::format("%s=%d", cAvbServiceParameter.c_str(), state)));
#endif // __ANDROID__

    switch (state)
    {
    case eIasAvbServiceStarting:
      // TO BE REPLACED ias_dlt_log_btm_start(dltCtx, "avb_streamhandler", NULL);
        break;
    case eIasAvbServiceStopped:
        (void) unlink(cReadyFileName);
        break;
    case eIasAvbServiceReady:
        writeReadyIndicator();
        // TO BE REPLACED ias_dlt_log_btm_ready(dltCtx, "avb_streamhandler", NULL);
        break;
    default:
        break;
    }
}

void writeReadyIndicator()
{
  FILE* fd = fopen(cReadyFileName, "w");
  if (NULL == fd)
  {
    DLT_LOG_CXX(*dltCtx, DLT_LOG_ERROR, LOG_PREFIX, "WARNING: Couldn't write ready indication to /tmp!");
  }
  else
  {
    char buf[256];
    snprintf(buf, (sizeof buf) - 1u, "%d", getpid());
    buf[(sizeof buf) - 1u] = '\0';
    fputs(buf, fd);
    fclose(fd);
  }
}

// EOF
