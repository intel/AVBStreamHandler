/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/timex.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <iomanip>
#include <iostream>
#include <cstring>



const char * versionStr = "1.0.1";
const std::string clockDev = "/dev/ptp0";
clockid_t clockId = 0;

void help();
void setClock(int64_t offset);
void getClock();

int main(int argc, char* argv[])
{
  static const struct option options[] =
  {
    { "set",       required_argument, 0, 's' },
    { "get",             no_argument, 0, 'g'},
    { "help",            no_argument, 0, 'h'},
    { NULL, 0, 0, 0 }
  };


  std::cout << "PTP Time Control, Version " << versionStr << std::endl;

  if (1 == argc) // no options passed
  {
    printf("invalid parameter list, try --help\n");
    return -1;
  }

  while (1)
  {
    int32_t option_index = 0;

    int32_t opt = getopt_long(argc, argv, "s:gh", options, &option_index);
    if (-1 == opt)
    {
      break;
    }

    switch (opt)
    {
      case 'h':
      {
        help();
        break;
      }
      case 's':
      {
        int64_t offset = atoll(optarg);
        setClock(offset);
        break;
      }
      case 'g':
      {
        getClock();
        break;
      }

      default:
      {
        return -1;
      }
    }
  }

  return 0;
}

void help()
{
  std::cout << "Usage: avb_ptp_time_ctl [option(s)] " << std::endl;
  std::cout << "Options:\n" <<
  "\t -s <nanosec> or --set <nanosec>\tset clock - adjusts the PTP clock by the given +/- value [ns]\n" <<
  "\t -g or --get\t\t\t\tget clock - queries the current PTP clock\n" <<
  "\t -h or --help\t\t\t\tthis help text\n" << std::endl;
}


void getClock()
{
  int32_t clockHandle = open( clockDev.c_str(), O_RDWR );
  if (clockHandle < 0)
  {
    std::cerr << "Failed to open PTP clock device " << clockDev.c_str() << " (" << errno << "," << strerror(errno) << ")" << std::endl;
  }
  else
  {
    clockid_t clockId = ((~(clockid_t(clockHandle)) << 3) | 3);

    struct timespec tIn;
    memset(&tIn, 0, sizeof tIn);
    clock_gettime(clockId, &tIn);
    std::cout << "PTP clock: " << tIn.tv_sec << "." << std::setfill('0') << std::setw(9) << tIn.tv_nsec << std::endl;

    close(clockHandle);
  }
}


void setClock(int64_t offset)
{
  int32_t clockHandle = open( clockDev.c_str(), O_RDWR );
  if (clockHandle < 0)
  {
    std::cerr << "Failed to open PTP clock device " << clockDev.c_str() << " (" << errno << "," << strerror(errno) << ")" << std::endl;
  }
  else
  {
    clockid_t clockId = ((~(clockid_t(clockHandle)) << 3) | 3);
    std::cout << "Adjusting " << clockDev.c_str() << " by " << offset << "ns" << std::endl;

    struct timespec tIn;
    memset(&tIn, 0, sizeof tIn);
    clock_gettime(clockId, &tIn);
    std::cout << "before: " << tIn.tv_sec << "." << std::setfill('0') << std::setw(9) << tIn.tv_nsec << std::endl;

    struct timex t;
    memset(&t, 0, sizeof t);
    t.modes = ADJ_SETOFFSET | ADJ_NANO;
    if( offset >= 0 )
    {
      t.time.tv_sec  = offset / 1000000000LL;
      t.time.tv_usec = offset % 1000000000LL;
    }
    else
    {
      t.time.tv_sec  = (offset / 1000000000LL)-1;
      t.time.tv_usec = (offset % 1000000000LL)+1000000000;
    }
    if (syscall(__NR_clock_adjtime, clockId, &t) < 0)
    {
      std::cerr << "Failed to adjust clock (" << errno << "," << strerror(errno) << ")" << std::endl;
    }
    else
    {
      memset(&tIn, 0, sizeof tIn);
      clock_gettime(clockId, &tIn);
      std::cout << "after:  " << tIn.tv_sec << "." << std::setfill('0') << std::setw(9) << tIn.tv_nsec << std::endl;
    }

    close(clockHandle);
  }
}

