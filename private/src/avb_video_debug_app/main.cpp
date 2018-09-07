/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>
#include <netinet/in.h>
#include <getopt.h>
#include <time.h>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <endian.h>
#include <algorithm>
#include <fstream>
#include <iostream>

#include "media_transport/avb_video_bridge/IasAvbVideoBridge.h"

/******************************************************************************
 * Configuration of the sender instance.
 ******************************************************************************/
ias_avbvideobridge_sender * h264_sender = 0;
ias_avbvideobridge_sender * mpegts_sender = 0;
const char * rolename_sender = "media_transport.avb_streaming.1";
const char * rolename_mpegts_sender = "media_transport.avb.mpegts_streaming.1";
volatile sig_atomic_t send_packages = 0;
uint32_t printDelay = 200u;


/******************************************************************************
 * Configuration of the receiver instance.
 ******************************************************************************/
ias_avbvideobridge_receiver * h264_receiver = 0;
ias_avbvideobridge_receiver * mpegts_receiver = 0;
const char * rolename_receiver = "media_transport.avb_streaming.7";
const char * rolename_mpegts_receiver = "media_transport.avb.mpegts_streaming.7";
const char * default_H264_instanceName = "My_H264_Receiver";
const char * default_MpegTs_instanceName = "My_MpegTs_Receiver";

/******************************************************************************
 * Declaration of subroutines
 ******************************************************************************/
void h264_callback(ias_avbvideobridge_receiver* inst, ias_avbvideobridge_buffer const * packet, void* user_ptr);
void MpegTS_callback(ias_avbvideobridge_receiver* inst, bool sph, ias_avbvideobridge_buffer const * packet, void* user_ptr);
void create_h264_instances(int mode);
void create_mpegts_instances(int mode);
void destroy_instances(void);
void INThandler(int sig);

/******************************************************************************
 * MPEG-TS globals common to sender and receiver
 ******************************************************************************/
const uint32_t mpegTsSize = 188u;
uint16_t maxPacketSize = 1460u; // This is set for each Video stream in IasAvbConfigReference.cpp
uint16_t maxPacketRate = 4000u; // This is set for each Video stream in IasAvbConfigReference.cpp
bool override_looptime = false; // If user provides packet rate then ignore loop time
int32_t looptime = 20000;
const uint32_t tspsInAvb = maxPacketSize / mpegTsSize;
int32_t direction = 's';
uint32_t tspsInBuffer = tspsInAvb;
const uint32_t tspsInBufferMax = 10000;
int32_t transport = 'h';
int32_t burst = 1;      // if set from command line then burstMode is set to true
bool burstMode = false;
bool verbose = false;
uint32_t lastPktCount = 0u;
bool doLatency = false;
bool isRunning = false; // used for sender gracefuly exit

char * mpegts_in_filename = NULL;    // to support mpegts data file transfer
char * mpegts_out_filename = NULL;   // to support mpegts data file transfer
std::ifstream mpegts_infile;
std::ofstream mpegts_outfile;

struct timespec now, then;
struct timespec tm_before_send, tm_after_send, tm_req;
std::string clockPath = "";
int32_t clockHandle = -1;
clockid_t clockId = 0;

const uint32_t nspersec = 1000000000u;
const char * version_string = "1.1";
char * cmdLnRoleName = NULL;
char * instanceName = NULL;

// statistics
uint32_t sequence_errors = 0u;
uint32_t packet_count_errors = 0u;
uint32_t data_errors =0u;
static uint32_t pktCount = 0u;
uint32_t sphlen = 0u;
static bool hassph = false;
static uint32_t tspTotal = 0u;
static uint32_t tspSendCount = 0u;
const uint32_t myArraySize = 500u;
uint64_t *myArray = new uint64_t[myArraySize];
uint32_t myIndex = 0u;
bool tsp_doIncremental = false;
uint32_t clockDev = 0xffu;
/******************************************************************************
 * Main
 *
 * This app supports H.264 and MPEG-TS transport in sender/talker and receiver/
 * lister roles. While in theory multiple roles can be supported on a single
 * machine this app is not there yet. The app assumes only one role/instance
 * on one machine.
 *
 * Invocation:
 * avb_video_debug_app --h264 --send       // H.264 sender
 * avb_video_debug_app --h264 --receive    // H.264 receiver
 * avb_video_debug_app --mpegts --send     // mpegts sender
 * avb_video_debug_app --mpegts --receive  // mpegts receiver
 * avb_video_debug_app --help              // complete list of options
 *
 * In H.264:
 * A sender instance constructs a 1400 byte buffer of ascending numbers in the
 * packet payload, inserts a sequence number in the RTP header, and uses one of
 * first two bytes to record a packet number that is used by the receiver to check
 * for packet loss. The receiver registers the call back that is called when each packet is received.
 * The call back verifies sequence order and packet consistency.
 *
 * In mpegts:
 * A successfully created sender instance will fill as many TSPs as possible in to
 * the AVB packet that is intended for transmission. A successfully created
 * receiver instance will verify packet data consistency and reception order.
 * The sequence ordering is manufactured by the app and does not conform to any
 * specification. The ordering process takes into consideration the possible reuse
 * of packets previously submitted for transport and still lingering with space for
 * adding TSPs.
 *
 * The program on either end of the stream runs until ctrl-c is pressed. Sender and
 * receiver instances are destroyed to ensure a proper cleanup.
 ******************************************************************************/
