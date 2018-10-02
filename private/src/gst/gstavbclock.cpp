/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "gst/gstavbclock.h"

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NSEC_PER_SEC 1000000000ULL
// Stolen from <linux-src>/include/linux/posix-timers.h
#define FD_TO_CLOCKID(FD) ((~FD) << 3) | 3;

GST_DEBUG_CATEGORY_STATIC(gst_avbclock_debug_category);
#define GST_CAT_DEFAULT gst_avbclock_debug_category

static GstClock *g_avbclock_singleton;

static GstClockTime gst_avbclock_get_internal_time(GstClock *object);

G_DEFINE_TYPE_WITH_CODE(GstAvbClock, gst_avbclock, GST_TYPE_SYSTEM_CLOCK,
        GST_DEBUG_CATEGORY_INIT(gst_avbclock_debug_category, "avbclock", 0,
            "debug category for avbclock element"));

static void
gst_avbclock_class_init(GstAvbClockClass * klass)
{
  GstClockClass *gstclock_class = GST_CLOCK_CLASS(klass);

  gstclock_class->get_internal_time = gst_avbclock_get_internal_time;
}

static void
gst_avbclock_init(GstAvbClock *avbclock)
{
    avbclock->device_fd = -1;
    avbclock->clock_id = -1;
}

static GstClockTime
gst_avbclock_get_internal_time(GstClock *object)
{
    GstAvbClock *avbclock = GST_AVBCLOCK(object);

    struct timespec ts;
    clock_gettime(avbclock->clock_id, &ts);
    GstClockTime time = GST_TIMESPEC_TO_TIME(ts);
    GST_DEBUG_OBJECT(avbclock, "Internal time: %lu", time);
    return time;
}

GstClockTime
gst_avbclock_avtp_timestamp_to_pts(unsigned avtp_timestamp, GstClockTime start_time)
{
    GstAvbClock *avbclock = GST_AVBCLOCK(gst_avbclock_obtain());

    struct timespec ts;
    clock_gettime(avbclock->clock_id, &ts);

    // Idea is to check how far in the future avtp_timestamp is compared to current avtp_timestamp
    // Modulo 2^32 is used to account for avtp_timestamp smaller than current avtp_timestamp
    unsigned current_avtp_timestamp = (unsigned)((ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec) % (1ULL << 32));
    GstClockTime pts = (avtp_timestamp - current_avtp_timestamp) % (1ULL << 32);

    GstClockTime now = GST_TIMESPEC_TO_TIME(ts);

    // Final pts is relative to running time, the time the pipeline is in PLAYING state
    return pts + (now - start_time);
}

static bool
open_clock(GstClock *object)
{
    GstAvbClock *avbclock = GST_AVBCLOCK(object);

    //TODO: get ptp device from network device or some configuration
    avbclock->device_fd = open("/dev/ptp0", O_RDONLY);
    if (avbclock->device_fd >= 0) {
        avbclock->clock_id = FD_TO_CLOCKID(avbclock->device_fd);
        if (avbclock->clock_id != -1) {
            return true;
        }
    }

    return false;
}

GstClock *
gst_avbclock_obtain(void)
{
    static GMutex avbclock_mutex;

    g_mutex_lock(&avbclock_mutex);

    if (g_avbclock_singleton == NULL) {
        GstClock *clock = (GstClock *)g_object_new(GST_TYPE_AVBCLOCK, "name", "GstAvbClock", NULL);
        if (open_clock(clock)) {
            g_avbclock_singleton = clock;
        }
    } else {
        g_object_ref(g_avbclock_singleton);
    }

    g_mutex_unlock(&avbclock_mutex);

    return g_avbclock_singleton;
}
