/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include "gst/gstavbvideosink.h"

GST_DEBUG_CATEGORY_STATIC(gst_avbvideosink_debug_category);
#define GST_CAT_DEFAULT gst_avbvideosink_debug_category

#define DEFAULT_STREAM_NAME "media_transport.avb.mpegts_streaming.1"
#define MPEG_TS_PACKET_SIZE 188

/* prototypes */

static void gst_avbvideosink_finalize(GObject *object);

static GstFlowReturn gst_avbvideosink_render(GstBaseSink *sink,
        GstBuffer *buffer);

static void gst_avbvideosink_set_property(GObject *object, guint prop_id,
        const GValue *value, GParamSpec *pspec);
static void gst_avbvideosink_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *pspec);
static GstStateChangeReturn gst_avbvideosink_change_state(GstElement *element,
        GstStateChange transition);
static gboolean gst_avbvideosink_event(GstBaseSink *sink, GstEvent *event);

enum
{
    PROP_0,
    PROP_STREAM_NAME
};

/* pad templates */

static GstStaticPadTemplate gst_avbvideosink_sink_template =
GST_STATIC_PAD_TEMPLATE("sink",
        GST_PAD_SINK,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS(
            "video/mpegts,"
            "  packetsize = (int) " G_STRINGIFY(MPEG_TS_PACKET_SIZE) ";"
            "application/x-rtp,"
            "  media = (string) \"video\", "
            "  clock-rate = (int) 90000,"
            "  encoding-name = (string) \"H264\""
            )
        );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstAvbVideoSink, gst_avbvideosink, GST_TYPE_BASE_SINK,
        GST_DEBUG_CATEGORY_INIT(gst_avbvideosink_debug_category, "avbvideosink", 0,
            "debug category for avbvideosink element"));

