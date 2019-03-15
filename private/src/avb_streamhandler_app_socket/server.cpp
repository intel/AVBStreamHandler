/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    server.cpp
 * @brief   This executable starts the AVB Stream Handler and creates a socket which
 *          receives command requests and calls the command via the Avb Stream Handler instance.
 *
 * @details
 *          Avb Stream Handler start options are the same with the original avb_streamhandler_app_demo.
 *          To enable client-server communication, sockets are used. Serialization is used over the socket
 *          to enable the sending of structs with user parameters.
 *
 *          Note: Each request command from a client will have a corresponding execute function:
 *          CLIENT          SERVER
 *          Request   ->
 *                      ->  Execute
 *                      <-
 *          Receive   <-
 *
 *          To add new commands at the server-side, add into the 3 sections:
 *          -Declare new command class
 *          -Add to command table
 *          -Define its execute() function
 *
 * @date    May 2018
 */

#include <iomanip>
#include <stdio.h>
#include <getopt.h>
#include <iostream>
#include <sstream>
#include <dlt/dlt_cpp_extension.hpp>

#include "avb_streamhandler/IasAvbStreamHandler.hpp"
#include "avb_streamhandler_app_socket/IasAvbStreamHandlerSocketIpc.hpp"

#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "version.h"
#include <thread>

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#ifndef TMP_PATH
#define TMP_PATH "/tmp/"
#endif

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 81
#endif

using namespace IasMediaTransportAvb;
int32_t verbosity = 0;
volatile sig_atomic_t shutdownReason;

std::string instanceID = "DEMO_APPLICATION";
std::string appName = "avb_streamhandler_app_socket";

static const char* const cReadyFileName = TMP_PATH "avb_streamhandler.lock";
static DltContext* dltCtx = NULL;

enum IasAvbServiceState
{
    eIasAvbServiceStopped = 0,
    eIasAvbServiceStarting,
    eIasAvbServiceReady
};

//Function prototype
static void setAvbServiceState(IasAvbServiceState state);
static void writeReadyIndicator();
std::stringstream printDiagnostics (const IasAvbStreamDiagnostics &diag);
std::stringstream printAvbStreamInfo(AudioStreamInfoList &audioStreamInfo,VideoStreamInfoList &videoStreamInfo,ClockReferenceStreamInfoList &clockRefStreamInfo);
std::stringstream printLocalStreamInfo(LocalAudioStreamInfoList &localAudioStreamInfo);
IasAvbAudioFormat getAudioFormat(uint32_t audioFormat);
IasAvbVideoFormat getVideoFormat(uint32_t videoFormat);
IasAvbIdAssignMode getAssignMode(uint32_t assingMode);
const char * getResultString(IasAvbResult result);

