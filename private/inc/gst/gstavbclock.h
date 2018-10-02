/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _GST_AVBCLOCK_H_
#define _GST_AVBCLOCK_H_

#include <gst/gstsystemclock.h>

#include "media_transport/avb_video_bridge/IasAvbVideoBridge.h"

#include <time.h>

G_BEGIN_DECLS

#define GST_TYPE_AVBCLOCK   (gst_avbclock_get_type())
#define GST_AVBCLOCK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVBCLOCK,GstAvbClock))
#define GST_AVBCLOCK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVBCLOCK,GstAvbClockClass))
#define GST_IS_AVBCLOCK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVBCLOCK))
#define GST_IS_AVBCLOCK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVBCLOCK))

typedef struct _GstAvbClock GstAvbClock;
typedef struct _GstAvbClockClass GstAvbClockClass;

struct _GstAvbClock
{
    GstSystemClock base_avbclock;

    clockid_t clock_id;
    int device_fd;
};

struct _GstAvbClockClass
{
    GstSystemClockClass base_avbclocklass;
};

GType gst_avbclock_get_type(void);
GstClock *gst_avbclock_obtain(void);
GstClockTime gst_avbclock_avtp_timestamp_to_pts(unsigned avtp_timestamp, GstClockTime start_time);

G_END_DECLS

#endif
