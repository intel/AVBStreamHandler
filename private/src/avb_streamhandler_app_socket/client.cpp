/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**

 * file     client.cpp
 * @brief   The client application for users to send queries to the avb_streamhandler,
 *          modified from avb_streamhandler_client_app
 * @details
 *          To add new commands, add into the 3 sections
 *          -Declare new command class
 *          -Add to command table
 *          -Define its printusage(), validaterequest() and receive() function
 * @date    2018
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>
#include <iomanip>
#include <stdint.h>
#include "avb_streamhandler_app_socket/IasAvbStreamHandlerSocketIpc.hpp"
#include <dlt/dlt_cpp_extension.hpp>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>


#ifndef ARRAY_LEN
#define ARRAY_LEN(a) ((sizeof a)/(sizeof a[0]))
#endif

int32_t verbosity = 0;

#define INVALID_STREAM_DIR   0xFFFFFFFFu
#define INVALID_LC_STREAM_ID 0xFFFFu              // invalid local stream id
#define INVALID_NW_STREAM_ID 0xFFFFFFFFFFFFFFFFu  // invalid network stream id
#define INVALID_MAC_ADDRESS  0xFFFFFFFFFFFFFFFFu
#define INVALID_CHANNEL_IDX  0xFFFFu
#define VERSION_STRING 0x01

std::string hostip = "127.0.0.1";
std::string hostport = "81";

std::string instanceID = "CLIENT_DEMO_APPLICATION";
std::string appName = "avb_streamhandler_client_app_socket";

//Function prototype
static void printUsage(std::string cmdName);

/*******************************************************
  Client-side io_service
*******************************************************/
namespace AvbStreamHandlerSocketIpc {

class client
{
public:
  client(boost::asio::io_service& io_service,
      const std::string& host, const std::string& service,
      Command *selectedCmdPtr,
      int argc,
      AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
      : connection_(io_service)
  {
    selectedCmdPtr_ = selectedCmdPtr;
    argc_ = argc;
    requestSocketIpcStruct_=*userInputStruct;
    requestSocketIpcStruct_.command = selectedCmdPtr_->getName();

    boost::asio::ip::tcp::resolver resolver(io_service);
    boost::asio::ip::tcp::resolver::query query(host, service);
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

    boost::asio::async_connect(connection_.socket(), endpoint_iterator,
        boost::bind(&client::handle_connect, this,
        boost::asio::placeholders::error));

  }

  void handle_connect(const boost::system::error_code& e)
  {
    if (!e)
    {
      connection_.async_write(requestSocketIpcStruct_,
          boost::bind(&client::handle_write, this,
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
    connection_.async_read(responseSocketIpcStruct_,
        boost::bind(&client::handle_read, this,
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
      selectedCmdPtr_->receive(&responseSocketIpcStruct_);
    }
    else
    {
      std::cerr << e.message() << std::endl;
    }
    // Since we are not starting a new operation the io_service will run out of
    // work to do and the CLIENT io_service WILL EXIT.
  }

private:
  int argc_;
  connection connection_;
  requestSocketIpc requestSocketIpcStruct_;
  responseSocketIpc responseSocketIpcStruct_;

  Command* selectedCmdPtr_;
  };

} //namespace AvbStreamHandlerSocketIpc

///  Command derived class macro
#define DECLARE_COMMAND(name, desc, argc) \
class name : public AvbStreamHandlerSocketIpc::Command \
{ \
public: \
  name() : AvbStreamHandlerSocketIpc::Command (#name, desc, argc) {} \
  void printUsage (void);\
  bool validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc*);\
  void receive (AvbStreamHandlerSocketIpc::responseSocketIpc*);\
};\
name name##Obj;

//Note: this table is used to cross check minimum default values required. not total arguments this app is able to accept
//              command name                  command description                                            minimum number of arguments
DECLARE_COMMAND(GetAvbStreamInfo,             "Retrieves information about all AVB streams currently created.",           0)
DECLARE_COMMAND(GetLocalStreamInfo,           "Retrieves information about all local streams currently created.",         0)
DECLARE_COMMAND(CreateTransmitAvbAudioStream, "Creates an AVB transmit stream.",                                          2)
DECLARE_COMMAND(CreateReceiveAudioStream,     "Creates an AVB receive stream.",                                           2)
DECLARE_COMMAND(DestroyStream,                "Destroys a previously created AVB stream.",                                1)
DECLARE_COMMAND(SetStreamActive,              "Sets an AVB transmit stream to active or inactive.",                       1)
DECLARE_COMMAND(CreateAlsaStream,             "Creates a local audio stream connected to a virtual/hardware ALSA device.",2)
DECLARE_COMMAND(DestroyLocalStream,           "Destroys a ALSA stream.",                                                  1)
DECLARE_COMMAND(ConnectStreams,               "Connects an AVB stream and a local audio stream.",                         2)
DECLARE_COMMAND(DisconnectStreams,            "Disconnects an already connected AVB stream from the local audio stream.", 1)
DECLARE_COMMAND(SetChannelLayout,             "Sets the layout of the audio data within the stream.",                     1)
DECLARE_COMMAND(Monitor,                      "Waits for events to receive until aborted by Ctrl-C.",                     0)  //TODO - need a new client class to do this
DECLARE_COMMAND(CreateTransmitAvbVideoStream, "Creates an AVB video transmit stream.",                                    2)
DECLARE_COMMAND(CreateReceiveVideoStream,     "Creates an AVB video receive stream.",                                     2)
DECLARE_COMMAND(CreateLocalVideoStream,       "Creates a local video stream that connects to applications.",              2)  //TODO replace ufipc with vstreaming?
DECLARE_COMMAND(CreateTestToneStream,         "Creates a local stream that produces test tones on its audio channels.",   0)
DECLARE_COMMAND(SetTestToneParams,            "Changes parameters of test tone generators.",                              0)
DECLARE_COMMAND(SuspendStreamhandler,         "Suspends AVB streamhandler.",                                              0)  //TODO reimplement suspendstreamhandler

AvbStreamHandlerSocketIpc::Command* cmdTbl[] =
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
  Command's receive functions
*******************************************************/
/// GetAvbStreamInfo command
void GetAvbStreamInfo::printUsage ()
{
  std::cout << "\t syntax: " << appName << " GetAvbStreamInfo\n";
}

bool GetAvbStreamInfo::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
	(void) userInputStruct;
	return true;
}

void GetAvbStreamInfo::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
  std::cout << "  avbStreamInfo: \n" << receivedResponse->avbStreamInfo << "\n";
}

/// GetLocalStreamInfo command
void GetLocalStreamInfo::printUsage ()
{
  std::cout << "\t syntax: " << appName << " GetLocalStreamInfo\n";
}

bool GetLocalStreamInfo::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
	(void) userInputStruct;
	return true;
}

void GetLocalStreamInfo::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
  std::cout << "  avbStreamInfo: \n" << receivedResponse->avbStreamInfo << "\n";
}


/// CreateTransmitAvbAudioStream command
void CreateTransmitAvbAudioStream::printUsage ()
{
  std::cout <<
    " syntax: " << appName << " CreateTransmitAvbAudioStream -n <streamId> -m <dmac> -q <srClass> -c <maxNumCh> -r <sampleFreq> "
    "-f <format> -C <clockId> -M <assignMode> -a <active>\n\n"
    << std::left << std::setw(20) << "\t\t <srClass>"    << " : " << "stream reservation class (H = high, L = low)\n"
    << std::left << std::setw(20) << "\t\t"              << "   " << "(default = H)\n"
    << std::left << std::setw(20) << "\t\t <maxNumCh>"   << " : " << "maximum number of audio channels the stream has to support \n"
    << std::left << std::setw(20) << "\t\t"              << "   " << "(default = 2)\n"
    << std::left << std::setw(20) << "\t\t <sampleFreq>" << " : " << "sample frequency in Hertz\n"
    << std::left << std::setw(20) << "\t\t"              << "   " << "(default = 48000)\n"
    << std::left << std::setw(20) << "\t\t <format>"     << " : " << "format of the audio (SAF16 == 1)\n"
    << std::left << std::setw(20) << "\t\t"              << "   " << "(default = 1)\n"
    << std::left << std::setw(20) << "\t\t <clockId>"    << " : " << "clockId Id of the clock domain to be used by the stream\n"
    << std::left << std::setw(20) << "\t\t"              << "   " << "cIasAvbPtpClockDomainId=0x00, cIasAvbHwCaptureClockDomainId=0x10, cIasAvbJackClockDomainId=0x20\n"
    << std::left << std::setw(20) << "\t\t"              << "   " << "(default = 0x00)\n"
    << std::left << std::setw(20) << "\t\t <assignMode>" << " : " << "assignMode controls the definition of streamId and destination MAC (static == 0)\n"
    << std::left << std::setw(20) << "\t\t"              << "   " << "(default = 0)\n"
    << std::left << std::setw(20) << "\t\t <streamId>"   << " : " << "streamId, if assignMode indicates manual configuration\n"
    << std::left << std::setw(20) << "\t\t"              << "   " << "(default = none, specify value)\n"
    << std::left << std::setw(20) << "\t\t <dmac>"       << " : " << "MAC address, if assignMode indicates manual configuration. The 16 most significant bits are unused\n"
    << std::left << std::setw(20) << "\t\t"              << "   " << "(default = none, specify value)\n"
    << std::left << std::setw(20) << "\t\t <active>"     << " : " << "if set to '1', stream is activated immediately\n"
    << std::left << std::setw(20) << "\t\t"              << "   " << "(default = 1)\n"
    "\n\t NOTE: streamId, dmac and clockId has to be specified as a HEX (0x12345...) representation!\n\n"

    "\t Alternative call methods:\n"
    "\t\t CreateTransmitAvbAudioStream --srclass H --channels 2 --rate 48000 --format 1 --clock 0x00 --mode 0 --network_id <streamId> --dmac <dmac> --active 1\n\n"

    "\t Mandatory arguments using default values only:\n"
    "\t\t CreateTransmitAvbAudioStream --network_id <streamId> --dmac <dmac>\n\n"
    "\t\t CreateTransmitAvbAudioStream -n <streamId> -m <dmac>\n\n"
    "\t See " << appName << " --help for the details of the option names\n";
}

bool CreateTransmitAvbAudioStream::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
  //std::cout << "ERROR: Specify the <streamId> and the <dmac> arguments.\n";
  if (userInputStruct->networkStreamId == INVALID_NW_STREAM_ID) return false;
  if (userInputStruct->dmac == INVALID_MAC_ADDRESS) return false;
  return true;
}