int main(int argc, char* argv[])
{
  (void) argc;
  (void) argv;

  static const struct option options[] =
  {
    { "h264",           no_argument, 0, 'h' },
    { "mpegts",         no_argument, 0, 'm' },
    { "send",           no_argument, 0, 's' },
    { "receive",        no_argument, 0, 'r' },
    { "looptime",       required_argument, 0, 'l' },
    { "delay",          required_argument, 0, 'd' },
    { "burst",          required_argument, 0, 'b' },
    { "tspsinbuffer",   required_argument, 0, 't' },
    { "maxpacketsize",  required_argument, 0, 'S' },
    { "maxpacketrate",  required_argument, 0, 'R' },
    { "rolename",       required_argument, 0, 'N' },
    { "latency",        required_argument, 0, 'L' },
    { "incremental",    required_argument, 0, 'I' },
    { "clock",          required_argument, 0, 'C' },
    { "instancename",   required_argument, 0, 'U' },
    { "help",           no_argument, 0, 'p'},
    { "verbose",        no_argument, 0, 'v'},
    { "hassph",          no_argument, 0, 'H'},
    { NULL, 0, 0, 0 }
  };

  int32_t c;
  struct timespec tIn;

  printf("Avb Bridge Debug Application\t%s\n", version_string);

  signal(SIGINT, INThandler);

  if (1 == argc) // no options passed, assume H.264
  {
    printf("assuming send with H.264 transport..\n");
  }
  else
  {
    while (1)
    {
      int32_t option_index = 0;

      c = getopt_long(argc, argv, "hml:srd:b:t:vS:R:N:HLIC:U:", options, &option_index);
      if (-1 == c)
      {
        break;
      }
      switch (c)
      {
        case 'h':
        {
          printf("Using H.264 transport\n");
          transport = 'h';
          break;
        }
        case 'm':
        {
          printf("Using MPEG-TS transport\n");
          transport = 'm';
          break;
        }
        case 'b':
        {
          burst = atoi(optarg);
          if ( (1 < burst) && (0x7fffffff > burst) )
          {
            burstMode = true;
            break;
          }
          else
          {
            printf(" option burst requires positive integer argument greater than 1\n");
            return -1;
          }
        }
        case 'd':
        {
          if (0 < atoi(optarg))
          {
            printDelay = atoi(optarg);
            break;
          }
          else
          {
            printf(" option delay requires positive integer argument\n");
            return -1;
          }
        }
        case 'l':
        {
          if (0 < atoi(optarg))
          {
            looptime = atoi(optarg);
            break;
          }
          else
          {
            printf(" option looptime requires positive integer argument\n");
            return -1;
          }
        }
        case 'S':
        {
          if (0 < atoi(optarg))
          {
            maxPacketSize = static_cast<uint16_t>(atoi(optarg));
            break;
          }
          else
          {
            printf(" option maxpacketsize requires positive integer argument\n");
            return -1;
          }
        }
        case 'R':
        {
          if (0 < atoi(optarg))
          {
            maxPacketRate = static_cast<uint16_t>(atoi(optarg));
            override_looptime = true;
            break;
          }
          else
          {
            printf(" option maxpacketrate requires positive integer argument\n");
            return -1;
          }
        }
        case 'N':
        {
          cmdLnRoleName = optarg;
          break;
        }
        case 'L':
        {
          doLatency = true;
          break;
        }
        case 'I':
        {
          tsp_doIncremental = true;
          break;
        }
        case 'H':
        {
          hassph = true;
          break;
        }
        case 'U':
        {
          instanceName = optarg;
          break;
        }

        case 't':
        {
          tspsInBuffer = atoi(optarg);
          if (0 == tspsInBuffer)
          {
            printf(" option tspsinbuffer set to minimum\n");
            tspsInBuffer = 1;
            break;
          }
          else if (tspsInBufferMax < tspsInBuffer)
          {
            printf(" option tspsinbuffer set to maximum\n");
            tspsInBuffer = tspsInBufferMax;
            break;
          }
          break;
        }
        case 'C':
        {
        // By adding '=', the default clockdev /dev/ptp0 can be explicitly used
          if (0 <= atoi(optarg))
          {
            clockDev = atoi(optarg);
            break;
          }
          else
          {
            printf(" option clock requires positive integer argument\n");
            return -1;
          }
        }
        case 's':
        {
          direction = 's';
          break;
        }
        case 'r':
        {
          direction = 'r';
          break;
        }
        case 'p':
        {
          printf(
              "Usage: avb_video_debug_app [options] \n"
              "\n"
              "Options:\n"
              "\t -m or --mpegts\t\tfor MpegTS transport\n"
              "\t -h or --h264\t\tfor H.264 transport\n"
              "\t -s or --send\t\tfor sender\n"
              "\t -r or --receive\tfor receiver\n"
              "\t -l or --looptime\tto set transmit delay in  microseconds\n"
              "\t -d or --delay\t\tto set print delay in number of received packets\n"
              "\t -b or --burst\t\tto send a fixed number of packets and stop\n"
              "\t -t or --tspsinbuffer\tto set number of ts packets in send buffer (UINT32_t)\n"
              "\t -v or --verbose\tto make receiver print payload content\n"
              "\t -S or --maxpacketsize\tto set max packet size\n"
              "\t -R or --maxpacketrate\tto set max packet rate [will superceed looptime setting]\n"
              "\t -N or --rolename\tto set the rolename of the stream containing video data\n"
              "\t -U or --instancename\tto label a listening session\n"
              "\t -L or --latency\tto show latency information on exit (listner only)\n"
              "\t -C or --clock\t\tto set ptp clock device number\n"
              "\t --help\t\t\tdisplays this usage info and exit\n"
              "\n"
              "Note:\n"
              "avb_video_debug_app tool can be used for streaming a TS file over AVB.\n"
              "\tTalker:  avb_video_debug_app -m -s <input TS file>  [options]\n"
              "\tListner: avb_video_debug_app -m -r <output TS file> [options]\n"
              "\n");
          printf(
 " In H.264:\n"
 " A sender instance constructs a 1400 byte buffer of ascending\n"
 " numbers in the packet payload, inserts a sequence number in \n"
 " the RTP header, and uses one of first two bytes to record a \n"
 " packet number that is used by the receiver to check for \n"
 " packet loss. The receiver registers the call back that is\n"
 " called when each packet is received. The call back verifies\n"
 " sequence order and packet consistency.\n"
 "\n"
 " In mpegts:\n"
 " A successfully created sender instance will fill as many \n"
 " TSPs as possible in to the AVB packet that is intended for\n"
 " transmission. A successfully created receiver instance will\n"
 " verify packet data consistency and reception order. The sequence\n"
 " ordering is manufactured by the app and does not conform to any\n"
 " specification. The ordering process takes into consideration the\n"
 " possible reuse of packets previously submitted for transport and \n"
 " still lingering with space for adding TSPs.\n"
 "\n"
 " In mpegts mode, a file name may be provided for input or output.\n"
 " The talker asumes that the file provides input while the listner\n"
 " assumes it to be output. Please note that a talker reading data \n"
 " from a file will produce unpredictable results on a listner that \n"
 " does NOT have an output file.\n"
 "\n"
 " The program on either end of the stream runs until ctrl-c is pressed.\n"
 " Sender and receiver instances are destroyed to ensure a proper cleanup\n");
          return 0;
        }
        case 'v':
        {
          verbose = true;
          break;
        }
        default:
        {
          return -1;
        }
      }
    }
  }

  if (0xff == clockDev)        //clock device not set. default to 0
  {
    clockDev = 0;
  }
  clockPath = "/dev/ptp" + std::to_string(clockDev);
  clockHandle = open(clockPath.c_str(), O_RDWR );
  if (-1 == clockHandle)
  {
    printf("Failed to open clock device %s!!!\n", clockPath.c_str());
    return -1;
  }
  clockId = ((~(clockid_t(clockHandle)) << 3) | 3);

  if ('h' == transport)
  {
    create_h264_instances(direction);
  }
  if ('m' == transport)
  {
    create_mpegts_instances(direction);
    if ('s' == direction)
    {
      mpegts_in_filename = argv[optind];

      // is filename provided
      if (NULL != mpegts_in_filename)
      {
        // can we open the file?
        mpegts_infile.open(mpegts_in_filename);
        if (!mpegts_infile.is_open())
        {
          printf(" **** Cant open output file %s\n Closing... \n", mpegts_in_filename);
          return(-1);
        }
      }
    }
    else if ('r' == direction)
    {
      mpegts_out_filename = argv[optind];

      // is filename provided
      if (NULL != mpegts_out_filename)
      {
        // can we open the file?
        mpegts_outfile.open(mpegts_out_filename, std::ofstream::out |std::ofstream::binary);
        if (!mpegts_outfile.is_open())
        {
          printf(" **** Cant open output file %s\n Closing... \n", mpegts_out_filename);
          return(-1);
        }
        else
        {
          printf("Writing to file %s\n", mpegts_out_filename);
        }
      }
    }
  }

  uint32_t pi_nsec = 1000000000u/maxPacketRate;  //packet interval in nano secs

  if (h264_sender)
  {
    printf("Send h264 packets - press ctrl-c to stop\n");
    uint16_t counter = 0u;
    uint8_t counter2 = 0u;
    ias_avbvideobridge_buffer p;
    char d[1400];

    uint8_t  * rtpBase8  = (uint8_t*)  d;
    uint16_t * rtpBase16 = (uint16_t*) d;
    uint32_t * rtpBase32 = (uint32_t*) d;
    uint64_t * rtpBase64 = (uint64_t*) d;

    (void)rtpBase16;
    // show how a packet could be filled using H.264 rtp style
    rtpBase8[0]  = 0x80; // RFC 1889 version(2)
    rtpBase8[1]  = 96; //provide marker bit + payload type: hardcoded H264
    rtpBase32[1] = htonl(0xdeadbeef);
    rtpBase32[2] = htonl(0x4120db95); // SSRC hard-coded
    rtpBase8[12] = 0x5C; // NAL header
    rtpBase8[13] = 0x41; // NAL header

    // Fill buffer with number sequence for verification
    // leave 32 bytes for internal use and rtp header
    uint32_t i;
    for (i = 0; i < sizeof(d)-32; i++)
    {
        rtpBase8[i + 32] = static_cast<uint8_t>(i);
    }

    p.data = rtpBase8; // hand over filled rtp packet
    p.size = sizeof(d);

    printf("\n\tSending... \n");

    send_packages = 1;

    while ( send_packages && (0 < burst) )
    {
      // Adding sequence counter at start of payload. Reserve  14 and 15 for it but currently sequence counter is only 8 bits wide so only one byte is used
      rtpBase16[1] = htons(counter);
      rtpBase8[14] = counter2;

      // for latency measurement
      memset(&tIn, 0, sizeof tIn);
      clock_gettime(clockId, &tIn);
      rtpBase64[2] = htobe64((uint64_t(tIn.tv_sec) * uint64_t(1000000000u)) + tIn.tv_nsec); // bytes 16 - 24

      memset(&tm_before_send, 0, sizeof(tm_before_send));
      (void)clock_gettime(clockId, &tm_before_send);
      if (IAS_AVB_RES_OK != ias_avbvideobridge_send_packet_H264(h264_sender, &p))
      {
        printf("Failed to send H.264 packet\n");
      }

      memset(&tm_after_send, 0, sizeof(tm_after_send));
      (void)clock_gettime(clockId, &tm_after_send);
      counter++;
      counter2++;
      if (0 == counter % (8 * printDelay))
      {
        printf("packets sent - %d\r", counter);
        fflush(stdout);
      }
      if (override_looptime)
      {
        // assume that sleep is always sub second granularity
        tm_req.tv_sec = 0;
        if (tm_after_send.tv_nsec >= tm_before_send.tv_nsec)
        {
          tm_req.tv_nsec = pi_nsec - (tm_after_send.tv_nsec - tm_before_send.tv_nsec);
        }
        else
        {
          tm_req.tv_nsec = pi_nsec - (tm_after_send.tv_nsec + nspersec - tm_before_send.tv_nsec);
        }
        nanosleep(&tm_req, NULL);  // don't worry about returned value or remaining time. wake up if interrupted
      }
      else
      {
        usleep(looptime); // command line arg. default 100000
      }
      if (burstMode)
      {
        burst--;
      }
    }
  }
  else if (mpegts_sender)
  {
    printf("Send mpegts packets - press ctrl-c to stop\n");
    ias_avbvideobridge_buffer p;

    // if an input file was provided we want to read it in 188 byte chunks and send it off
    // Do not manufacture data
    if (mpegts_infile.is_open())
    {
      char *buf;
      int bufSize = mpegTsSize * tspsInBuffer;

      buf = static_cast<char *>(malloc(bufSize));
      if (NULL == buf)
      {
        printf("MpegTS2 file transfer **** FAILED to allocate %d bytes\n", bufSize);
        return -1;
      }

      mpegts_infile.read(buf, bufSize);
      tspSendCount += tspsInBuffer;
      while(!mpegts_infile.eof()  && !isRunning)
      {
        p.data = buf;
        p.size = bufSize;

        if (IAS_AVB_RES_OK != ias_avbvideobridge_send_packet_MpegTs(mpegts_sender, hassph, &p))
        {
          sleep(1);
          if (IAS_AVB_RES_OK != ias_avbvideobridge_send_packet_MpegTs(mpegts_sender, hassph, &p))
          {
            tspSendCount--;
            printf("Failed to send MpegTS packet\n");
          }
        }
        mpegts_infile.read(buf, bufSize);
        tspSendCount += tspsInBuffer;
        printf("packets sent - %d\r", tspSendCount);
        fflush(stdout);
        usleep(looptime);
      }
      printf("\n");
      mpegts_infile.close();
    }
    else
    {
      // Manufacture data for testing
      uint16_t counter = 0u;

      if (tsp_doIncremental)
      {
        doLatency = false;
        tspsInBuffer = 1u;
      }

      if (doLatency)
      {
        tspsInBuffer = tspsInAvb;
      }
      uint8_t * payloadBase8 = NULL;
      uint64_t * payloadBase64 = NULL;
      uint8_t sqnbr = 0u;
      size_t d_size = tspsInBuffer * mpegTsSize;
      uint32_t tsp_limit = 30u;

      if (tsp_doIncremental)
      {
        payloadBase8 = static_cast<uint8_t *>(malloc(30 * mpegTsSize));
      }
      else
      {
        if (d_size > 0)
        {
          payloadBase8 = static_cast<uint8_t *>(malloc(d_size));
        }
        else
        {
          payloadBase8 = static_cast<uint8_t *>(malloc(tspsInAvb * mpegTsSize));
        }
      }
      payloadBase64 = reinterpret_cast<uint64_t *>(payloadBase8);
      if (NULL != payloadBase8)
      {
        send_packages = 1;
        printf("\n\tSending.. \n");
      }
      else
      {
        printf("\n\tAllocation failed. Not enough memory\n");
        return -1;
      }

      while ( send_packages && (0 < burst) )
      {
        // Fill buffer with number sequence for verification
        uint32_t i, j, start;

        if (tsp_doIncremental)
        {
          d_size = tspsInBuffer * mpegTsSize;
        }
        // printf("\nSequence Count : %d\n\n", sqnbr);

        if (doLatency)
        {
          // first TSP is left empty for house keeping activity
          start = 1;
        }
        else
        {
          start = 0;
        }
        for (i = start; (i * mpegTsSize) < d_size; i++)
        {
          // printf("Creating tsp --- %d\n", i);
          for (j = 0; j < mpegTsSize + sphlen; j++)
          {
            //payloadBase8[i * mpegTsSize + j] = static_cast<uint8_t>(j);
            //payloadBase8[i*mpegTsSize + j] = 0xAB;
            payloadBase8[i * (mpegTsSize + sphlen) + j] = static_cast<uint8_t>(sqnbr + j);
          }
        }
        p.data = payloadBase8; // hand over filled MpegTS packet
        p.size = d_size;
        if (doLatency)
        {
          payloadBase8[16] = sqnbr++;
        }

        // for latency measurement
        if (doLatency)
        {
          memset(&tIn, 0, sizeof(tIn));
          clock_gettime(clockId, &tIn);
          payloadBase64[1] = htobe64((uint64_t(tIn.tv_sec) * uint64_t(1000000000u)) + tIn.tv_nsec);
        }
        memset(&tm_before_send, 0, sizeof(tm_before_send));
        (void)clock_gettime(clockId, &tm_before_send);
        if (IAS_AVB_RES_OK != ias_avbvideobridge_send_packet_MpegTs(mpegts_sender, hassph, &p))
        {
          sleep(1);
          if (IAS_AVB_RES_OK != ias_avbvideobridge_send_packet_MpegTs(mpegts_sender, hassph, &p))
          {
            printf("Failed to send packet\n");
          }
        }
        tspSendCount += tspsInBuffer;
        memset(&tm_after_send, 0, sizeof(tm_after_send));
        (void)clock_gettime(clockId, &tm_after_send);
        if (0 == counter % (8 * printDelay))
        {
          printf("packets sent - %d\r", counter);
          fflush(stdout);
        }
        if (override_looptime)
        {
          // assume that sleep is always sub second granularity
          tm_req.tv_sec = 0;
          if (tm_after_send.tv_nsec >= tm_before_send.tv_nsec)
          {
            tm_req.tv_nsec = pi_nsec - (tm_after_send.tv_nsec - tm_before_send.tv_nsec);
          }
          else
          {
            tm_req.tv_nsec = pi_nsec - (tm_after_send.tv_nsec + nspersec - tm_before_send.tv_nsec);
          }
          nanosleep(&tm_req, NULL);  // don't worry about returned value or remaining time. wake up if interrupted
        }
        else
        {
          usleep(looptime); // command line arg. default 100000
        }
        counter++;
        if (burstMode)
        {
          burst--;
        }

        if (tsp_doIncremental)
        {
          if (tsp_limit > tspsInBuffer)
          {
            tspsInBuffer++;
          }
          else
          {
            tspsInBuffer = 1;
          }
        }
      }
      if (NULL != payloadBase8)
      {
        free(payloadBase8);
      }
    }
  }
  else
  {
    // Main loop for receiver
    send_packages = 1;
    while (send_packages)
    {
      usleep(looptime);
    }
  }

  // TODO: For debug purposes only. Set spin = 1 to be able to attach with debugger
  volatile uint32_t spin = 0u;
  while (spin);

  destroy_instances();

  printf("Bye!\n");
  return 0;
}

