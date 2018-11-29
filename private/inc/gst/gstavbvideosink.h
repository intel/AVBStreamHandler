/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _GST_AVBVIDEOSINK_H_
#define _GST_AVBVIDEOSINK_H_

#include <gst/base/gstbasesink.h>

#include "media_transport/avb_video_bridge/IasAvbVideoBridge.h"

G_BEGIN_DECLS

#define GST_TYPE_AVBVIDEOSINK   (gst_avbvideosink_get_type())
#define GST_AVBVIDEOSINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVBVIDEOSINK,GstAvbVideoSink))
#define GST_AVBVIDEOSINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVBVIDEOSINK,GstAvbVideoSinkClass))
#define GST_IS_AVBVIDEOSINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVBVIDEOSINK))
#define GST_IS_AVBVIDEOSINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVBVIDEOSINK))

typedef struct _GstAvbVideoSink GstAvbVideoSink;
typedef struct _GstAvbVideoSinkClass GstAvbVideoSinkClass;

struct _GstAvbVideoSink
{
    GstBaseSink base_avbvideosink;

    struct ias_avbvideobridge_sender *sender;
    gchar *stream_name;
    gboolean is_mpegts;
};

struct _GstAvbVideoSinkClass
{
    GstBaseSinkClass base_avbvideosink_class;
};

GType gst_avbvideosink_get_type(void);

G_END_DECLS

#endif