void CreateTransmitAvbAudioStream::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
  std::cout << "  StreamId 0x" << std::hex << receivedResponse->oStreamId << std::endl;
}

/// CreateReceiveAudioStream command
void CreateReceiveAudioStream::printUsage ()
{
  std::cout <<
    "\t syntax: " << appName << " CreateReceiveAudioStream -q <srClass> -c <maxNumCh> -r <sampleFreq> -n <streamId> -m <dmac>\n\n"
    << std::left << std::setw(20) << "\t\t <srClass>"    << " : " << "stream reservation class (H = high, L = low)\n"
    << std::left << std::setw(20) << "\t\t"                                  << "(default = H)\n"
    << std::left << std::setw(20) << "\t\t <maxNumCh>"   << " : " << "maximum number of channels within the stream\n"
    << std::left << std::setw(20) << "\t\t"                                  << "(default = 2)\n"
    << std::left << std::setw(20) << "\t\t <sampleFreq>" << " : " << "sample frequency in Hertz\n"
    << std::left << std::setw(20) << "\t\t"                                  << "(default = 48000)\n"
    << std::left << std::setw(20) << "\t\t <streamId>"   << " : " << "ID of the AVB stream\n"
    << std::left << std::setw(20) << "\t\t"                                  << "(default = none, specify value)\n"
    << std::left << std::setw(20) << "\t\t <dmac>"       << " : " << "MAC address to listen on\n"
    << std::left << std::setw(20) << "\t\t"                                  << "(default = none, specify value)\n"
    "\n\t NOTE: streamId and dmac has to be specified as a HEX (0x12345...) representation!\n\n"

    "\t Alternative call methods:\n"
    "\t\t CreateReceiveAudioStream --srclass H --channels 2 --rate 48000 --network_id <streamId> --dmac <dmac>\n\n"

    "\t Mandatory arguments using default values only:\n"
    "\t\t CreateReceiveAudioStream --network_id <streamId> --dmac <dmac>\n"
    "\t\t CreateReceiveAudioStream -n <streamId> -m <dmac>\n\n"
    "\t See " << appName << " --help for the details of the option names\n";
}

bool CreateReceiveAudioStream::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
  if (userInputStruct->networkStreamId == INVALID_NW_STREAM_ID) return false;
  if (userInputStruct->dmac == INVALID_MAC_ADDRESS) return false;
   return true;
}


