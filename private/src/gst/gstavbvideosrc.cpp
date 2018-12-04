/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include "gst/gstavbvideosrc.h"
#include "gst/gstavbclock.h"

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstdio>

GST_DEBUG_CATEGORY_STATIC(gst_avbvideosrc_debug_category);
#define GST_CAT_DEFAULT gst_avbvideosrc_debug_category

#define DEFAULT_STREAM_NAME "media_transport.avb.mpegts_streaming.7"
#define MPEG_TS_PACKET_SIZE 188
#define SPH_SIZE 4

/* prototypes */

static GstClock *gst_avbvideosrc_provide_clock(GstElement *element);
static void gst_avbvideosrc_finalize(GObject * object);

static GstFlowReturn gst_avbvideosrc_create(GstPushSrc * src, GstBuffer ** buf);

static void mpegts_callback(ias_avbvideobridge_receiver *receiver, bool sph, ias_avbvideobridge_buffer const * packet, void *user_ptr);
static void h264_callback(ias_avbvideobridge_receiver *receiver,
                          ias_avbvideobridge_buffer const * packet,
                          void *user_ptr);

static void gst_avbvideosrc_set_property(GObject *object, guint prop_id,
        const GValue *value, GParamSpec *pspec);
static void gst_avbvideosrc_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *pspec);
static GstStateChangeReturn gst_avbvideosrc_change_state(GstElement *element,
        GstStateChange transition);

enum
{
    PROP_0,
    PROP_STREAM_NAME,
    PROP_STREAM_TYPE
};

enum
{
    TYPE_MPEG_TS,
    TYPE_RTP_H264
};

#define GST_TYPE_AVBVIDEO_STREAM (gst_avb_video_src_get_stream_type ())
static GType
gst_avb_video_src_get_stream_type (void)
{
    static GType avb_video_src_stream_type = 0;
    static const GEnumValue stream_type[] = {
        {TYPE_MPEG_TS, "Represents an Mpeg-TS stream", "mpegts"},
        {TYPE_RTP_H264, "Represents an RTP encapsulated H.264 stream", "rtp-h264"},
        {0, NULL, NULL}
    };

    if (!avb_video_src_stream_type)
        avb_video_src_stream_type = g_enum_register_static ("GstAvbvideosrcStream", stream_type);

    return avb_video_src_stream_type;
}

/* pad templates */

static GstStaticPadTemplate gst_avbvideosrc_src_template =
GST_STATIC_PAD_TEMPLATE("src",
        GST_PAD_SRC,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS(
            "video/mpegts,"
            "  packetsize = (int) " G_STRINGIFY(MPEG_TS_PACKET_SIZE) ","
            "  systemstream = (boolean) true;"
            "application/x-rtp,"
            "  media = (string) \"video\", "
            "  clock-rate = (int) 90000, "
            "  encoding-name = (string) \"H264\""
            )
        );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstAvbVideoSrc, gst_avbvideosrc, GST_TYPE_PUSH_SRC,
        GST_DEBUG_CATEGORY_INIT(gst_avbvideosrc_debug_category, "avbvideosrc", 0,
            "debug category for avbvideosrc element"));

