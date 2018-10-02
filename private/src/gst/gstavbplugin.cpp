/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <gst/gst.h>
#include "gst/gstavbvideosink.h"
#include "gst/gstavbvideosrc.h"

static gboolean
plugin_init(GstPlugin * plugin)
{
    gst_element_register(plugin, "avbvideosink", GST_RANK_NONE,
            GST_TYPE_AVBVIDEOSINK);
    gst_element_register(plugin, "avbvideosrc", GST_RANK_NONE,
            GST_TYPE_AVBVIDEOSRC);

    return TRUE;
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "gstreamer-avb"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "gst_avb_plugin"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://01.org/"
#endif

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
        GST_VERSION_MINOR,
        ias_media_transport_gst_avb_video_plugin,
        "AVB-SH GStreamer plugin sample",
        plugin_init, VERSION, "BSD", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