/******************************************************************************
 * Definition of the H.264 callback function for the receiver
 ******************************************************************************/
void h264_callback(ias_avbvideobridge_receiver* inst, ias_avbvideobridge_buffer const * packet, void* user_ptr)
{
  static uint16_t prevSeqNum = 0u;
  static uint8_t prevPktNum = 0u;
  (void) inst;
  (void) user_ptr;
  bool dataOK = true;
  bool doprint = false;
  int64_t timediff;
  struct timespec ltm;

  static bool seqScanStart = true;
  uint8_t *rtpBase8 = (uint8_t *)(packet->data);
  uint16_t *rtpBase16 = (uint16_t *)(packet->data);
  uint32_t *rtpBase32 = (uint32_t *)(packet->data);
  uint64_t *rtpBase64 = (uint64_t *)(packet->data);

  if (doLatency)
  {
    (void)clock_gettime(clockId, &ltm);
    if (myArraySize == myIndex)
    {
      myIndex = 0u;
    }
    myArray[myIndex] = (ltm.tv_nsec + (uint64_t(ltm.tv_sec) * (uint64_t) 1000000000) - be64toh(rtpBase64[2]));
    myIndex++;
  }

  if(0 == (pktCount++ % printDelay))
  {
    doprint = true;
  }

  if (doprint)
  {
    printf("\n\tH264_callback * Size %zu Packet count %x \n", packet->size, pktCount);
    printf("\tRTP time stamp - 0x%x\n", htonl(rtpBase32[1]));
    printf("\tRTP sequence number - 0x%x\n", htons(rtpBase16[1]));
    printf ("\tVerifying data.. \n");

    (void)clock_gettime(clockId, &now);
    timediff = (now.tv_nsec + now.tv_sec * 1000000000) - (then.tv_nsec +then.tv_sec * 1000000000);
    if ((int64_t)1000000000 <= timediff)
    {
      printf("\n\t*** Packet rate - %d \n\n", (pktCount - lastPktCount));
      lastPktCount = pktCount;
      then.tv_sec = now.tv_sec;
      then.tv_nsec = now.tv_nsec;
    }
  }


#if 0
  uint32_t *rtpBase32 = (uint32_t *)(packet->data);

  printf("\trfc version %x\n", rtpBase8[0]);
  printf("\ttype %x\n", rtpBase8[1]);
  printf("\tsequence number : %x\n", ntohs(rtpBase16[1]));
  printf("\tSSRC %x\n", ntohl(rtpBase32[2]));

#endif

  if (seqScanStart)
  {
    prevSeqNum = htons(rtpBase16[1]);
    seqScanStart = false;
    prevPktNum = (uint8_t)rtpBase8[14];
  }
  else
  {
    // sequence number check
    if ( (uint16_t)(prevSeqNum + 1) != htons(rtpBase16[1]) )
    {
      printf("*** Incorrect Sequence at packet count %x\n", pktCount);
      printf("Expected %x but got %x \n",(uint16_t)(prevSeqNum + 1), htons(rtpBase16[1]));
      sequence_errors++;
    }
    // packet count check
    if ( (uint8_t)(prevPktNum + 1) != (uint8_t)rtpBase8[14] )
    {
      printf("*** Packet loss at packet count %x\n", pktCount);
      printf("Expected packet count was %x but received %x \n", prevPktNum + 1, (uint8_t)rtpBase8[14]);
      packet_count_errors++;
    }
    prevSeqNum = htons(rtpBase16[1]);
    prevPktNum = (uint8_t)rtpBase8[14];
  }

  // data check
  for (int32_t i=32; i<1400; i++)
  {
    if ( (uint8_t)(i - 32) != rtpBase8[i] )
    {
      dataOK = false;
      printf("*******  Data inconsistent at offset %d \n", i);
      printf("\tExpected %x but found %x \n",(uint8_t)(i - 32), (uint8_t)rtpBase8[i]);
      data_errors++;
    }
    if (verbose && doprint)
    {
      printf("%x,", rtpBase8[i]);
    }
  }
  if (verbose && doprint)
  {
    printf("\n");
  }
  if ( dataOK && doprint )
  {
    printf("\tData Consistency check OK\n");
  }
}