void CreateReceiveAudioStream::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
  std::cout << "  StreamId 0x" << std::hex << receivedResponse->oStreamId << std::endl;
}

/// DestroyStream command
void DestroyStream::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " DestroyStream -n <streamId>\n\n"
      << std::left << std::setw(20) << "\t\t <streamId>" << " : " << "ID of the AVB stream that should be destroyed\n"
      << std::left << std::setw(20) << "\t\t"            << "   " << "(default = none, specify value)\n"
      "\n\t NOTE: streamId has to be specified as a HEX (0x12345...) representation!\n"

      "\t Alternative call methods:\n"
      "\t\t DestroyStream --network_id <streamId>\n";
}

bool DestroyStream::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct){
  if (userInputStruct->networkStreamId == INVALID_NW_STREAM_ID) return false;
  return true;
}


void DestroyStream::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
}


/// SetStreamActive command
void SetStreamActive::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " SetStreamActive -q <streamId> -a <active>\n\n"
      << std::left << std::setw(20) << "\t\t <streamId>"   << " : " << "ID of the AVB stream\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = none, specify value)\n"
      << std::left << std::setw(20) << "\t\t <active>"     << " : " << "'1' if the stream shall be activated, '0' if the stream shall be deactivated\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 1)\n"
      "\n\t NOTE: streamId has to be specified as a HEX (0x12345...) representation!\n\n"

      "\t Alternative call methods:\n"
      "\t\t SetStreamActive --network_id <streamId> --active 1\n\n"

      "\t Use default values:\n"
      "\t\t SetStreamActive --network_id <streamId>\n\n"
      "\t\t SetStreamActive -n <streamId>\n\n"
      "\t See " << appName << " --help for the details of the option names\n";
}

bool SetStreamActive::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct){
  //std::cout << "ERROR: Specify the <streamId> argument.\n";
  if (userInputStruct->networkStreamId == INVALID_NW_STREAM_ID) return false;
  return true;
}

void SetStreamActive::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
}


/// CreateAlsaStream command
void CreateAlsaStream::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " CreateAlsaStream -d <direction> -c <numberOfCh> -r <sampleFreq> -y <alsaDeviceType> -Y <sampleFreqASRC>"
      "-r <format> -C <clockId> -p <periodSize> -N <numPeriods> -L <chLayout> -s <hasSideCh> -D <deviceName>\n\n"
      << std::left << std::setw(20) << "\t\t <direction>"  << " : " << "specifies whether this is a transmit (0) or receive (1) stream\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = none, specify value)\n"
      << std::left << std::setw(20) << "\t\t <numberOfCh>" << " : " << "Number of channels within the stream\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 2)\n"
      << std::left << std::setw(20) << "\t\t <sampleFreq>" << " : " << "sample frequency in Hertz\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 48000)\n"

      << std::left << std::setw(20) << "\t\t <format>"     << " : " << "format of the audio (SAF16 == 1)\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 1)\n"

      << std::left << std::setw(20) << "\t\t <clockId>"    << " : " << "ID of the clock domain the stream is driven from\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 0x00)\n"
      << std::left << std::setw(20) << "\t\t <periodSize>" << " : " << "ALSA period size (number of ALSA frames, e.g. 256)\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 256)\n"
      << std::left << std::setw(20) << "\t\t <numPeriods>" << " : " << "The size of the IPC buffer in periods\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 3)\n"
      << std::left << std::setw(20) << "\t\t <chLayout>"   << " : " << "Application specific value indicating layout of audio data within the channel;\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 0)\n"
      << std::left << std::setw(20) << "\t\t"              << " : " << "depending on the setting of the compatibility.audio option, only the lower 4 bits of the layout argument are valid.\n"
      << std::left << std::setw(20) << "\t\t <hasSideCh>"  << "   " << "if set to '1' use last audio channel for channel info\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 0)\n"
      << std::left << std::setw(20) << "\t\t <deviceName>" << " : " << "name of ALSA device as configured in asound.conf\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = none, specify value)\n"
      << std::left << std::setw(20) << "\t\t <alsaDeviceType>" << " : " << "name of ALSA device as configured in asound.conf\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default 0 = virtual, 1 = hardware)\n"
      << std::left << std::setw(20) << "\t\t <sampleFreqASRC>"   << " : " << "ALSA ASRC frequency in Hertz \n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 48000, specify value)\n"
      "\n\t NOTE: clockId has to be specified as a HEX (0x12345...) representation!\n\n"

      "\t Alternative call methods:\n"
      "\t\t CreateAlsaStream --direction <direction> --channels 2 --rate 48000 --format 1 --clock 0x00 --period_size 256 --numPeriods 3 --ch_layout 0 --has_sidech 0 --device \"AVB_Alsa_2ch_p0 --device_type 1 --asrc_freq 48000\"\n\n"

      "\t Use default values:\n"
      "\t\t CreateAlsaStream --direction <direction> --device <deviceName>\n\n"
      "\t\t CreateAlsaStream -d <direction> -D <deviceName>\n\n"
      "\t See " << appName << " --help for the details of the option names\n";
}

bool CreateAlsaStream::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct){
  //std::cout << "ERROR: Specify the <direction> and the <deviceName> argument.\n";
  if (userInputStruct->direction == INVALID_STREAM_DIR) return false;
  if (userInputStruct->deviceName.empty()) return false;

  return true;
}

void CreateAlsaStream::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
  std::cout << "  StreamId 0x" << receivedResponse->oStreamId << std::endl;
}

/// DestroyLocalStream command
void DestroyLocalStream::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " DestroyLocalStream -n <streamId>\n\n"
      << std::left << std::setw(20) << "\t\t <streamId>"  << " : " << "ID of the local stream to be destroyed\n"
      << std::left << std::setw(20) << "\t\t"             << "   " << "(default = none, specify value)\n"
      "\n\t NOTE: streamId has to be specified as a HEX (0x12345...) representation!\n"

      "\t Alternative call methods:\n"
      "\t\t DestroyLocalStream --network_id <networkStreamId>";
}

bool DestroyLocalStream::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
  //std::cout << "ERROR: Specify the <streamId> argument.\n";
  if (userInputStruct->localStreamId == INVALID_LC_STREAM_ID) return false;
  return true;
}

void DestroyLocalStream::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
}

