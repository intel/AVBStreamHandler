/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _GST_AVBVIDEOSRC_H_
#define _GST_AVBVIDEOSRC_H_

#include <gst/base/gstpushsrc.h>

#include <dlt/dlt.h>

#include "media_transport/avb_video_bridge/IasAvbVideoBridge.h"

G_BEGIN_DECLS

#define GST_TYPE_AVBVIDEOSRC   (gst_avbvideosrc_get_type())
#define GST_AVBVIDEOSRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVBVIDEOSRC,GstAvbVideoSrc))
#define GST_AVBVIDEOSRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVBVIDEOSRC,GstAvbVideoSrcClass))
#define GST_IS_AVBVIDEOSRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVBVIDEOSRC))
#define GST_IS_AVBVIDEOSRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVBVIDEOSRC))

typedef struct _GstAvbVideoSrc GstAvbVideoSrc;
typedef struct _GstAvbVideoSrcClass GstAvbVideoSrcClass;

struct _GstAvbVideoSrc
{
    GstPushSrc base_avbvideosrc;

    DltContext log;

    struct ias_avbvideobridge_receiver *receiver;
    GAsyncQueue *queue;
    gchar *stream_name;
    gboolean done;
    guint stream_type;
};

struct _GstAvbVideoSrcClass
{
    GstPushSrcClass base_avbvideosrc_class;
};

GType gst_avbvideosrc_get_type(void);

G_END_DECLS

#endif