/******************************************************************************
 * Definition of the MpegTS callback function for the receiver
 ******************************************************************************/
void MpegTS_callback(ias_avbvideobridge_receiver* inst, bool sph, ias_avbvideobridge_buffer const * packet, void* user_ptr)
{
  (void) inst;
  (void) user_ptr;
  bool doprint = false;

  pktCount++;

  uint32_t tspCount = (uint32_t)packet->size / mpegTsSize;

  uint32_t i,j;

  int64_t timediff;
  struct timespec ltm;

  uint8_t* tspStart = (uint8_t*)(packet->data);
  uint32_t* tspStart32 = (uint32_t*)(packet->data);

  if (NULL != mpegts_out_filename)
  {
    tspTotal += tspCount;
    // write data to file
    if (mpegts_outfile.is_open())
    {
      for (unsigned int i=0; i< tspCount; i++)
      {
        mpegts_outfile.write(reinterpret_cast< const char *>(tspStart + 4), mpegTsSize);
        tspStart += mpegTsSize + 4;
      }
      // mpegts_outfile.write(reinterpret_cast<const char *>(packet->data), packet->size);
      printf("TSP Count %d\r", tspTotal);
      fflush(stdout);
    }
  }
  else
  {
    if (0 != (packet->size % (mpegTsSize + 4)))   // using '+4' because we asume that SPH is always present
    {
      printf("*****  Packet Length Not multiple of %d\n", mpegTsSize);
    }

    if (doLatency)
    {
      (void)clock_gettime(clockId, &ltm);
      if (myArraySize == myIndex)
      {
        myIndex = 0u;
      }
      myArray[myIndex] = ((uint64_t)ltm.tv_nsec + (uint64_t)ltm.tv_sec * (uint64_t)1000000000) - be64toh((uint64_t)tspStart32[4] << 32 | tspStart32[3]);
      myIndex++;
    }

    tspTotal += tspCount;

    if (0 == pktCount % printDelay)
    {
      doprint = true;
    }
    if (doprint)
    {
      printf("\n\tMpegTS_callback - PC %d * ts packet received \n\tsize %zu \n", pktCount, packet->size);
      printf("\tTS packets in packet %u\n", (uint32_t)packet->size/mpegTsSize);

      (void)clock_gettime(clockId, &now);
      timediff = (now.tv_nsec + now.tv_sec * 1000000000) - (then.tv_nsec +then.tv_sec * 1000000000);
      // printf("\tTime Diff %ld\n", timediff);
      if ((int64_t)1000000000 <= timediff)
      {
        printf("\n\t*** Packet rate - %d \n\n", (pktCount - lastPktCount));
        lastPktCount = pktCount;
        then.tv_sec = now.tv_sec;
        then.tv_nsec = now.tv_nsec;
      }

      if (sph)
      {
        printf("\tSPH is set\n");
      }
      else
      {
        printf("\tSPH not set\n");
      }

      printf ("\tVerifying data.. \n");
    }

    bool dataOK = true;
    uint32_t start = 0;

    if (doLatency)
    {
      // first TSP for internal use
      start = 1u;
    }
    for (i = start; i < tspCount; i++)
    {
      // printf("\tSPH - %d : 0x%x:\n", i, ntohl(tspStart32[i * 192/4]));

      // 0-3 is sph
      for (j = 4; j < 192; j++)
      {
        if (verbose && doprint)
        {
          printf ("%x,", tspStart[i*192 + j]);
        }
        if (uint8_t(tspStart[i*192 + 4] + (j - 4)) != tspStart[i*192 + j])
        {
          dataOK = false;
          printf("*** TSP %d - Offset 0x%x has inconsistent data\n", i, j);
          printf("Expected %x but got %x\n", tspStart[i*192 + 4] + (j - 4), tspStart[i*192 + j]);
          data_errors++;
        }
      }
      if (verbose && doprint)
      {
        printf("\n");
      }
    }
    if (dataOK)
    {
      if (doprint)
      {
        printf("\tData Consistency check OK\n");
      }
    }
    doprint = false;
  }
}