/*******************************************************
  Server-side io_service
*******************************************************/
namespace AvbStreamHandlerSocketIpc {

class server
{
public:
  server(boost::asio::io_service& io_service,
      unsigned short port,
      IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
      AvbStreamHandlerSocketIpc::Command **serverCmdTbl,
      int cmdTblSize)
    : acceptor_(io_service,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
  {
    avbStreamHandler_ = avbStreamHandler;
    serverCmdTbl_ = serverCmdTbl;
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    cmdTblSize_ = cmdTblSize;
    connx = new connection(acceptor_.get_io_service());
    acceptor_.async_accept(connx->socket(),
        boost::bind(&server::handle_accept, this,boost::asio::placeholders::error));
  }

  void handle_accept(const boost::system::error_code& e)
  {
    if (!e)
    {
      connx->async_read(requestSocketIpcStruct_,
          boost::bind(&server::handle_read, this,
          boost::asio::placeholders::error));
    }
    else
    {
      std::cerr << e.message() << std::endl;
    }
  }

  void handle_read(const boost::system::error_code& e)
  {
    if (!e)
    {
      std::cout << "\tCommand requested: " << requestSocketIpcStruct_.command << "\n";

      for (int i = 0; i < cmdTblSize_; i++)
      {
        if (serverCmdTbl_[i]->getName() == requestSocketIpcStruct_.command)
        {
          responseSocketIpcStruct_ = serverCmdTbl_[i]->execute(avbStreamHandler_, &requestSocketIpcStruct_);
          break;
        }
      }

      connx->async_write(responseSocketIpcStruct_,
          boost::bind(&server::handle_write, this,
          boost::asio::placeholders::error));
    }
    else
    {
      std::cerr << e.message() << std::endl;
    }
  }

  void handle_write(const boost::system::error_code& e)
  {
    if (!e)
    {
      connx = new connection(acceptor_.get_io_service());

      acceptor_.async_accept(connx->socket(),
      boost::bind(&server::handle_accept, this,boost::asio::placeholders::error));
    }
    else
    {
      std::cerr << e.message() << std::endl;
    }
  }

private:
  boost::asio::ip::tcp::acceptor acceptor_;
  AvbStreamHandlerSocketIpc::connection* connx;
  IasMediaTransportAvb::IasAvbStreamHandler* avbStreamHandler_;
  AvbStreamHandlerSocketIpc::Command** serverCmdTbl_;
  int cmdTblSize_;
  requestSocketIpc requestSocketIpcStruct_;
  responseSocketIpc responseSocketIpcStruct_;
};

} // namespace avbsocketipc

/// Command derived class
#define DECLARE_COMMAND(name) \
class name : public AvbStreamHandlerSocketIpc::Command \
{ \
public: \
  name() : AvbStreamHandlerSocketIpc::Command (#name, "Not required in server command table", 0) {} \
  AvbStreamHandlerSocketIpc::responseSocketIpc execute (IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler, AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct);\
};\
name name##Obj; // command instance

//Command names only for server-side
DECLARE_COMMAND(GetAvbStreamInfo)
DECLARE_COMMAND(GetLocalStreamInfo)
DECLARE_COMMAND(CreateTransmitAvbAudioStream)
DECLARE_COMMAND(CreateReceiveAudioStream)
DECLARE_COMMAND(DestroyStream)
DECLARE_COMMAND(SetStreamActive)
DECLARE_COMMAND(CreateAlsaStream)
DECLARE_COMMAND(DestroyLocalStream)
DECLARE_COMMAND(ConnectStreams)
DECLARE_COMMAND(DisconnectStreams)
DECLARE_COMMAND(SetChannelLayout)
DECLARE_COMMAND(Monitor)
DECLARE_COMMAND(CreateTransmitAvbVideoStream)
DECLARE_COMMAND(CreateReceiveVideoStream)
DECLARE_COMMAND(CreateLocalVideoStream)
DECLARE_COMMAND(CreateTestToneStream)
DECLARE_COMMAND(SetTestToneParams)
DECLARE_COMMAND(SuspendStreamhandler)

AvbStreamHandlerSocketIpc::Command* serverCmdTbl[] =
{
  &GetAvbStreamInfoObj,
  &GetLocalStreamInfoObj,
  &CreateTransmitAvbAudioStreamObj,
  &CreateReceiveAudioStreamObj,
  &DestroyStreamObj,
  &SetStreamActiveObj,
  &CreateAlsaStreamObj,
  &DestroyLocalStreamObj,
  &ConnectStreamsObj,
  &DisconnectStreamsObj,
  &SetChannelLayoutObj,
  &MonitorObj,
  &CreateTransmitAvbVideoStreamObj,
  &CreateReceiveVideoStreamObj,
  &CreateLocalVideoStreamObj,
  &CreateTestToneStreamObj,
  &SetTestToneParamsObj,
  &SuspendStreamhandlerObj
};

/*******************************************************
  Command's execute functions definitions
*******************************************************/
/// GetAvbStreamInfo command
AvbStreamHandlerSocketIpc::responseSocketIpc GetAvbStreamInfo::execute (
      IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
      AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::AudioStreamInfoList audioStreamInfo;
  IasMediaTransportAvb::VideoStreamInfoList videoStreamInfo;
  IasMediaTransportAvb::ClockReferenceStreamInfoList clockRefStreamInfo;
  IasMediaTransportAvb::LocalAudioStreamInfoList localAudioStreamInfo;
  IasMediaTransportAvb::LocalVideoStreamInfoList localVideoStreamInfo;

  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  resultx = avbStreamHandler->getAvbStreamInfo(audioStreamInfo,videoStreamInfo,clockRefStreamInfo);

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;

  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.avbStreamInfo = printAvbStreamInfo(audioStreamInfo,videoStreamInfo,clockRefStreamInfo).str();
  responseSocketIpcStruct.result = getResultString(resultx);

  return responseSocketIpcStruct;
}

/// GetLocalStreamInfo command
AvbStreamHandlerSocketIpc::responseSocketIpc GetLocalStreamInfo::execute (
      IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
      AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::LocalAudioStreamInfoList localAudioStreamInfo;
  IasMediaTransportAvb::LocalVideoStreamInfoList localVideoStreamInfo;

  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  resultx = avbStreamHandler->getLocalStreamInfo(localAudioStreamInfo,localVideoStreamInfo);

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;

  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.avbStreamInfo = printLocalStreamInfo(localAudioStreamInfo).str();
  responseSocketIpcStruct.result = getResultString(resultx);

  return responseSocketIpcStruct;
}

/// CreateTransmitAvbAudioStream command
AvbStreamHandlerSocketIpc::responseSocketIpc CreateTransmitAvbAudioStream::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{

  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  if (getAudioFormat(requestedCmdStruct->format) != IasAvbAudioFormat::eIasAvbAudioFormatSaf16)
  {
    std::cout << "Instead of format(" << requestedCmdStruct->format << ") ";
    std::cout << "using eIasAvbAudioFormatSaf16(format = 1), since it is the currently supported format.\n";
    requestedCmdStruct->format=1;
  }

  resultx = avbStreamHandler->createTransmitAudioStream (
          requestedCmdStruct->srClass,
          requestedCmdStruct->numOfCh,
          requestedCmdStruct->sampleFreq,
          getAudioFormat(requestedCmdStruct->format),
          requestedCmdStruct->clockId,
          getAssignMode(requestedCmdStruct->assignMode),
          requestedCmdStruct->networkStreamId,
          requestedCmdStruct->dmac,
          requestedCmdStruct->active
          );

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;

  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = getResultString(resultx);

  //streamid may be updated by avbstreamhandler if it is not available
  responseSocketIpcStruct.oStreamId = requestedCmdStruct->networkStreamId;

  return responseSocketIpcStruct;
}

/// CreateReceiveAudioStream command
AvbStreamHandlerSocketIpc::responseSocketIpc CreateReceiveAudioStream::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{

  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  resultx = avbStreamHandler->createReceiveAudioStream (
          requestedCmdStruct->srClass,
          requestedCmdStruct->numOfCh,
          requestedCmdStruct->sampleFreq,
          requestedCmdStruct->networkStreamId,
          requestedCmdStruct->dmac
          );

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = getResultString(resultx);

  //streamid may be updated by avbstreamhandler if it is not available
  responseSocketIpcStruct.oStreamId = requestedCmdStruct->networkStreamId;

  return responseSocketIpcStruct;
}

/// DestroyStream command
AvbStreamHandlerSocketIpc::responseSocketIpc DestroyStream::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  resultx = avbStreamHandler->destroyStream(requestedCmdStruct->networkStreamId);

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = getResultString(resultx);

