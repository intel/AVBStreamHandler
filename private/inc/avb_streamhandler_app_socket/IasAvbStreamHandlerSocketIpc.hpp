/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**

 * @file    IasAvbStreamHandlerSocketIpc.hpp
 * @brief   This specifies the common serializataion functions, Command class and structs used by both client and server.
 * @details
 *          A separate namespace AvbStreamHandlerSocketIPC is used to store these classes.
 *          Boost's serialization examples are used in order to utilize the BOOST:ASIO serialization libraries.
 * @date    2018
 */

#ifndef SERIALIZATION_CONNECTION_HPP
#define SERIALIZATION_CONNECTION_HPP

#include <boost/asio.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include "avb_streamhandler/IasAvbStreamHandler.hpp"
#include <stdint.h>

using namespace IasMediaTransportAvb;

#define INVALID_STREAM_DIR   0xFFFFFFFFu
#define INVALID_LC_STREAM_ID 0xFFFFu              // invalid local stream id
#define INVALID_NW_STREAM_ID 0xFFFFFFFFFFFFFFFFu  // invalid network stream id
#define INVALID_MAC_ADDRESS  0xFFFFFFFFFFFFFFFFu
#define INVALID_CHANNEL_IDX  0xFFFFu

namespace AvbStreamHandlerSocketIpc {
using boost::serialization::make_nvp;

struct requestSocketIpc
{
  std::string command;
  std::string avbStreamInfo;
  std::string result;
  uint16_t oStreamId;
  uint16_t numOfCh          = 2;
  uint32_t sampleFreq       = 48000;
  int32_t  format           = -1;
  uint32_t assignMode       = 0;  // static
  uint64_t dmac             = INVALID_MAC_ADDRESS;
  int32_t  active           = 1;
  uint32_t direction        = INVALID_STREAM_DIR;
  uint8_t  channelLayout    = 0;
  bool   hasSideChannel   = false;
  std::string portPrefix;
  std::string deviceName;
  uint32_t clockId          = cIasAvbPtpClockDomainId;
  uint16_t periodSize       = 256;
  uint16_t numPeriods       = 3;
  uint16_t channelIdx       = INVALID_CHANNEL_IDX;
  uint64_t networkStreamId  = INVALID_NW_STREAM_ID;
  uint16_t localStreamId    = INVALID_LC_STREAM_ID;
  uint16_t maxPacketSize    = 1460;
  uint16_t maxPacketRate    = 4000;
  uint32_t signalFrequency  = 0;
  int32_t  amplitude        = 0;
  int32_t  userParam        = 0;
  bool   suspendAction      = true;
  IasAvbTestToneMode  toneMode = IasAvbTestToneMode::eIasAvbTestToneSine;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;
  uint16_t alsaDeviceType = 0;//virtual
  uint32_t sampleFreqASRC = 48000;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version)
  {
    (void) version;
    ar & command;
    ar & avbStreamInfo;
    ar & result;
    ar & oStreamId;
    ar & numOfCh;
    ar & sampleFreq;
    ar & format;
    ar & assignMode;
    ar & dmac;
    ar & active;
    ar & direction;
    ar & channelLayout;
    ar & hasSideChannel;
    ar & portPrefix;
    ar & deviceName;
    ar & clockId;
    ar & periodSize;
    ar & numPeriods;
    ar & channelIdx;
    ar & networkStreamId;
    ar & localStreamId;
    ar & maxPacketSize;
    ar & maxPacketRate;
    ar & signalFrequency;
    ar & amplitude;
    ar & userParam;
    ar & suspendAction;
    ar & toneMode;
    ar & srClass;
    ar & alsaDeviceType;
    ar & sampleFreqASRC;
  }
};

struct responseSocketIpc
{
  std::string command;
  std::string avbStreamInfo;
  std::string result;
  uint64_t oStreamId;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version)
  {
    (void) version;
    ar & command;
    ar & avbStreamInfo;
    ar & result;
    ar & oStreamId;
  }
};

// Command base class
class Command {
public:
  Command(const std::string& name, const std::string& desc, uint32_t argc) : mName(name), mDesc(desc), mArgc(argc) {}

  virtual ~Command() {};

  virtual void printUsage (void) {};
  const std::string& getName(void) {return mName;};
  const std::string& getDesc(void) {return mDesc;};
  const int32_t&  getArgc(void) {return mArgc;};

  virtual bool validateRequest (AvbStreamHandlerSocketIpc::requestSocketIpc */*userInputStruct*/) {return true;};