/******************************************************************************
 * Create the sender and receiver instances
 ******************************************************************************/
void create_h264_instances(int mode)
{
  struct timespec tIn;

  memset(&tIn, 0, sizeof tIn);
  clock_gettime(clockId, &tIn);
  printf("\nPtp event count - %ld\n\n", tIn.tv_nsec + tIn.tv_sec * 1000000000);

  if ('s' == mode)
  {
    printf("Create instance to send H.264 packets\n");
    if (NULL == cmdLnRoleName)
    {
      h264_sender = ias_avbvideobridge_create_sender(rolename_sender);
    }
    else
    {
      h264_sender = ias_avbvideobridge_create_sender(cmdLnRoleName);
    }
    if (!h264_sender)
    {
      printf("ERROR: Failed to create the H.264 sender\n");
    }
  }

  if ('r' == mode)
  {
    printf("Create instance to receive H.264 packets\n");
    if (NULL == cmdLnRoleName)
    {
      if (NULL != instanceName)
      {
        h264_receiver = ias_avbvideobridge_create_receiver(instanceName, rolename_receiver);
      }
      else
      {
        h264_receiver = ias_avbvideobridge_create_receiver(default_H264_instanceName, rolename_receiver);
      }
    }
    else
    {
      if (NULL != instanceName)
      {
        h264_receiver = ias_avbvideobridge_create_receiver(instanceName, cmdLnRoleName);
      }
      else
      {
        h264_receiver = ias_avbvideobridge_create_receiver(default_H264_instanceName, cmdLnRoleName);
      }
    }
    if (!h264_receiver)
    {
      printf("ERROR: Failed to create the H.264 receiver\n");
    }
    else
    {
      if (IAS_AVB_RES_OK != ias_avbvideobridge_register_H264_cb(h264_receiver, &h264_callback, 0))
      {
        printf("ERROR: Failed to register the H.264 callback\n");
      }
    }
  }
}