  return responseSocketIpcStruct;
}

/// SetStreamActive command
AvbStreamHandlerSocketIpc::responseSocketIpc SetStreamActive::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  resultx = avbStreamHandler->setStreamActive(requestedCmdStruct->networkStreamId,requestedCmdStruct->active);

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = getResultString(resultx);

  return responseSocketIpcStruct;
}

/// CreateAlsaStream command
AvbStreamHandlerSocketIpc::responseSocketIpc CreateAlsaStream::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;
  IasAlsaDeviceTypes alsaDeviceType = ( requestedCmdStruct->alsaDeviceType == 0) ? IasAlsaDeviceTypes::eIasAlsaVirtualDevice : IasAlsaDeviceTypes::eIasAlsaHwDevice;
  IasAvbStreamDirection streamDirection = ( requestedCmdStruct->direction == 0) ? IasAvbStreamDirection::eIasAvbTransmitToNetwork : IasAvbStreamDirection::eIasAvbReceiveFromNetwork;

  if (getAudioFormat(requestedCmdStruct->format) != IasAvbAudioFormat::eIasAvbAudioFormatSaf16)
  {
    std::cout << "Instead of format(" << requestedCmdStruct->format << ") ";
    std::cout << "using eIasAvbAudioFormatSaf16(format = 1), since it is the currently supported format.\n";
    requestedCmdStruct->format=1;
  }
  //Bug: casting to ostreamid only required for createalsastream
  uint16_t oStreamId = 0;

  resultx = avbStreamHandler->createAlsaStream (
          streamDirection,
          requestedCmdStruct->numOfCh,
          requestedCmdStruct->sampleFreq,
          getAudioFormat(requestedCmdStruct->format),
          requestedCmdStruct->clockId,
          requestedCmdStruct->periodSize,
          requestedCmdStruct->numPeriods,
          requestedCmdStruct->channelLayout,
          requestedCmdStruct->hasSideChannel,
          requestedCmdStruct->deviceName,
          oStreamId,
          alsaDeviceType,
          requestedCmdStruct->sampleFreqASRC
          );

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = getResultString(resultx);

  //streamid may be updated by avbstreamhandler if it is not available
  responseSocketIpcStruct.oStreamId = oStreamId;

  return responseSocketIpcStruct;
}

/// DestroyLocalStream command
AvbStreamHandlerSocketIpc::responseSocketIpc DestroyLocalStream::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  resultx = avbStreamHandler->destroyLocalStream(requestedCmdStruct->localStreamId);

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = getResultString(resultx);

  return responseSocketIpcStruct;
}