  virtual AvbStreamHandlerSocketIpc::responseSocketIpc execute (IasMediaTransportAvb::IasAvbStreamHandler */*avbStreamHandler*/, AvbStreamHandlerSocketIpc::requestSocketIpc */*defaultEmptyCommand*/) {
  AvbStreamHandlerSocketIpc::responseSocketIpc defaultEmptyCommand;
  defaultEmptyCommand.command = "EMPTY";
  return defaultEmptyCommand;};

  virtual void receive (AvbStreamHandlerSocketIpc::responseSocketIpc */*receivedResponse*/) {};

protected:
  const std::string mName;
  const std::string mDesc;
  const int32_t mArgc;
};


/// The connection class provides serialization primitives on top of a socket.
/**
 * Each message sent using this class consists of:
 * @li An 8-byte header containing the length of the serialized data in
 * hexadecimal.
 * @li The serialized data.
 */
class connection
{
public:
  connection();
  connection(boost::asio::io_service& io_service)
      : socket_(io_service)
  {
  }

  boost::asio::ip::tcp::socket& socket()
  {
    return socket_;
  }

  template <typename T, typename Handler>
  void async_write(const T& t, Handler handler)
  {
    // Serialize the data first so we know how large it is.
    std::ostringstream archive_stream;
    boost::archive::text_oarchive archive(archive_stream);
    archive << t;
    outbound_data_ = archive_stream.str();

    // Format the header.
    std::ostringstream header_stream;
    header_stream << std::setw(header_length)
      << std::hex << outbound_data_.size();
    if (!header_stream || header_stream.str().size() != header_length)
    {
      boost::system::error_code error(boost::asio::error::invalid_argument);
      socket_.get_io_service().post(boost::bind(handler, error));
      return;
    }
    outbound_header_ = header_stream.str();

    // Write the serialized data to the socket. We use "gather-write" to send
    // both the header and the data in a single write operation.
    std::vector<boost::asio::const_buffer> buffers;
    buffers.push_back(boost::asio::buffer(outbound_header_));
    buffers.push_back(boost::asio::buffer(outbound_data_));
    boost::asio::async_write(socket_, buffers, handler);
  }

  template <typename T, typename Handler>
  void async_read(T& t, Handler handler)
  {
    // Issue a read operation to read exactly the number of bytes in a header.
    void (connection::*f)(
        const boost::system::error_code&,
        T&, boost::tuple<Handler>)
      = &connection::handle_read_header<T, Handler>;
    boost::asio::async_read(socket_, boost::asio::buffer(inbound_header_),
        boost::bind(f,
          this, boost::asio::placeholders::error, boost::ref(t),
          boost::make_tuple(handler)));
  }

  template <typename T, typename Handler>
  void handle_read_header(const boost::system::error_code& e,
      T& t, boost::tuple<Handler> handler)
  {
    if (e)
    {
      boost::get<0>(handler)(e);
    }
    else
    {
      // Determine the length of the serialized data.
      std::istringstream is(std::string(inbound_header_, header_length));
      std::size_t inbound_data_size = 0;
      if (!(is >> std::hex >> inbound_data_size))
      {
        // Header doesn't seem to be valid. Inform the caller.
        boost::system::error_code error(boost::asio::error::invalid_argument);
        boost::get<0>(handler)(error);
        return;
      }

      inbound_data_.resize(inbound_data_size);
      void (connection::*f)(
          const boost::system::error_code&,
          T&, boost::tuple<Handler>)
          = &connection::handle_read_data<T, Handler>;
        boost::asio::async_read(socket_, boost::asio::buffer(inbound_data_),
        boost::bind(f, this,
            boost::asio::placeholders::error, boost::ref(t), handler));
    }
  }

  template <typename T, typename Handler>
  void handle_read_data(const boost::system::error_code& e,
      T& t, boost::tuple<Handler> handler)
  {
    if (e)
    {
      boost::get<0>(handler)(e);
    }
    else
    {
      try
      {
        std::string archive_data(&inbound_data_[0], inbound_data_.size());
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> t;
      }
      catch (std::exception& e)
      {
        boost::system::error_code error(boost::asio::error::invalid_argument);
        boost::get<0>(handler)(error);
        return;
      }

      // Inform caller that data has been received ok.
      boost::get<0>(handler)(e);
    }
  }

private:
  /// The underlying socket.
  boost::asio::ip::tcp::socket socket_;

  /// The size of a fixed length header.
  enum { header_length = 8 };

  /// Holds an outbound header.
  std::string outbound_header_;

  /// Holds the outbound data.
  std::string outbound_data_;

  /// Holds an inbound header.
  char inbound_header_[header_length];

  /// Holds the inbound data.
  std::vector<char> inbound_data_;
};

typedef boost::shared_ptr<connection> connection_ptr;

} // namespace AvbStreamhandlerSocketIpc

#endif // SERIALIZATION_CONNECTION_HPP