void create_mpegts_instances(int mode)
{
  struct timespec tIn;

  memset(&tIn, 0, sizeof tIn);
  clock_gettime(clockId, &tIn);
  printf("\nPtp event count - %ld\n\n", tIn.tv_nsec + tIn.tv_sec * 1000000000);

  if ('s' == mode)
  {
    printf("Create instance to send MPEG-TS packets\n");
    if (NULL == cmdLnRoleName)
    {
      mpegts_sender = ias_avbvideobridge_create_sender(rolename_mpegts_sender);
    }
    else
    {
      mpegts_sender = ias_avbvideobridge_create_sender(cmdLnRoleName);
    }
    if (!mpegts_sender)
    {
      printf("ERROR: Failed to create the mpegts sender\n");
    }
  }

  if ('r' == mode)
  {
    printf("Create instance to receive MPEG-TS packets\n");

    if (NULL == cmdLnRoleName)
    {
      if (NULL != instanceName)
      {
        mpegts_receiver = ias_avbvideobridge_create_receiver(instanceName, rolename_mpegts_receiver);
      }
      else
      {
        mpegts_receiver = ias_avbvideobridge_create_receiver(default_MpegTs_instanceName, rolename_mpegts_receiver);
      }
    }
    else
    {
      if (NULL != instanceName)
      {
        mpegts_receiver = ias_avbvideobridge_create_receiver(instanceName, cmdLnRoleName);
      }
      else
      {
        mpegts_receiver = ias_avbvideobridge_create_receiver(default_MpegTs_instanceName, cmdLnRoleName);
      }
    }

    if (!mpegts_receiver)
    {
      printf("ERROR: Failed to create the mpegts receiver\n");
    }
    else
    {
      if (IAS_AVB_RES_OK != ias_avbvideobridge_register_MpegTS_cb(mpegts_receiver, &MpegTS_callback, 0))
      {
        printf("ERROR: Failed to register the mpegts callback\n");
      }
    }
  }
}

