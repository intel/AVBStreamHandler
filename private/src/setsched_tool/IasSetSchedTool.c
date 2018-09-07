/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasSetSchedTool.c
 * @brief   THE start of AVB Stream Handler
 * @details This is a helper command line tool to change the scheduling policy
 *          and priority for one more specified processes. The processes have to
 *          specified in decimal notation
 *
 * @date    2013
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>

#define VERSION_STR "1.0"
static const char *version_str = "Set Scheduler Tool v" VERSION_STR "\nCopyright (c) 2013, Intel Corporation\n";


// Options
static int optVerbose = 0;            // switch on verbosity
static int optPolicy = SCHED_OTHER;   // scheduler policy
static int optPrio = 0;               // scheduler priority

// usage message, exits with failure
static void usage( void )
{
  fprintf(stderr,
    "\n"
    "%s"
    "\n"
    "usage: setsched_tool options PID [...]"
    "\n"
    "options:\n"
    "    -h             show this message\n"
    "    -o <policy>    policy (o=SCHED_OTHER, r=SCHED_RR, f=SCHED_FIFO)\n"
    "    -p <prio>      priority 1...99\n"
    "PID:               list of process IDs (decimal notation)\n"
    "\n example:\n"
    " ./setsched_tool -or -p1 12345\n"
    "\n", version_str);

  exit(EXIT_FAILURE);
}


// return the policy as string
static char* policyStr(int policy)
{
  return (SCHED_RR == policy) ? "SCHED_RR" : (SCHED_FIFO == policy) ? "SCHED_FIFO" : "SCHED_OTHER";
}


// returns the a decimal string number as long int
static long int getLongInt(const char* str)
{
  return strtol(str, NULL, 0); // base is '0' which means 'decimal'
}

// the main function
int main(int argc, char **argv)
{
  int c;
  struct sched_param sp;

  for (;;)
  {
    c = getopt(argc, argv, "hp:o:v");

    if (c < 0)
    {
      break;
    }

    switch (c)
    {
      case 'h':
        usage();
        break;
      case 'o':
        optPolicy = ('r' == *optarg) ? SCHED_RR : ('f' == *optarg) ? SCHED_FIFO : SCHED_OTHER;
        break;
      case 'p':
        sscanf(optarg, "%2u", &optPrio);
        break;
      case 'v':
        optVerbose++;
        break;
      default:
        usage();
        break;
    }
  }

//  printf("optind  : %u, argc  : %u\n", optind, argc);

  // policy and prio has to be specified
  if (optind < 3 )
  {
    fprintf(stderr,"ERROR: Parameter missing!\n");
    usage();
  }

  // at least one PID has to be specified
  if (argc < 4 )
  {
    fprintf(stderr,"ERROR: PID missing!\n");
    usage();
  }

  // check range for priority
  if ((99 < optPrio) || (0 > optPrio))
  {
    fprintf(stderr,"ERROR: Priority out of range!\n");
    usage();
  }

  // print recognized parameter if verbose mode is on
  if (optVerbose > 0)
  {
    printf("policy: %s (%d)\n", policyStr(optPolicy), optPolicy);
    printf("prio  : %d\n", optPrio);
    for (int n = 4; n < argc; n++)
    {
      printf("PID %d : %ld\n", n-4, getLongInt(argv[n]));
    }
  }

  // set scheduling parameter
  sp.sched_priority = optPrio;

  for (int n = 4; n < argc; n++)
  {
    if (0 != sched_setscheduler((__pid_t)getLongInt(argv[n]), optPolicy, &sp))
    {
      fprintf(stderr,"ERROR: Colud not set scheduler parameter for PID=%ld!\n", getLongInt(argv[n]));
      exit(EXIT_FAILURE);
    }
  }

  if (optVerbose > 0)
  {
    printf("Scheduling parameter successfully changed\n");
  }

  exit(EXIT_SUCCESS);
}

// *** EOF ***