/// ConnectStreams command
void ConnectStreams::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " ConnectStreams -n <networkStreamId> -l <localStreamId>\n\n"
      << std::left << std::setw(20) << "\t\t <networkStreamId>" << " : " << "ID of the AVB stream\n"
      << std::left << std::setw(20) << "\t\t"                   << "   " << "(default = none, specify value)\n"
      << std::left << std::setw(20) << "\t\t <localStreamId>"   << " : " << "ID of the local stream\n"
      << std::left << std::setw(20) << "\t\t"                   << "   " << "(default = none, specify value)\n"
      "\n\t NOTE: streamIds has to be specified as a HEX (0x12345...) representation!\n\n"

      "\t Alternative call methods:\n"
      "\t\t ConnectStreams --network_id <networkStreamId> --local_id <localStreamId>\n\n";
}

bool ConnectStreams::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
  //std::cout << "ERROR: Specify the <networkStreamId> and the <localStreamId> arguments.\n";
  if (userInputStruct->localStreamId == INVALID_LC_STREAM_ID) return false;
  if (userInputStruct->networkStreamId == INVALID_NW_STREAM_ID) return false;
  return true;
}

void ConnectStreams::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
}

/// DisconnectStreams command
void DisconnectStreams::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " DisconnectStreams -n <networkStreamId>\n\n"
      << std::left << std::setw(20) << "\t\t <networkStreamId>" << " : " << "ID of the AVB stream\n"
      << std::left << std::setw(20) << "\t\t"                   << "   " << "(default = none, specify value)\n"
      "\n\t NOTE: streamId has to be specified as a HEX (0x12345...) representation!\n"

      "\t Alternative call methods:\n"
      "\t\t DisconnectStreams --network_id <networkStreamId>\n\n";
}

bool DisconnectStreams::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
  //std::cout << "ERROR: Specify the <networkStreamId> argument.\n";
  if (userInputStruct->networkStreamId == INVALID_NW_STREAM_ID) return false;
  return true;
}

void DisconnectStreams::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
}

/// SetChannelLayout command
void SetChannelLayout::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " SetChannelLayout <localStreamId> <channelLayout>\n\n"
      << std::left << std::setw(20) << "\t\t <localStreamId>" << " : " << "ID of the local stream\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = none, specify value)\n"
      << std::left << std::setw(20) << "\t\t <chLayout>"      << " : " << "Application specific value indicating layout of audio data within the channel;\n"
      << std::left << std::setw(20) << "\t\t"                 << " : " << "depending on the setting of the compatibility.audio option, only the lower 4 bits of the layout argument are valid.\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 0)\n"
      "\n\t NOTE: streamId has to be specified as a HEX (0x12345...) representation!\n\n"

      "\t Alternative call methods:\n"
      "\t\t SetChannelLayout --local_id <localStreamId> --ch_layout 0\n\n"

      "\t Use default values:\n"
      "\t\t SetChannelLayout --local_id <localStreamId> \n\n"
      "\t\t SetChannelLayout -l <localStreamId> \n\n"
      "\t See " << appName << " --help for the details of the option names\n";
}

bool SetChannelLayout::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
  //std::cout << "ERROR: Specify the <networkStreamId> argument.\n";
  if (userInputStruct->localStreamId == INVALID_LC_STREAM_ID) return false;
  return true;
}

void SetChannelLayout::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
}

/// Monitor command
void Monitor::printUsage ()
{
    std::cout << "\t syntax: " << appName << " Monitor\n";
}

bool Monitor::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
	(void) userInputStruct;
	return true;
}

void Monitor::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
}

/// CreateTransmitAvbVideoStream command
void CreateTransmitAvbVideoStream::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " CreateTransmitAvbVideoStream -q <srClass> -R <maxPacketRate> -S <maxPacketSize> "
      "-f <format> -C <clockId> -M <assignMode> -n <streamId> -m <dmac> -a <active>\n\n"
      << std::left << std::setw(20) << "\t\t <srClass>"       << " : " << "stream reservation class (H = high, L = low)\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = H)\n"
      << std::left << std::setw(20) << "\t\t <maxPacketRate>" << " : " << "maximum number of packets that will be transmitted per second\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 4000)\n"
      << std::left << std::setw(20) << "\t\t <maxPacketSize>" << " : " << "maximum size of a packet in bytes\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 1460)\n"
      << std::left << std::setw(20) << "\t\t <format>"        << " : " << "format of the video stream (RTP == 1)\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 1)\n"
      << std::left << std::setw(20) << "\t\t <clockId>"       << " : " << "clockId Id of the clock domain to be used by the stream\n"
      << std::left << std::setw(20) << "\t\t"                 << " : "    << "cIasAvbPtpClockDomainId=0x00, cIasAvbHwCaptureClockDomainId=0x10, cIasAvbJackClockDomainId=0x20\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 0x00)\n"
      << std::left << std::setw(20) << "\t\t <assignMode>"    << " : " << "assignMode controls the definition of streamId and destination MAC (static == 0)\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 0)\n"
      << std::left << std::setw(20) << "\t\t <streamId>"      << " : " << "streamId, if assignMode indicates manual configuration\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = none, specify value)\n"
      << std::left << std::setw(20) << "\t\t <dmac>"          << " : " << "MAC address, if assignMode indicates manual configuration. The 16 most significant bits are unused\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = none, specify value)\n"
      << std::left << std::setw(20) << "\t\t <active>"        << " : " << "if set to '1', stream is activated immediately\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 1)\n"
      "\n\t NOTE: streamId, dmac and clockId has to be specified as a HEX (0x12345...) representation!\n\n"

      "\t Alternative call methods:\n"
      "\t\t CreateTransmitAvbVideoStream --srclass H --packet_rate 4000 --packet_size 1460 --format 1 --clock 0x00 --mode 0 --network_id <streamId> --dmac <dmac> --active 1\n\n"

      "\t Use default values:\n"
      "\t\t CreateTransmitAvbVideoStream --network_id <streamId> --dmac <dmac>\n\n"
      "\t\t CreateTransmitAvbVideoStream -n <streamId> -m <dmac>\n\n"
      "\t See " << appName << " --help for the details of the option names\n";
}

bool CreateTransmitAvbVideoStream::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
  //std::cout << "ERROR: Specify the <streamId> and the <dmac> arguments.\n";
  if (userInputStruct->networkStreamId == INVALID_NW_STREAM_ID) return false;
  if (userInputStruct->dmac == INVALID_MAC_ADDRESS) return false;
  return true;
}

void CreateTransmitAvbVideoStream::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
  std::cout << "  StreamId 0x" << std::hex << receivedResponse->oStreamId << std::endl;
}