/******************************************************************************
 * Destroy all created instances
 ******************************************************************************/
void destroy_instances(void)
{
  if (h264_sender)
  {
    printf("Destroy sender                              \n");
    ias_avbvideobridge_destroy_sender(h264_sender);
    h264_sender = 0;
  }
  if (mpegts_sender)
  {
    printf("\nTSP send count \t\t%d\n", tspSendCount);
    printf("Destroy mpegts_sender\n");
    ias_avbvideobridge_destroy_sender(mpegts_sender);
    mpegts_sender = 0;
  }

  if (h264_receiver)
  {
    if (doLatency)
    {
      std::sort(myArray, myArray + myArraySize);
      printf("Latency[median]\t\t\t%" PRIu64 ".%" PRIu64 "ms\n", myArray[myArraySize/2]/1000000, myArray[myArraySize/2]%1000000);
      printf("Latency[median + 1]\t\t%" PRIu64 ".%" PRIu64 "ms\n", myArray[(myArraySize/2) + 1]/1000000, myArray[(myArraySize/2) + 1]%1000000);
    }
    printf("Packets Recieved\t\t%d\n", pktCount);
    printf("Sequence Errors\t\t\t%d\n", sequence_errors);
    printf("Packet Count Errors\t\t%d\n", packet_count_errors);
    printf("Data Errors\t\t\t%d\n", data_errors);
    printf("Destroy receiver\n");
    ias_avbvideobridge_destroy_receiver(h264_receiver);
    h264_receiver = 0;
  }
  if (mpegts_receiver)
  {
    if (doLatency)
    {
      std::sort(myArray, myArray + myArraySize);
      printf("Latency[median]\t\t\t%" PRIu64 ".%" PRIu64 "ms\n", myArray[myArraySize/2]/1000000, myArray[myArraySize/2]%1000000);
      printf("Latency[median + 1]\t\t%" PRIu64 ".%" PRIu64 "ms\n", myArray[(myArraySize/2) + 1]/1000000, myArray[(myArraySize/2) + 1]%1000000);
    }
    printf("Packets Recieved\t\t%d\n", pktCount);
    printf("Total TSP count\t\t\t%d\n", tspTotal);
    printf("Data Errors\t\t\t%d\n", data_errors);
    printf("Destroy mpegts_receiver\n");
    ias_avbvideobridge_destroy_receiver(mpegts_receiver);
    mpegts_receiver = 0;
  }
  mpegts_outfile.close();
  return;
}

/******************************************************************************
 * Signal handler
 ******************************************************************************/
void INThandler(int sig)
{
  printf("\nReceived signal\n");
  (void) sig; // not of interest
  send_packages = 0;
  isRunning = true; // make while loop of mpegTs sender close
  close(clockHandle);
}
