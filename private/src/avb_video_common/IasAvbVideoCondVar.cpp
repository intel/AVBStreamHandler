/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/

#include "avb_video_common/IasAvbVideoCondVar.hpp"

#include <cerrno>
#include <climits>
#include <iostream>

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#define MSEC_PER_SEC 1000
#define NSEC_PER_SEC 1000000000
#define NSEC_PER_MSEC 1000000

namespace IasMediaTransportAvb {

#define TIMESPEC_TO_MSEC(ts) ((ts.tv_sec * MSEC_PER_SEC) + (ts.tv_nsec / NSEC_PER_MSEC))

IasAvbVideoCondVar::IasAvbVideoCondVar()
    :mFutex(0)
{
  // Nothing to do?
}

IasAvbVideoCondVar::~IasAvbVideoCondVar()
{
  // Could we check if there's someone waiting? Should we send a broadcast here?
}

/**
 * Some notes about futex usage in this class.
 *
 * Usually a futex - as fast userspace mutex - will have a varying value, but
 * in this class one can notice that mFutex is always 0.
 *
 * IasAvbVideoCondVar provides for a very specific use case: multiple readers,
 * that can read together from a ringbuffer, as long as there is something to read.
 * "As long as there is something to read" is verified on IasAvbVideoRingBufferShm
 * class `waitRead` method. If this predicate is true, there will be no wait. When
 * it is false (so there's nothing to read) the thread/process will wait. The writter
 * process is the one who adds things to the ringbuffer, so it's the one who can
 * change this predicate. When it does so, it issues a `broadcast` on the condition
 * variable, to awake all readers that were waiting. So, the truth value of the
 * predicate is controlled by IasAvbVideoRingBufferShm, and the futex here is only
 * used to keep a list of threads/process waiting and wake them up when needed.
 *
 * In the end, the value of the mutex is not important in this use case - setting it
 * to any value to replicate predicate truth value is unnecessary.
 *
 * Race conditions
 *
 * One can argue that this simple approach is prone to race conditions: what if a thread
 * decides to wait and before actually sleeping the writer sends a broadcast, so reader
 * starts to wait but the broadcast was lost?
 *
 * That's a valid concern. This class could then use different values for mFutex to
 * have some control over this. For instance, `broadcast` could set it to "1", so
 * that a reader would only sleep if the mFutex value is not "1". While this is a simple
 * solution, it adds new problems: who sets the value to "0"? The first thread trying
 * to wait? If so, it will always find it as "1", not sleep just to find out that the
 * predicate didn't change, then sleep. If first thread to awake takes this task,
 * we can still risk a third thread going to sleep unnecessarily if it was blocked after
 * checking the predicate and unblocked after first thread change value to "0".
 *
 * A simple approach is that no thread can wait forever, but only for a certain time.
 * This way, even if it goes to sleep unnecessarily, it will soon awake due timeout,
 * and all code dealing with the ringbuffer simply accepts that timeout is not an issue,
 * unless it happens many times. This simpler approach is the one used by this class.
 *
 * Naturally, if the race condition case become so common that affects performance,
 * it's possible to revisit this class and add some code to squeeze the probability of
 * it happening.
 *
 * Finally, note that the way ringbuffer works prevent the case in which a reader
 * decides to sleep and writer sends broadcast just before reader actually sleeps
 * happening twice: after the reader awakes on timeout, predicate will still be
 * true, as ringbuffer waits all readers to read.
 *
 * Mutex for the condition variable
 *
 * POSIX threads condition variables require a mutex to work with, but this class
 * does not. As one of the goals of this condition variable is to awake all readers
 * and all of them can read the ringbuffer at the same time, requiring a mutex would
 * defeat this purpose.
 *
 * Writer also uses this condition variable
 *
 * IasAvbVideoRingBufferShm class also uses a IasAvbVideoCondVar for the writer. The
 * reason is to avoid keeping more than one implementation of condition variables,
 * with different idiosyncrasies. For instance, the need for a mutex for POSIX
 * condition variables.
 *
 * General case
 *
 * As previous comments note, this condition variable is not generic and may not
 * fit different use cases (or even IasAvbVideoRingBufferShm if it changes).
 */
IasAvbVideoCondVar::IasResult IasAvbVideoCondVar::wait(uint64_t time_ms)
{
  struct timespec ts;
  uint64_t enter_time;
  IasResult result = eIasOk;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
    return eIasClockTimeNotAvailable;
  }
  enter_time = TIMESPEC_TO_MSEC(ts);

  ts.tv_sec = time_ms / MSEC_PER_SEC;
  ts.tv_nsec = (time_ms % MSEC_PER_SEC) * NSEC_PER_MSEC;

  while (true)
  {
    int ret = futex(&mFutex, FUTEX_WAIT, 0, &ts, NULL, 0);
    if (ret == 0)
    {
      break;
    }
    else if (errno == EINTR)
    {
      // How much time has passed?
      if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
          result = eIasClockTimeNotAvailable;
      }
      else
      {
        uint64_t elapsed_time = TIMESPEC_TO_MSEC(ts) - enter_time;
        if (elapsed_time > time_ms)
        {
          // Ok, we were interrupted, but it was about time
          break;
        }
        else
        {
          // Wait again, but for remaining time only
          time_ms = time_ms - elapsed_time;

          ts.tv_sec = time_ms / MSEC_PER_SEC;
          ts.tv_nsec = (time_ms % MSEC_PER_SEC) * NSEC_PER_MSEC;
          continue;
        }
      }
    }
    else if (errno == ETIMEDOUT)
    {
      result = eIasTimeout;
      break;
    }
    else
    {
      result = eIasCondWaitFailed;
      break;
    }
  }

  return result;
}

IasAvbVideoCondVar::IasResult IasAvbVideoCondVar::broadcast()
{
  int ret;
  IasResult result = eIasOk;

  ret = futex(&mFutex, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);

  if (ret == -1)
  {
    result = eIasCondBroadcastFailed;
  }

  return result;
}

int IasAvbVideoCondVar::futex(int *uaddr, int futex_op, int val,
    const struct timespec *timeout, int *uaddr2, int val3)
{
  return (int)syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

}