/// CreateReceiveVideoStream command
void CreateReceiveVideoStream::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " CreateReceiveVideoStream -q <srClass> -P <maxPacketRate> -S <maxPacketSize> -f <format> -n <streamId> -m <dmac>\n\n"
      << std::left << std::setw(20) << "\t\t <srClass>"       << " : " << "stream reservation class (H = high, L = low)\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = H)\n"
      << std::left << std::setw(20) << "\t\t <maxPacketRate>" << " : " << "maximum number of packets that will be transmitted per second\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 4000)\n"
      << std::left << std::setw(20) << "\t\t <maxPacketSize>" << " : " << "maximum size of a packet in bytes\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 1460)\n"
      << std::left << std::setw(20) << "\t\t <format>"        << " : " << "format of the video stream (RTP == 1)\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 1)\n"
      << std::left << std::setw(20) << "\t\t <streamId>"      << " : " << "ID of the AVB stream\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = none, specify value)\n"
      << std::left << std::setw(20) << "\t\t <dmac>"          << " : " << "MAC address to listen on\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = none, specify value)\n"
      "\n\t NOTE: streamId and dmac has to be specified as a HEX (0x12345...) representation!\n\n"

      "\t Alternative call methods:\n"
      "\t\t CreateReceiveVideoStream --srclass H --packet_rate 4000 --packet_size 1460 --format 1 --network_id <streamId> --dmac <dmac>\n\n"

      "\t Use default values:\n"
      "\t\t CreateReceiveVideoStream --network_id <streamId> --dmac <dmac>\n\n"
      "\t\t CreateReceiveVideoStream -n <streamId> -m <dmac>\n\n"
      "\t See " << appName << " --help for the details of the option names\n";
}

bool CreateReceiveVideoStream::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
  //std::cout << "ERROR: Specify the <streamId> and the <dmac> arguments.\n";
  if (userInputStruct->networkStreamId == INVALID_NW_STREAM_ID) return false;
  if (userInputStruct->dmac == INVALID_MAC_ADDRESS) return false;
  return true;
}

void CreateReceiveVideoStream::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
  std::cout << "  StreamId 0x" << std::hex << receivedResponse->oStreamId << std::endl;
}

/// CreateLocalVideoStream command
//TODO: keeping ufipc code for reference, will delete when have replacement.
void CreateLocalVideoStream::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " CreateLocalVideoStream -d <direction> -R <maxPacketRate> -S <maxPacketSize> -f <format> -i <ufipcName>\n\n"
      << std::left << std::setw(20) << "\t\t <direction>"     << " : " << "specifies whether this is a transmit (0) or receive (1) stream \n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = none, specify value)\n"
      << std::left << std::setw(20) << "\t\t <maxPacketRate>" << " : " << "maximum number of packets that will be transmitted per second\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 4000)\n"
      << std::left << std::setw(20) << "\t\t <maxPacketSize>" << " : " << "maximum size of a packet in bytes\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 1460)\n"
      << std::left << std::setw(20) << "\t\t <format>"        << " : " << "format of the video stream (RTP == 1)\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = 1)\n"
      << std::left << std::setw(20) << "\t\t <ufipcName>"     << " : " << "UF-IPC channel name dedicated to the local video stream\n"
      << std::left << std::setw(20) << "\t\t"                 << "   " << "(default = none, specify value)\n\n"

      "\t Alternative call methods:\n"
      "\t\t CreateLocalVideoStream --direction <direction> --packet_rate 4000 --packet_size 1460 --format 1 --uf_ipc <ufipcName>\n\n"

      "\t Use default values:\n"
      "\t\t CreateLocalVideoStream --direction <direction> --uf_ipc <ufipcName>\n\n"
      "\t\t CreateLocalVideoStream --direction <direction> --uf_ipc <ufipcName>\n\n"
      "\t See " << appName << " --help for the details of the option names\n";
}

bool CreateLocalVideoStream::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
  //std::cout << "ERROR: Specify the <direction> and the <ufipcName> arguments.\n";
  if (userInputStruct->direction == INVALID_STREAM_DIR) return false;
  //if (userInputStruct->ufipcName.empty()) return false;
  return true;
}

void CreateLocalVideoStream::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
  std::cout << "  StreamId 0x" << std::hex << receivedResponse->oStreamId << std::endl;
}

/// CreateTestToneStream command
void CreateTestToneStream::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " CreateTestToneStream -n <numberOfCh> -r <sampleFreq> -f <format> -L <chLayout>\n\n"
      << std::left << std::setw(20) << "\t\t <numberOfCh>" << " : " << "Number of channels within the stream\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 2)\n"
      << std::left << std::setw(20) << "\t\t <sampleFreq>" << " : " << "sample frequency in Hertz\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 48000)\n"
      << std::left << std::setw(20) << "\t\t <format>"     << " : " << "format of the audio (SAFFloat == 4)\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 4)\n"
      << std::left << std::setw(20) << "\t\t <chLayout>"   << " : " << "Application specific value indicating layout of audio data within the channel;\n"
      << std::left << std::setw(20) << "\t\t "             << " : " << "depending on the setting of the compatibility.audio option, only the lower 4 bits of the layout argument are valid.\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 0)\n\n"
      "\t Use the SetTestToneParams command to modify the test tones after creation.\n"
      "\t Currently, only SafFloat(format = 4) is supported.\n\n"

      "\t Alternative call methods:\n"
      "\t\t CreateTestToneStream --channels 2 --rate 48000 --format 4 --ch_layout 0\n\n"

      "\t Use default values:\n"
      "\t\t CreateTestToneStream\n\n"
      "\t See " << appName << " --help for the details of the option names\n";
}

bool CreateTestToneStream::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
        (void) userInputStruct;
        return true;
}

void CreateTestToneStream::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
  std::cout << "  StreamId 0x" << std::hex << receivedResponse->oStreamId << std::endl;
}