/// ConnectStreams command
AvbStreamHandlerSocketIpc::responseSocketIpc ConnectStreams::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  resultx = avbStreamHandler->connectStreams(requestedCmdStruct->networkStreamId,
      requestedCmdStruct->localStreamId);

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = getResultString(resultx);

  return responseSocketIpcStruct;
}

/// DisconnectStreams command
AvbStreamHandlerSocketIpc::responseSocketIpc DisconnectStreams::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  resultx = avbStreamHandler->disconnectStreams(requestedCmdStruct->networkStreamId);

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = getResultString(resultx);

  return responseSocketIpcStruct;
}

/// SetChannelLayout command
AvbStreamHandlerSocketIpc::responseSocketIpc SetChannelLayout::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  resultx = avbStreamHandler->setChannelLayout(requestedCmdStruct->localStreamId,
      requestedCmdStruct->channelLayout);

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = getResultString(resultx);

  return responseSocketIpcStruct;
}

/// Monitor command - NOT IMPLEMENTED TODO: find another way to implement this
AvbStreamHandlerSocketIpc::responseSocketIpc Monitor::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{

/*- this was a common api capability. avbsh do not have this.
  std::cout << "subscribing for events...\n";

  (void) mAvbControlClient->subscribeForEvents();

  std::cout << "waiting for events...\n";

  pause();

  (void) mAvbControlClient->unsubscribeEvents();

  return IasAvbResult::eIasAvbResultOk;
*/
  (void) *avbStreamHandler; //suppress temporarily
  std::cout << "\tResult: " << IasAvbResult::eIasAvbResultNotImplemented << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = IasAvbResult::eIasAvbResultNotImplemented;

  return responseSocketIpcStruct;
}

/// CreateTransmitAvbVideoStream command
AvbStreamHandlerSocketIpc::responseSocketIpc CreateTransmitAvbVideoStream::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  if (getVideoFormat(requestedCmdStruct->format) == -1)
  {
    requestedCmdStruct->format=1; // default eIasAvbVideoFormatRtp
  }

  resultx = avbStreamHandler->createTransmitVideoStream (
          requestedCmdStruct->srClass,
          requestedCmdStruct->maxPacketRate,
          requestedCmdStruct->maxPacketSize,
          getVideoFormat(requestedCmdStruct->format),
          requestedCmdStruct->clockId,
          getAssignMode(requestedCmdStruct->assignMode),
          requestedCmdStruct->networkStreamId,
          requestedCmdStruct->dmac,
          requestedCmdStruct->active
          );

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = getResultString(resultx);

  //streamid may be updated by avbstreamhandler if it is not available
  responseSocketIpcStruct.oStreamId = requestedCmdStruct->networkStreamId;

  return responseSocketIpcStruct;
}

/// CreateReceiveVideoStream command
AvbStreamHandlerSocketIpc::responseSocketIpc CreateReceiveVideoStream::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  if (getVideoFormat(requestedCmdStruct->format) == -1)
  {
    requestedCmdStruct->format=1; // default eIasAvbVideoFormatRtp
  }

  resultx = avbStreamHandler->createReceiveVideoStream (
          requestedCmdStruct->srClass,
          requestedCmdStruct->maxPacketRate,
          requestedCmdStruct->maxPacketSize,
          getVideoFormat(requestedCmdStruct->format),
          requestedCmdStruct->networkStreamId,
          requestedCmdStruct->dmac
          );

  std::cout << "\tResult: " << getResultString(resultx) << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = getResultString(resultx);

  //streamid may be updated by avbstreamhandler if it is not available
  responseSocketIpcStruct.oStreamId = requestedCmdStruct->networkStreamId;

  return responseSocketIpcStruct;
}

/// CreateLocalVideoStream command
// TODO: Not implemented/enabled now. To update after vstreaming implementation is ready
AvbStreamHandlerSocketIpc::responseSocketIpc CreateLocalVideoStream::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  //suppress temporarily
  //IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  if (getVideoFormat(requestedCmdStruct->format) == -1)
  {
    requestedCmdStruct->format=1; // default eIasAvbVideoFormatRtp
  }
  (void) *avbStreamHandler; //suppress temporarily
  std::cout << "\tResult: " << IasAvbResult::eIasAvbResultNotImplemented << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = IasAvbResult::eIasAvbResultNotImplemented;

  //streamid may be updated by avbstreamhandler if it is not available
  responseSocketIpcStruct.oStreamId = requestedCmdStruct->networkStreamId;

  return responseSocketIpcStruct;
}