static void
gst_avbvideosink_class_init(GstAvbVideoSinkClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
    GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    gst_element_class_add_static_pad_template(GST_ELEMENT_CLASS(klass),
            &gst_avbvideosink_sink_template);

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
            "AVB-SH Sink", "Sink/Video/Network", "Sends MPEG-TS packets to AVB-SH",
            "Intel");

    gobject_class->finalize = gst_avbvideosink_finalize;
    gobject_class->set_property = gst_avbvideosink_set_property;
    gobject_class->get_property = gst_avbvideosink_get_property;

    gstelement_class->change_state = gst_avbvideosink_change_state;

    base_sink_class->render = GST_DEBUG_FUNCPTR(gst_avbvideosink_render);
    base_sink_class->event = GST_DEBUG_FUNCPTR(gst_avbvideosink_event);
    // TODO render_list, if available, is used when using mpegtsmux before
    // avbvideosink in the pipeline.
    //  base_sink_class->render_list = GST_DEBUG_FUNCPTR(gst_avbvideosink_render_list);

    g_object_class_install_property(gobject_class, PROP_STREAM_NAME,
            g_param_spec_string("stream-name", "Stream name",
                "Name of the stream", DEFAULT_STREAM_NAME,
                (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gst_avbvideosink_set_property(GObject *object, guint prop_id,
        const GValue *value, GParamSpec *pspec)
{
    GstAvbVideoSink *avbvideosink = GST_AVBVIDEOSINK(object);
    (void)pspec;

    if (prop_id == PROP_STREAM_NAME) {
        free(avbvideosink->stream_name);
        avbvideosink->stream_name = g_value_dup_string(value);
        GST_DEBUG_OBJECT(avbvideosink, "Set stream name to [%s]", avbvideosink->stream_name);
    }
}

static void gst_avbvideosink_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *pspec)
{
    GstAvbVideoSink *avbvideosink = GST_AVBVIDEOSINK(object);

    if (prop_id == PROP_STREAM_NAME) {
        g_value_set_string(value, avbvideosink->stream_name);
    } else {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static GstStateChangeReturn
gst_avbvideosink_change_state(GstElement *element, GstStateChange transition)
{
    GstAvbVideoSink *avbvideosink = GST_AVBVIDEOSINK(element);
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

    if (transition == GST_STATE_CHANGE_NULL_TO_READY) {
        avbvideosink->sender = ias_avbvideobridge_create_sender(avbvideosink->stream_name);
        if (!avbvideosink->sender) {
            GST_ERROR_OBJECT(avbvideosink, "Could not create avbvideobridge sender [%s]", avbvideosink->stream_name);
            return GST_STATE_CHANGE_FAILURE;
        }
    }

    ret = GST_ELEMENT_CLASS(gst_avbvideosink_parent_class)->change_state(element, transition);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        if (transition == GST_STATE_CHANGE_NULL_TO_READY) {
            ias_avbvideobridge_destroy_sender(avbvideosink->sender);
            avbvideosink->sender = NULL;
        }
        return ret;
    }

    if (transition == GST_STATE_CHANGE_READY_TO_NULL) {
        ias_avbvideobridge_destroy_sender(avbvideosink->sender);
        avbvideosink->sender = NULL;
    }

    return ret;
}

static void
gst_avbvideosink_init(GstAvbVideoSink *avbvideosink)
{
    avbvideosink->stream_name = strdup(DEFAULT_STREAM_NAME);
    avbvideosink->is_mpegts = TRUE;
}

void
gst_avbvideosink_finalize(GObject * object)
{
    GstAvbVideoSink *avbvideosink = GST_AVBVIDEOSINK(object);

    free(avbvideosink->stream_name);
    avbvideosink->stream_name = NULL;

    G_OBJECT_CLASS(gst_avbvideosink_parent_class)->finalize(object);
}

static gboolean gst_avbvideosink_event(GstBaseSink *sink, GstEvent *event)
{
    GstAvbVideoSink *avbvideosink = GST_AVBVIDEOSINK(sink);
    gboolean ret = TRUE;

    GST_DEBUG_OBJECT (avbvideosink, "Received event %s",
                     gst_event_type_get_name (GST_EVENT_TYPE (event)));

    switch (GST_EVENT_TYPE (event)) {
        case GST_EVENT_CAPS:
            {
                GstCaps *caps;
                GstStructure *video_format;

                gst_event_parse_caps (event, &caps);

                if (caps) {
                    video_format = gst_caps_get_structure(caps, 0);
                } else {
                    GST_ERROR_OBJECT (avbvideosink, "Caps parsing failed");
                    ret = FALSE;
                    goto unref;
                }

                const gchar *vf_type = gst_structure_get_name(video_format);
                if (strcmp (vf_type, "application/x-rtp") == 0)
                    avbvideosink->is_mpegts = FALSE;
                else if (strcmp (vf_type, "video/mpegts") == 0)
                    avbvideosink->is_mpegts = TRUE;

                GST_DEBUG_OBJECT (avbvideosink, "Using video-format %s",
                             vf_type);
            }
            break;
        default:
            break;
    }

unref:
    gst_event_unref (event);

    return ret;
}

static GstFlowReturn
gst_avbvideosink_render(GstBaseSink * sink, GstBuffer * buffer)
{
    GstAvbVideoSink *avbvideosink = GST_AVBVIDEOSINK(sink);
    GstMapInfo map;
    ias_avbvideobridge_result r;
    const char *pkt_type = avbvideosink->is_mpegts ? "MPEG-TS" : "RTP-H.264";


    gst_buffer_map(buffer, &map, GST_MAP_READ);

    gsize size = gst_buffer_get_size(buffer);

    if (avbvideosink->is_mpegts && (size != MPEG_TS_PACKET_SIZE)) {
        // TODO avmux_mpegts doesn't generate 188-bytes packets, but any arbitrary number. It even breaks
        // a 188-bytes packet so that one call to gst_avbvideosink_render may receive the first part of the
        // bytes, and the next call receives the remaining bytes. So, these cases, where a muxer
        // generates "strange" packets, need to be considered.
        // mpegtsmux otoh, sends to gst_avbvideosink_render only 188 bytes packets.
        gst_buffer_unmap(buffer, &map);
        GST_ERROR_OBJECT(avbvideosink,
                "Handling of MPEG-TS buffers with more than one "
                G_STRINGIFY(MPEG_TS_PACKET_SIZE) "-byte packets not suppported");
        return GST_FLOW_NOT_NEGOTIATED;
    }

    ias_avbvideobridge_buffer avb_buffer;
    avb_buffer.size = size;
    avb_buffer.data = map.data;

    if (avbvideosink->is_mpegts)
        r = ias_avbvideobridge_send_packet_MpegTs(avbvideosink->sender, false, &avb_buffer);
    else
        r = ias_avbvideobridge_send_packet_H264(avbvideosink->sender, &avb_buffer);

    if (r != IAS_AVB_RES_OK) {
        GST_ERROR_OBJECT(avbvideosink, "Failed to send %s packet [%d]", pkt_type, r);
        return GST_FLOW_ERROR;
    }

    GST_DEBUG_OBJECT(avbvideosink, "Sent %s packet to avb-sh: %d", pkt_type, r);

    gst_buffer_unmap(buffer, &map);

    return GST_FLOW_OK;
}