/// SetTestToneParams command
void SetTestToneParams::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " SetTestToneParams -l <streamId> -x <channelIdx> "
                                  "-F <signalFrequency> -A <amplitude> -w <waveform> -u <userParam>\n\n"
      << std::left << std::setw(20) << "\t\t <streamId>"        << " : " << "ID of the local audio stream\n"
      << std::left << std::setw(20) << "\t\t"                   << "   " << "(default = none, specify value)\n"
      << std::left << std::setw(20) << "\t\t <channelIdx>"      << " : " << "index of audio channel to modify\n"
      << std::left << std::setw(20) << "\t\t"                   << "   " << "(default = none, specify value)\n"
      << std::left << std::setw(20) << "\t\t <signalFrequency>" << " : " << "frequency of the tone to be generated in Hz (<=sampleFreq/2)\n"
      << std::left << std::setw(20) << "\t\t"                   << "   " << "(default = 997)\n"
      << std::left << std::setw(20) << "\t\t <amplitude>"       << " : " << "level/amplitude of the tone in dBFS (0 = full scale, -6 = half, etc.)\n"
      << std::left << std::setw(20) << "\t\t"                   << "   " << "(default = 0)\n"
      << std::left << std::setw(20) << "\t\t <waveform>"        << " : " << "wave form selection\n"
      << std::left << std::setw(20) << "\t\t "                  << " : " << "Sine=0x00, Pulse=1, Sawtooth=2, File=3\n"
      << std::left << std::setw(20) << "\t\t"                   << "   " << "Sine == 0, \n"
      << std::left << std::setw(20) << "\t\t"                   << "   " << "(default = 0)\n"
      << std::left << std::setw(20) << "\t\t <userParam>"       << " : " << "additional param to modify wave generation, depending on mode\n"
      << std::left << std::setw(20) << "\t\t"                   << "   " << "(default = -1|50 for sawtooth|pulse)\n"
      "\n\t NOTE: streamId has to be specified as a HEX (0x12345...) representation!\n\n"

      "\t Alternative call methods:\n"
      "\t\t SetTestToneParams --local_id <streamId> --ch_index <channelIdx> --signal_freq 0 --amplitude 0 --user_param 0\n\n"

      "\t Use default values:\n"
      "\t\t SetTestToneParams --local_id <streamId> --ch_index <channelIdx>\n\n"
      "\t\t SetTestToneParams -l <streamId> -x <channelIdx>\n\n"
      "\t See " << appName << " --help for the details of the option names\n";
}

bool SetTestToneParams::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
  //std::cout << "ERROR: Specify the <streamId> argument.\n";
  if (userInputStruct->channelIdx == INVALID_LC_STREAM_ID) return false;
  if (userInputStruct->localStreamId == INVALID_LC_STREAM_ID) return false;
  return true;
}

void SetTestToneParams::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
}

/// SuspendStreamhandler command - NOT IMPLEMENTED
void SuspendStreamhandler::printUsage ()
{
  std::cout <<
      "\t syntax: " << appName << " SuspendStreamhandler <action>\n\n"
      << std::left << std::setw(20) << "\t\t <action>"     << " : " << "'1' prepare for suspend, '0' resume\n"
      << std::left << std::setw(20) << "\t\t"                                  << "(default = 1)\n\n"

      "\t Alternative call methods:\n"
      "\t\t SuspendStreamhandler --suspend <action>\n\n"
      "\t See " << appName << " --help for the details of the option names\n";

}

bool SuspendStreamhandler::validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc *userInputStruct)
{
        (void) userInputStruct;
        return true;
}

void SuspendStreamhandler::receive (AvbStreamHandlerSocketIpc::responseSocketIpc *receivedResponse)
{
  std::cout << "The received response is: \n";
  std::cout << "  Command: " << receivedResponse->command << "\n";
  std::cout << "  Result: " << receivedResponse->result << "\n";
}

/*******************************************************
  Helper functions and main
*******************************************************/
/// Main's generic app printusage
static void printUsage(std::string cmdName)
{
  uint32_t i;

  if (cmdName.empty())
  {
    // show overview
    std::cout <<
        "\n"
        "AVB Streamhandler Client Application Version " << VERSION_STRING << "\n"
        "\n"
        "Usage: " << appName << " <command> <arg1> <arg2> <arg3> ...\n"
        "\n"
        "Commands:"
        "\n"
    << std::endl;

    for (i = 0; i < ARRAY_LEN(cmdTbl); i++)
    {
      std::cout <<
          "\t" << std::left << std::setw(30) << cmdTbl[i]->getName() << " - " << cmdTbl[i]->getDesc()
      << std::endl;
    }

    std::cout << std::endl;

    std::cout <<
        "Options:\n"
        "\t" << std::left << std::setw(18) << "-h, --help"       << "help \n"
        "\t" << std::left << std::setw(18) << "-o, --hostip"     << "host ip address (default 127.0.0.1)\n"
        "\t" << std::left << std::setw(18) << "-O, --hostport"   << "host port number (default 81) \n"
        "\t" << std::left << std::setw(18) << "-q, --srclass"    << "stream reservation class (H = high, L = low) (default H)\n"
        "\t" << std::left << std::setw(18) << "-c, --channels"   << "number of channels (default 2)\n"
        "\t" << std::left << std::setw(18) << "-r, --rate"       << "sample frequency (default 48000) \n"
        "\t" << std::left << std::setw(18) << "-f, --format"     << "format of the audio/video (default audio:SAF16=1/video:RTP=1) \n"
        "\t" << std::left << std::setw(18) << "-C, --clock"      << "clockId Id of the clock domain (default cIasAvbPtpClockDomainId=0x00) \n"
        "\t" << std::left << std::setw(18) << "-M, --mode"       << "assignMode controls the definition of streamId and destination MAC (default static=0) \n"
        "\t" << std::left << std::setw(18) << "-n, --network_id" << "network Audio Stream ID (default none)\n"
        "\t" << std::left << std::setw(18) << "-l, --local_id"   << "local Audio Stream ID (default none)\n"
        "\t" << std::left << std::setw(18) << "-m, --dmac"       << "MAC address (default none)\n"
        "\t" << std::left << std::setw(18) << "-a, --active"     << "activate Network Stream immediately (default 0)\n"
        "\t" << std::left << std::setw(18) << "-d, --direction"  << "whether a transmit (0) or receive (1) stream (default none)\n"
        "\t" << std::left << std::setw(18) << "-D, --device"     << "name of ALSA device (default none)\n"
        "\t" << std::left << std::setw(18) << "-y, --device_type"<< "ALSA device type (default virtual)\n"
        "\t" << std::left << std::setw(18) << "-Y, --asrc_freq"  << "ALSA sample (default 48000)\n"
        "\t" << std::left << std::setw(18) << "-L, --ch_layout"  << "application specific value indicating layout of audio data within the channel (default 0)\n"
        "\t" << std::left << std::setw(18) << "-s, --has_sidech" << "use last audio channel for channel info (default 0)\n"
        "\t" << std::left << std::setw(18) << "-p, --period_size"<< "ALSA period size (default 256)\n"
        "\t" << std::left << std::setw(18) << "-N, --num_periods"<< "The size of the IPC buffer in periods\n"
        "\t" << std::left << std::setw(18) << "-x, --ch_index"   << "index of the channel where the renaming shall begin (default 0)\n"
        "\t" << std::left << std::setw(18) << "-P, --port_prefix"<< "string used to prefix the jack port names (default empty)\n"
        "\t" << std::left << std::setw(18) << "-i, --uf_ipc"     << "UF-IPC channel name dedicated to a local video stream (default empty)\n"
        "\t" << std::left << std::setw(18) << "-I, --instance"   << "specify the instance name used for communication\n"
        "\t" << std::left << std::setw(18) << "-F, --signal_freq"<< "frequency of the tone to be generated in Hz (<=sampleFreq/2) (default 0)\n"
        "\t" << std::left << std::setw(18) << "-A, --amplitude"  << "level/amplitude of the tone in dBFS (0 = full scale, -6 = half, etc.) (default 0)\n"
        "\t" << std::left << std::setw(18) << "-w, --wave_form"  << "wave form selection (default sine=0)\n"
        "\t" << std::left << std::setw(18) << "-u, --user_param" << "additional param to modify wave generation, depending on mode (default 0)\n"
        "\t" << std::left << std::setw(18) << "-R, --packet_rate"<< "maximum video packet rate (default 4000)\n"
        "\t" << std::left << std::setw(18) << "-S, --packet_size"<< "maximum video packet size (default 1460)\n"
        "\t" << std::left << std::setw(18) << "-T, --suspend"    << "suspend/resume (1=suspend, 0= resume) (default 1)\n"
        "\t" << std::left << std::setw(18) << "-t, --timeout"    << "timeout for command execution (default 5000 ms)\n"
        "\t" << std::left << std::setw(18) << "-v, --verbose"    << "verbose mode, use with --help\n"
    << std::endl;

    std::cout <<
        "Arguments can be passed in arbitrary order with the options.\n\n"
        "\tWith options: " << appName << " CreateTransmitAvbAudioStream -c 2 -r 48000 -f 1 -C 0x00 -m 0 -n 0x91E0F000FE000001 -M 0x91E0F0000001 -a 1\n"
    << std::endl;

    std::cout <<
        "You can omit the options which has default value.\n\n"
        "\tWith partial options: " << appName << " CreateTransmitAvbAudioStream -m 0 -n 0x91E0F000FE000001 -M 0x91E0F0000001 -a\n"
    << std::endl;

    std::cout <<
        "See the help of each command for the further information.\n\n"
        "\t" << appName << " <command> --help\n";
  }
  else
  {
    // show the usage of the specific command
    for (i = 0; i < ARRAY_LEN(cmdTbl); i++)
    {
      if (cmdTbl[i]->getName() == cmdName)
        break;
    }
    if (i == ARRAY_LEN(cmdTbl))
    {
      std::cout << "Unrecognized command: " << cmdName << ", try option --help\n";
    }
    else
    {
      cmdTbl[i]->printUsage();
    }
  }
  std::cout << std::endl;
}