/// CreateTestToneStream command
AvbStreamHandlerSocketIpc::responseSocketIpc CreateTestToneStream::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;
  requestedCmdStruct->oStreamId = 0;

  if (getAudioFormat(requestedCmdStruct->format) != IasAvbAudioFormat::eIasAvbAudioFormatSafFloat)
  {
    std::cout << "Instead of format(" << requestedCmdStruct->format << ") ";
    std::cout << "using SafFloat(format = 4), since currently it is the supported format.\n";
  }

  resultx = avbStreamHandler->createTestToneStream (requestedCmdStruct->numOfCh,
                    requestedCmdStruct->sampleFreq,
                    IasAvbAudioFormat::eIasAvbAudioFormatSafFloat,
                    requestedCmdStruct->channelLayout,
                    requestedCmdStruct->oStreamId
                    );

  std::cout << "\tResult: " << IasAvbResult::eIasAvbResultNotImplemented << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.oStreamId = requestedCmdStruct->oStreamId;
  responseSocketIpcStruct.result = getResultString(resultx);

  return responseSocketIpcStruct;
}

/// SetTestToneParams command
AvbStreamHandlerSocketIpc::responseSocketIpc SetTestToneParams::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;
  requestedCmdStruct->oStreamId = 0;

  if (0 == requestedCmdStruct->signalFrequency)
  {
    requestedCmdStruct->signalFrequency = 997u;
  }
  if (0 == requestedCmdStruct->userParam)
  {
    switch(requestedCmdStruct->toneMode)
    {
    case IasAvbTestToneMode::eIasAvbTestToneSawtooth:
      requestedCmdStruct->userParam = -1;
      break;
    case IasAvbTestToneMode::eIasAvbTestTonePulse:
      requestedCmdStruct->userParam = 50;
      break;
    default:
      break;
    }
  }

  resultx = avbStreamHandler->setTestToneParams (requestedCmdStruct->localStreamId,
                          requestedCmdStruct->channelIdx,
                          requestedCmdStruct->signalFrequency,
                          requestedCmdStruct->amplitude,
                          requestedCmdStruct->toneMode,
                          requestedCmdStruct->userParam
                          );

  std::cout << "\tResult: " << IasAvbResult::eIasAvbResultNotImplemented << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = getResultString(resultx);

  return responseSocketIpcStruct;
}

/// SuspendStreamhandler command
AvbStreamHandlerSocketIpc::responseSocketIpc SuspendStreamhandler::execute (
			IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler,
			AvbStreamHandlerSocketIpc::requestSocketIpc *requestedCmdStruct)
{
  //suppress temporarily
  //IasMediaTransportAvb::IasAvbResult resultx = IasAvbResult::eIasAvbResultErr;

  /* TODO: re-enable suspending avb streamhandler
   Need to be implemented on server's main() first
  resultx = avbStreamHandler->SuspendStreamhandler (requestedCmdStruct->suspendAction);
  */
  (void) *avbStreamHandler; //suppress temporarily
  std::cout << "\tResult: " << IasAvbResult::eIasAvbResultNotImplemented << std::endl;
  AvbStreamHandlerSocketIpc::responseSocketIpc responseSocketIpcStruct;
  responseSocketIpcStruct.command = requestedCmdStruct->command;
  responseSocketIpcStruct.result = IasAvbResult::eIasAvbResultNotImplemented;

  return responseSocketIpcStruct;
}

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

  (void)sigaction(SIGINT, &act, NULL);
  (void)sigaction(SIGABRT, &act, NULL);
  (void)sigaction(SIGTERM, &act, NULL);
  (void)sigaction(SIGSEGV, &act, NULL);
  (void)sigaction(SIGFPE, &act, NULL);
  (void)sigaction(SIGUSR1, &act, NULL),
  (void)sigaction(SIGUSR2, &act, NULL);

}

/*
//TODO: Re-enable and update the suspending of avb streamhandler with signals.
//      Code below is old implementation where signal handler calls avbControl to trigger avbsh to suspend
//      Keeping it for future reference.
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

/*******************************************************
  Helper functions definitions and main
*******************************************************/
std::stringstream printDiagnostics (const IasAvbStreamDiagnostics &diag)
{
  std::stringstream ss;

  ss << "\t\tMedia";
  ss << " locked "      << diag.getMediaLocked();
  ss << " unlocked "    << diag.getMediaUnlocked();
  ss << " reset "       << diag.getMediaReset();
  ss << " unsupported " << diag.getUnsupportedFormat();
  ss << " interrupted " << diag.getStreamInterrupted();
  ss << " seqmismatch " << diag.getSeqNumMismatch();
  ss << std::endl;

  ss << "\t\tTimestamp";
  ss << " valid "     << diag.getTimestampValid();
  ss << " invalid "   << diag.getTimestampNotValid();
  ss << " late "      << diag.getLateTimestamp();
  ss << " early "     << diag.getEarlyTimestamp();
  ss << " uncertain " << diag.getTimestampUncertain();
  ss << std::endl;

  ss << "\t\t";
  ss << "RX frames "  << diag.getFramesRx();
  ss << " TX frames " << diag.getFramesTx();
  ss << std::endl;

  return ss;
};