static void
gst_avbvideosrc_class_init(GstAvbVideoSrcClass * klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
    GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    gst_element_class_add_static_pad_template(GST_ELEMENT_CLASS(klass),
            &gst_avbvideosrc_src_template);

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
            "AVB-SH Source", "Source/Video/Network", "Retrieves MPEG-TS packets from AVB-SH",
            "Intel");

    gobject_class->finalize = gst_avbvideosrc_finalize;
    gobject_class->set_property = gst_avbvideosrc_set_property;
    gobject_class->get_property = gst_avbvideosrc_get_property;

    gstelement_class->provide_clock = gst_avbvideosrc_provide_clock;
    gstelement_class->change_state = gst_avbvideosrc_change_state;

    push_src_class->create = GST_DEBUG_FUNCPTR(gst_avbvideosrc_create);

    g_object_class_install_property(gobject_class, PROP_STREAM_NAME,
            g_param_spec_string("stream-name", "Stream name",
                "Name of the stream", DEFAULT_STREAM_NAME,
                (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobject_class, PROP_STREAM_TYPE,
            g_param_spec_enum("stream-type", "Stream type",
                "Format of the video stream", GST_TYPE_AVBVIDEO_STREAM, TYPE_MPEG_TS,
                (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gst_avbvideosrc_set_property(GObject *object, guint prop_id,
        const GValue *value, GParamSpec *pspec)
{
    GstAvbVideoSrc *avbvideosrc = GST_AVBVIDEOSRC(object);
    (void)pspec;

    if (prop_id == PROP_STREAM_NAME) {
        free(avbvideosrc->stream_name);
        avbvideosrc->stream_name = g_value_dup_string(value);

        GST_DEBUG_OBJECT(avbvideosrc, "Set stream name to [%s]", avbvideosrc->stream_name);
    } else if (prop_id == PROP_STREAM_TYPE) {
        avbvideosrc->stream_type = g_value_get_enum(value);

        GST_DEBUG_OBJECT(avbvideosrc, "Set stream type to [%u]", avbvideosrc->stream_type);
    }
}

static void gst_avbvideosrc_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *pspec)
{
    GstAvbVideoSrc *avbvideosrc = GST_AVBVIDEOSRC(object);

    if (prop_id == PROP_STREAM_NAME) {
        g_value_set_string(value, avbvideosrc->stream_name);
    } else if (prop_id == PROP_STREAM_TYPE) {
        g_value_set_enum(value, avbvideosrc->stream_type);
    } else {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static GstStateChangeReturn
gst_avbvideosrc_change_state(GstElement *element, GstStateChange transition)
{
    GstAvbVideoSrc *avbvideosrc = GST_AVBVIDEOSRC(element);
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    GstCaps *caps_event = NULL;

    if (transition == GST_STATE_CHANGE_NULL_TO_READY) {
        ias_avbvideobridge_result r;

        if (avbvideosrc->stream_type == TYPE_MPEG_TS) {
            caps_event = gst_caps_new_simple("video/mpegts",
                                             "packetsize", G_TYPE_INT, MPEG_TS_PACKET_SIZE,
                                             "systemstream", G_TYPE_BOOLEAN, TRUE,
                                             NULL);
        } else if (avbvideosrc->stream_type == TYPE_RTP_H264) {
            caps_event = gst_caps_new_simple("application/x-rtp",
                                             "media", G_TYPE_STRING, "video",
                                             "clock-rate", G_TYPE_INT, 90000,
                                             "encoding-name", G_TYPE_STRING, "H264",
                                             NULL);
        } else {
            GST_ERROR_OBJECT(avbvideosrc, "Invalid stream type");
            return GST_STATE_CHANGE_FAILURE;
        }

        if (!caps_event && !(gst_base_src_set_caps(GST_BASE_SRC(avbvideosrc), caps_event))) {
            GST_ERROR_OBJECT(avbvideosrc, "Failed to set caps on the source pad");
            return GST_STATE_CHANGE_FAILURE;
        }

        avbvideosrc->receiver = ias_avbvideobridge_create_receiver("receiver",
                avbvideosrc->stream_name);

        if (!avbvideosrc->receiver) {
            GST_ERROR_OBJECT(avbvideosrc, "Could not create avbvideobridge receiver [%s]",
                    avbvideosrc->stream_name);
            return GST_STATE_CHANGE_FAILURE;
        }

        if (avbvideosrc->stream_type == TYPE_MPEG_TS)
            r = ias_avbvideobridge_register_MpegTS_cb(avbvideosrc->receiver,
                    &mpegts_callback, avbvideosrc);
        else
            r = ias_avbvideobridge_register_H264_cb(avbvideosrc->receiver,
                    &h264_callback, avbvideosrc);

        if (r != IAS_AVB_RES_OK) {
            GST_ERROR_OBJECT(avbvideosrc, "Could not register receiver callback [%d]", r);
            ias_avbvideobridge_destroy_receiver(avbvideosrc->receiver);
            avbvideosrc->receiver = NULL;
            return GST_STATE_CHANGE_FAILURE;
        }
    }

    ret = GST_ELEMENT_CLASS(gst_avbvideosrc_parent_class)->change_state(element, transition);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        if (transition == GST_STATE_CHANGE_NULL_TO_READY) {
            ias_avbvideobridge_destroy_receiver(avbvideosrc->receiver);
            avbvideosrc->receiver = NULL;
        }
        return ret;
    }

    if (transition == GST_STATE_CHANGE_PLAYING_TO_PAUSED) {
        ret = GST_STATE_CHANGE_NO_PREROLL;
        avbvideosrc->done = TRUE;
    }

    if (transition == GST_STATE_CHANGE_READY_TO_NULL) {
        ias_avbvideobridge_destroy_receiver(avbvideosrc->receiver);
        avbvideosrc->receiver = NULL;
    }

    return ret;
}

static void
gst_avbvideosrc_init(GstAvbVideoSrc *avbvideosrc)
{
    gst_base_src_set_live(GST_BASE_SRC(avbvideosrc), TRUE);
    gst_base_src_set_format(GST_BASE_SRC(avbvideosrc), GST_FORMAT_TIME);

    avbvideosrc->queue = g_async_queue_new();
    if (!avbvideosrc->queue) {
        GST_ERROR_OBJECT(avbvideosrc, "Could not create async queue");
        return;
    }

    GST_OBJECT_FLAG_SET(avbvideosrc, GST_ELEMENT_FLAG_PROVIDE_CLOCK);

    avbvideosrc->stream_name = strdup(DEFAULT_STREAM_NAME);
    avbvideosrc->done = FALSE;
    avbvideosrc->stream_type = TYPE_MPEG_TS;
}

void
gst_avbvideosrc_finalize(GObject * object)
{
    GstAvbVideoSrc *avbvideosrc = GST_AVBVIDEOSRC(object);

    // TODO use g_async_queue_unref_full to clean up any remaining items
    g_async_queue_unref(avbvideosrc->queue);
    avbvideosrc->queue = NULL;

    free(avbvideosrc->stream_name);
    avbvideosrc->stream_name = NULL;

    G_OBJECT_CLASS(gst_avbvideosrc_parent_class)->finalize(object);
}

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
static GstFlowReturn
gst_avbvideosrc_create(GstPushSrc * src, GstBuffer ** buf)
{
    GstAvbVideoSrc *avbvideosrc = GST_AVBVIDEOSRC(src);

    do {
        *buf = (GstBuffer *)g_async_queue_timeout_pop(avbvideosrc->queue, 100000);

        /* If nothing is coming from the other side of the bridge, and the other
         * side is not updating this time, maybe it died... */
        if (*buf == NULL) {
            // The `/ 1000` is so that we have writerTime in microseconds, to compare with glib time
            guint64 writerTime = ias_avbvideobridge_last_receiver_access(avbvideosrc->receiver) / 1000;

            if ((g_get_monotonic_time() - writerTime) > (2 * G_USEC_PER_SEC)) {
                GST_ERROR_OBJECT(avbvideosrc, "Bridge appears down, more than two seconds without updates");
                return GST_FLOW_ERROR;
            }
        }
    } while (*buf == NULL && !avbvideosrc->done);

    return GST_FLOW_OK;
}

static GstClock *
gst_avbvideosrc_provide_clock(GstElement *element)
{
    (void)element;

    return gst_avbclock_obtain();
}

static void
mpegts_callback(ias_avbvideobridge_receiver *receiver, bool sph, ias_avbvideobridge_buffer const * packet, void *user_ptr)
{
    GstAvbVideoSrc *avbvideosrc = GST_AVBVIDEOSRC(user_ptr);
    unsigned int expected_size = sph ? MPEG_TS_PACKET_SIZE + SPH_SIZE : MPEG_TS_PACKET_SIZE;
    unsigned int offset = sph ? SPH_SIZE : 0;
    (void)receiver;

    if ((packet->size % expected_size) != 0) {
        GST_WARNING_OBJECT(avbvideosrc, "Packet may contain incomplete data. Size %zd is not a multiple of %u", packet->size, expected_size);
    }

    for (size_t j = 0; packet->size - j >= expected_size; j += expected_size) {
        GstBuffer *buf;
        guint8 *data = (guint8 *)packet->data;
        GST_BASE_SRC_CLASS(gst_avbvideosrc_parent_class)->alloc(GST_BASE_SRC_CAST(avbvideosrc), -1, MPEG_TS_PACKET_SIZE, &buf);
        GST_BUFFER_PTS(buf) = gst_avbclock_avtp_timestamp_to_pts(ntohl(*(unsigned*)(data + j)), GST_ELEMENT_CAST(avbvideosrc)->base_time);
        gst_buffer_fill(buf, 0, data + j + offset, MPEG_TS_PACKET_SIZE);
        g_async_queue_push(avbvideosrc->queue, buf);
    }
}

static void
h264_callback(ias_avbvideobridge_receiver *receiver, ias_avbvideobridge_buffer const * packet, void *user_ptr)
{
    GstAvbVideoSrc *avbvideosrc = GST_AVBVIDEOSRC(user_ptr);
    GstBuffer *buf;

    if (!receiver || !packet) {
        GST_ERROR_OBJECT(avbvideosrc, "Invalid receiver/packet");
        return;
    }

    GST_BASE_SRC_CLASS(gst_avbvideosrc_parent_class)->alloc(GST_BASE_SRC_CAST(avbvideosrc), -1,
                       (guint)packet->size, &buf);
    gst_buffer_fill(buf, 0, packet->data, packet->size);
    g_async_queue_push(avbvideosrc->queue, buf);
}