int main(int argc, char *argv[])
{
  IasAvbProcessingResult result = eIasAvbProcErr;
  IasAvbResult cmdResult = IasAvbResult::eIasAvbResultErr;
  AvbStreamHandlerSocketIpc::requestSocketIpc userInputStruct;

  int32_t c = 0;
  bool showUsage = false;
  //bool legacyMode = false;

  std::string cmdName;
  uint32_t i = 0;

  static const struct option options[] =
  {
    { "hostip",     true, NULL, 'o' }, // socket server ip address
    { "hostport",   true, NULL, 'O' }, // socket server port
    { "channels",      true, NULL, 'C' }, // number of channels
    { "format",     true, NULL, 'f' }, // sample format
    { "rate",       true, NULL, 'r' }, // format of the audio (SAF16 == 1)
    { "clock",      true, NULL, 'C' }, // clock
    { "mode",       true, NULL, 'M' }, // assign mode
    { "dmac",       true, NULL, 'm' }, // MAC address, if assignMode indicates manual configuration.
    { "active",     true, NULL, 'a' }, // activate stream immediately
    { "direction",  true, NULL, 'd' }, // direction
    { "ch_layout",  true, NULL, 'L' }, // channel layout
    { "has_sidech", true, NULL, 's' }, // side channel
    { "period_size",true, NULL, 'p' }, // ALSA period size (number of ALSA frames, e.g. 256)
    { "num_periods",true, NULL, 'N' }, // The size of the IPC buffer in periods
    { "port_prefix",true, NULL, 'P' }, // jack port prefix
    { "device",     true, NULL, 'D' }, // alsa device name
    { "network_id", true, NULL, 'n' }, // network stream ID
    { "local_id",   true, NULL, 'l' }, // local stream ID
    { "ch_index",   true, NULL, 'x' }, // index of the channel where the renaming shall begin
    { "packet_rate",true, NULL, 'R' }, // maximum video packet rate
    { "packet_size",true, NULL, 'S' }, // maximum video packet size
    { "uf_ipc",     true, NULL, 'i' }, // UF-IPC channel name dedicated to a local video stream
    { "signal_freq",true, NULL, 'F' }, // frequency of the tone to be generated in Hz (<=sampleFreq/2)
    { "amplitude",  true, NULL, 'A' }, // level/amplitude of the tone in dBFS (0 = full scale, -6 = half, etc.)
    { "wave_form",  true, NULL, 'w' }, // wave form selection
    { "user_param", true, NULL, 'u' }, // additional param to modify wave generation, depending on mode
    { "srclass",    true, NULL, 'q' }, // stream reservation class (H = high, L = low)
    { "instance",   true, NULL, 'I' }, // the instance name used for communication"
    { "suspend",    true, NULL, 'T' }, // suspend/resume"
    { "verbose",    false,       NULL, 'v' }, // verbosity
    { "help",       false,       NULL, 'h' },
    { NULL,         false,    0,     0 }
  };

  for (;;)
  {
    c = getopt_long(argc, argv, "o:O:c:f:r:C:M:m:a:d:L:s:p:N:P:D:n:l:t:x:R:S:i:F:A:w:u:q:I:T:vh", options, NULL);
    if (-1 == c)
    {
      break;
    }

    switch (c)
    {
    case 'o':
      hostip = optarg;
      break;
    case 'O':
      hostport = optarg;
      break;
    case 'c':
      userInputStruct.numOfCh = static_cast<uint16_t>(atoi(optarg));
      break;
    case 'r':
      userInputStruct.sampleFreq = atoi(optarg);
      break;
    case 'f':
      userInputStruct.format = atoi(optarg);
      break;
    case 'C':
      std::stringstream(std::string(optarg)) >> std::hex >> userInputStruct.clockId;
      break;
    case 'M':
      userInputStruct.assignMode = atoi(optarg);
      break;
    case 'm':
      std::stringstream(std::string(optarg)) >> std::hex >> userInputStruct.dmac;
      break;
    case 'a':
      userInputStruct.active = (atoi(optarg) == 0) ? false : true;
      break;
    case 'd':
      userInputStruct.direction = atoi(optarg);
      break;
    case 'L':
      userInputStruct.channelLayout = static_cast<uint8_t>(atoi(optarg));
      break;
    case 's':
      userInputStruct.hasSideChannel = (atoi(optarg) == 0) ? false : true;
      printf ("OK");
      break;
    case 'p':
      userInputStruct.periodSize = static_cast<uint16_t>(atoi(optarg));
      break;
    case 'N':
      userInputStruct.numPeriods = static_cast<uint16_t>(atoi(optarg));
      break;
    case 'P':
      userInputStruct.portPrefix = optarg;
      break;
    case 'D':
      userInputStruct.deviceName = optarg;
      break;
    case 'y':
	userInputStruct.alsaDeviceType = static_cast<uint16_t>(atoi(optarg));
      break;
    case 'Y':
      userInputStruct.sampleFreqASRC = atoi(optarg);
      break;
    case 'n':
      std::stringstream(std::string(optarg)) >> std::hex >> userInputStruct.networkStreamId;
      break;
    case 'l':
      std::stringstream(std::string(optarg)) >> std::hex >> userInputStruct.localStreamId;
      break;
    case 'v':
      verbosity = 1;
      break;
    case 'R':
      userInputStruct.maxPacketRate = static_cast<uint16_t>(atoi(optarg));
      break;
    case 'S':
      userInputStruct.maxPacketSize = static_cast<uint16_t>(atoi(optarg));
      break;
    case 'x':
      userInputStruct.channelIdx = static_cast<uint16_t>(atoi(optarg));
      break;
    case 'i':
      //userInputStruct.ufipcName = optarg; //TODO: remove
      break;
    case 'F':
      userInputStruct.signalFrequency = static_cast<uint32_t>(atoi(optarg));
      break;
    case 'A':
      userInputStruct.amplitude = static_cast<int32_t>(atoi(optarg));
      break;
    case 'w':
      switch (atoi(optarg))
      {
      case 0:
        userInputStruct.toneMode = IasAvbTestToneMode::eIasAvbTestToneSine;
        break;
      case 1:
        userInputStruct.toneMode = IasAvbTestToneMode::eIasAvbTestTonePulse;
        break;
      case 2:
        userInputStruct.toneMode = IasAvbTestToneMode::eIasAvbTestToneSawtooth;
        break;
      case 3:
        userInputStruct.toneMode = IasAvbTestToneMode::eIasAvbTestToneFile;
        break;
      default:
        userInputStruct.toneMode = IasAvbTestToneMode::eIasAvbTestToneFile;
        break;
      }
      break;
    case 'u':
      userInputStruct.userParam = static_cast<int32_t>(atoi(optarg));
      break;
    case 'q':
      userInputStruct.srClass = ('H' == toupper(optarg[0])) ? IasAvbSrClass::eIasAvbSrClassHigh : IasAvbSrClass::eIasAvbSrClassLow;
      break;
    case 'I':
      instanceID = optarg;
      break;
    case 'T':
      userInputStruct.suspendAction = (atoi(optarg) == 0) ? false : true;
      break;
    case 'h':
    case '?':
      showUsage = true;
      break;
    default:
      break;
    }
  }

  if (optind < argc)
    cmdName = argv[optind];

  if(1 == argc || showUsage)
  {
    std::stringstream appNameFull(argv[0]);
    while (std::getline(appNameFull, appName, '/'))
    {
      // last entry will deliver the application name
    }

    // show help overview or usage of the specific command
    printUsage(cmdName);

    if (verbosity && cmdName.empty())
    {
      // show usage of all commands
      for (i = 0; i < ARRAY_LEN(cmdTbl); i++)
      {
        printUsage(cmdTbl[i]->getName());
      }
    }
    goto out;
  }

  // search for the command handler object
  for (i = 0; i < ARRAY_LEN(cmdTbl); i++)
  {
    if (cmdTbl[i]->getName() == cmdName)
    {
      break;
    }
  }

  if (ARRAY_LEN(cmdTbl) == i)
  {
    std::cout << "Unrecognized command: " << cmdName << ", try option --help\n";
    result = eIasAvbProcInvalidParam;
    goto out;
  }

  if (1 != optind)  //if params are specified
  {
    // note: argc does not include the program file name and the command name (i.e. argc = original argc - 2)
    argc = argc - 2;
    argc = argc/2;
    optind = optind + 1;

    if (cmdTbl[i]->getArgc() > argc)
    {
      std::cout <<
          "Error: Insufficient number of parameter "
          "(current=" << argc << "/needed=" << cmdTbl[i]->getArgc() << "), try --help\n";
      cmdResult = IasAvbResult::eIasAvbResultInvalidParam;
      goto out;
    }
  }
  else
  {
    argc = argc - 2;

    if (cmdTbl[i]->getArgc() == 0 && argc != 0)
    {
      std::cout <<
          "Error: Please pass parameters using options. Refer to --help\n";
      cmdResult = IasAvbResult::eIasAvbResultInvalidParam;
      goto out;
    }
  }

  if (!cmdTbl[i]->validateRequest(&userInputStruct)){
    std::cout << "Error: Mandatory arguments not included, try --help\n";
    cmdResult = IasAvbResult::eIasAvbResultInvalidParam;
    goto out;
  }

  try
  { // run command
    boost::asio::io_service io_service;
    AvbStreamHandlerSocketIpc::client client(io_service, hostip, hostport, cmdTbl[i], argc, &userInputStruct);
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  switch (cmdResult)
  {
  case IasAvbResult::eIasAvbResultOk:
    result = eIasAvbProcOK;
    break;
  case IasAvbResult::eIasAvbResultNotImplemented:
    result = eIasAvbProcNotImplemented;
    break;
  case IasAvbResult::eIasAvbResultInvalidParam:
    result = eIasAvbProcInvalidParam;
    break;
  default:
    result = eIasAvbProcErr;
    break;
  }

out:
  //TODO: Add proper cleaner/destructor for io_service in case interrupt
  return -result;
};// main