std::stringstream printLocalStreamInfo(LocalAudioStreamInfoList &localAudioStreamInfo)
{
  std::stringstream ss;
  for (LocalAudioStreamInfoList::const_iterator i = localAudioStreamInfo.begin(); i != localAudioStreamInfo.end(); i++)
  {
    const IasLocalAudioStreamAttributes &info = *i;
    IasLocalAudioStreamDiagnostics diag = info.getStreamDiagnostics();

    ss << "\tLocal Network Stream ID: 0x" << std::left << std::setw(16) << std::hex << info.getStreamId();
    ss << std::endl;
    if (IasAvbStreamDirection::eIasAvbTransmitToNetwork == info.getDirection())
    {
      ss << "\t\t"
                << "TX"
                << " ch "  << info.getNumChannels()
                << " freq " << std::dec << info.getSampleFrequency()
                << " format " << info.getFormat()
                << " period-size " << info.getPeriodSize()
                << " num-of-period " << info.getNumPeriods()
                << " ch-layout " << info.getChannelLayout()
                << " side-ch " << info.getHasSideChannel()
                << " device-name " << info.getDeviceName()
                << (info.getConnected() ? " connected" : " not connected")
                  ;
    }
    else
    {
      ss << "\t\t"
                << "RX"
                << " ch "  << info.getNumChannels()
                << " freq " << std::dec << info.getSampleFrequency()
                << " format " << info.getFormat()
                << " period-size " << info.getPeriodSize()
                << " num-of-period " << info.getNumPeriods()
                << " ch-layout " << info.getChannelLayout()
                << " side-ch " << info.getHasSideChannel()
                << " device-name " << info.getDeviceName()
                << (info.getConnected() ? " connected" : " not connected")
                  ;
    }

    ss << std::endl;
    ss << "\t\tDiagnostics:";
    ss << " base-period " << diag.getBasePeriod();
    ss << " base-freq " << diag.getBaseFreq();
    ss << " base-fillmultiplier " << diag.getBaseFillMultiplier();
    ss << " cycle-wait " << diag.getCycleWait();
    ss << " buffer-totalsize " << diag.getTotalBufferSize();
    ss << " buffer-threshold " << diag.getBufferReadThreshold();
    ss << " buffer-count " << diag.getResetBuffersCount();
    ss << " deviation " << diag.getDeviationOutOfBounds();

    ss << std::endl;
  }
  return ss;
}

std::stringstream printAvbStreamInfo(AudioStreamInfoList &audioStreamInfo,
    VideoStreamInfoList &videoStreamInfo,
    ClockReferenceStreamInfoList &clockRefStreamInfo
    )
{
  std::stringstream ss;

  for (AudioStreamInfoList::const_iterator i = audioStreamInfo.begin(); i != audioStreamInfo.end(); i++)
  {
    const IasAvbAudioStreamAttributes &info = *i;
    IasAvbStreamDiagnostics diag = info.getDiagnostics();

    ss << "\tNetwork Stream ID: 0x" << std::left << std::setw(16) << std::hex << info.getStreamId();
    ss << std::endl;

    if (IasAvbStreamDirection::eIasAvbTransmitToNetwork == info.getDirection())
    {
      ss << "\t\t"
                << "TX"
                << " DMAC " << info.getDmac()
                << " SMAC " << info.getSourceMac() << std::dec
                << " ch "  << info.getNumChannels() << "/" << info.getMaxNumChannels()
                << " freq " << std::dec << info.getSampleFreq()
                << " format " << info.getFormat()
                << " clock " << info.getClockId()
                << (info.getTxActive() ? " active" : " inactive")
                  ;
    }
    else
    {
      ss << "\t\t"
                << "RX"
                << " DMAC " << info.getDmac()
                << " SMAC " << info.getSourceMac() << std::dec
                << " ch "  << info.getNumChannels() << "/" << info.getMaxNumChannels()
                << " freq " << std::dec << info.getSampleFreq()
                << " format " << info.getFormat()
                << " status " << info.getRxStatus()
                  ;
    }

    if (0u != info.getLocalStreamId())
    {
      ss << " local " << info.getLocalStreamId();
    }
    else
    {
      ss << " <not connected>";
    }
    ss << (info.getPreconfigured() ? " pre-config" : " post-config") << std::endl;
    ss << printDiagnostics (diag).str();
    ss << std::endl;
  }

  ss << "Stream Type: Video\n";
  for (VideoStreamInfoList::const_iterator i = videoStreamInfo.begin(); i != videoStreamInfo.end(); i++)
  {
    const IasAvbVideoStreamAttributes &info = *i;
    IasAvbStreamDiagnostics diag = info.getDiagnostics();

    ss << "\tNetwork Stream ID: 0x" << std::left << std::setw(16) << std::hex << info.getStreamId();
    ss << std::endl;

    if (IasAvbStreamDirection::eIasAvbTransmitToNetwork == info.getDirection())
    {
      ss << "\t\t"
                << "TX"
                << " DMAC " << info.getDmac()
                << " SMAC " << info.getSourceMac() << std::dec
                << " " << info.getMaxPacketRate() << " * " << info.getMaxPacketSize() << " bytes/s"
                << " format " << info.getFormat()
                << " clock " << info.getClockId()
                << (info.getTxActive() ? " active" : " inactive")
                  ;
    }
    else
    {
      ss << "\t\t"
                << "RX"
                << " DMAC " << info.getDmac()
                << " SMAC " << info.getSourceMac() << std::dec
                << " " << info.getMaxPacketRate() << " * " << info.getMaxPacketSize() << " bytes/s"
                << " format " << info.getFormat()
                << " status " << info.getRxStatus()
                  ;
    }
    if (0u != info.getLocalStreamId())
    {
      ss << " local " << info.getLocalStreamId();
    }
    else
    {
      ss << " <not connected>";
    }
    ss << (info.getPreconfigured() ? " pre-config" : " post-config") << std::endl;
    ss << printDiagnostics(diag).str();
    ss << std::endl;
  }

  ss << "Stream Type: Clock Reference\n";
  for (ClockReferenceStreamInfoList::const_iterator i = clockRefStreamInfo.begin(); i != clockRefStreamInfo.end(); i++)
  {
    const IasAvbClockReferenceStreamAttributes &info = *i;
    IasAvbStreamDiagnostics diag = info.getDiagnostics();

    ss << "\tNetwork Stream ID: 0x" << std::left << std::setw(16) << std::hex << info.getStreamId();
    ss << std::endl;

    if (IasAvbStreamDirection::eIasAvbTransmitToNetwork == info.getDirection())
    {
      ss << "\t\t"
          << "TX"
          << " DMAC " << info.getDmac()
          << " SMAC " << info.getSourceMac() << std::dec
          << " stamps per PDU " << info.getCrfStampsPerPdu()
          << " stamp interval " << info.getCrfStampInterval()
          << " base frequency " << info.getBaseFreq()
          << " clock "          << info.getClockId()
          << " assign mode "    << info.getAssignMode()
          << " pull "           << info.getPull()
          << (info.getTxActive() ? " active" : " inactive")
            ;
    }
    else
    {
      ss << "\t\t"
          << "RX"
          << " DMAC " << info.getDmac()
          << " SMAC " << info.getSourceMac() << std::dec
          << " stamps per PDU " << info.getCrfStampsPerPdu()
          << " stamp interval " << info.getCrfStampInterval()
          << " base frequency " << info.getBaseFreq()
          << " type "           << info.getType()
          << " clock "          << info.getClockId()
          << " status " << info.getRxStatus()
            ;
    }
    ss << (info.getPreconfigured() ? " pre-config" : " post-config") << std::endl;
    ss << printDiagnostics(diag).str();
    ss << std::endl;
  }
  return ss;
}

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
      // TODO: REPLACED ias_dlt_log_btm_start(dltCtx, "avb_streamhandler", NULL);
        break;
    case eIasAvbServiceStopped:
        (void) unlink(cReadyFileName);
        break;
    case eIasAvbServiceReady:
        writeReadyIndicator();
        // TODO: REPLACED ias_dlt_log_btm_ready(dltCtx, "avb_streamhandler", NULL);
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


IasAvbAudioFormat getAudioFormat(uint32_t audioFormat)
{
  IasAvbAudioFormat retVal = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;

  if (0 == audioFormat)
    retVal = IasAvbAudioFormat::eIasAvbAudioFormatIec61883;
  else if (1 == audioFormat)
    retVal = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  else if (2 == audioFormat)
    retVal = IasAvbAudioFormat::eIasAvbAudioFormatSaf24;
  else if (3 == audioFormat)
    retVal = IasAvbAudioFormat::eIasAvbAudioFormatSaf32;
  else if (4 == audioFormat)
    retVal = IasAvbAudioFormat::eIasAvbAudioFormatSafFloat;
  else
    std::cout << std::endl << appName << ":ERROR: wrong audio format! Using default value (SAF16)\n";

  return retVal;
}

IasAvbVideoFormat getVideoFormat(uint32_t videoFormat)
{
  IasAvbVideoFormat retVal = IasAvbVideoFormat::eIasAvbVideoFormatRtp;

  if (0 == videoFormat)
    retVal = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  else if (1 == videoFormat)
    retVal = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  else
    printf("\navb_streamhandler_client_app:ERROR: wrong video format! Using default value (RTP)\n");

  return retVal;
}

IasAvbIdAssignMode getAssignMode(uint32_t assingMode)
{
  IasAvbIdAssignMode retVal = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;

  if (0 == assingMode)
    retVal = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  else if (1 == assingMode)
    retVal = IasAvbIdAssignMode::eIasAvbIdAssignModeDynamicAll;
  else if (2 == assingMode)
    retVal = IasAvbIdAssignMode::eIasAvbIdAssignModeDynamicMaap;
  else if (3 == assingMode)
    retVal = IasAvbIdAssignMode::eIasAvbIdAssignModeDynamicSrp;
  else
    std::cout << std::endl << appName << ":ERROR: Wrong assign mode! Using default value (Static)\n";

  return retVal;
}

const char * getResultString(IasAvbResult result)
{
  switch (result)
  {
  case IasAvbResult::eIasAvbResultOk:
    return "eIasAvbResultOk";
    break;
  case IasAvbResult::eIasAvbResultErr:
    return "eIasAvbResultErr";
    break;
  case IasAvbResult::eIasAvbResultNotImplemented:
    return "eIasAvbResultNotImplemented";
    break;
  case IasAvbResult::eIasAvbResultNotSupported:
    return "eIasAvbResultNotSupported";
    break;
  case IasAvbResult::eIasAvbResultInvalidParam:
    return "eIasAvbResultInvalidParam";
    break;
  default:
    break;
  }

  return "unknown result code";
}

int asyncSocketServer(IasMediaTransportAvb::IasAvbStreamHandler *avbStreamHandler, unsigned short port){
  try
  {
    //unsigned short port = DEFAULT_PORT;
    boost::asio::io_service io_service;
    AvbStreamHandlerSocketIpc::server server(io_service, port, avbStreamHandler, serverCmdTbl,(sizeof serverCmdTbl)/(sizeof serverCmdTbl[0]));
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}

int main(int argc, char* argv[])
{
  IasMediaTransportAvb::IasAvbStreamHandler* avbStreamHandler = NULL;
  // TODO REPLACE std::shared_ptr<IasAvbControlImp> avbController;
  DltLogLevelType dltLogLevel = DLT_LOG_DEFAULT;
  IasAvbProcessingResult result = eIasAvbProcOK;
  int32_t c = 0;
  int32_t daemonize = 0;
  int32_t runSetup = 1;
  int32_t startIpc = 1;
  int32_t setupArgc = 0;
  char** setupArgv = NULL;
  int32_t optIdx = 0;
  bool showUsage = false;
  bool localPrint = false;
  volatile int32_t debugSpin = 0;
  std::string configName = "pluginias-media_transport-avb_configuration_reference.so";
  std::string commandline;
  unsigned short port = DEFAULT_PORT;

  for ( size_t i = 1; i < (size_t) argc; i++)
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
    { "nosetup",    no_argument, &runSetup, 0 },
    { "noipc",      no_argument, &startIpc, 0 },
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
    c = getopt_long(argc, argv, "+qdv::cs:p:I:", options, &optIdx);

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
      case 'p':
      {
        if (NULL != optarg)
        {
          port = (short) strtoul(optarg, NULL, 10);
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
        "\t-p [port number]       port number for socket ipc"
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
        "Copyright (c) 2013-2017, Intel Corporation\n"
        "All rights reserved.\n"
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
      case -1: dltLogLevel = DLT_LOG_OFF; break;
      case  0: dltLogLevel = DLT_LOG_WARN; break;
      case  1: dltLogLevel = DLT_LOG_INFO; break;
#if IAS_PREPRODUCTION_SW
      case  2: dltLogLevel = DLT_LOG_DEBUG; break;
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
       * difficult to start a streamhandler process in gdb since it will loasyncGetAvbStreamInfose
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

    //Thread to run socket server io_service
    std::thread threadSocketServer (asyncSocketServer, avbStreamHandler, port);
    threadSocketServer.detach();

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
// EOF
