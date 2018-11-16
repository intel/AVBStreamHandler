/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoBridge.cpp
 * @brief   This is the implementation of the C-API
 * @details This is the interface that allows calls from C-domain.
 * @date    2018
 */

#include "avb_helper/ias_visibility.h"
#include "media_transport/avb_video_bridge/IasAvbVideoBridge.h"
#include "avb_video_common/IasAvbVideoLog.hpp"
#include "avb_video_bridge/IasAvbVideoSender.hpp"
#include "avb_video_bridge/IasAvbVideoReceiver.hpp"

namespace IasMediaTransportAvb {


#if defined( __cplusplus )
extern "C"
{
#endif

IAS_DSO_PUBLIC void ias_avbvideobridge_register_log_context(DltContext *dlt_context)
{
    IasAvbVideoLog::setDltContext(dlt_context);
}

IAS_DSO_PUBLIC ias_avbvideobridge_sender* ias_avbvideobridge_create_sender(const char* senderRole)
{
  IasAvbVideoSender *sender = new IasAvbVideoSender(senderRole);
  if (nullptr != sender)
  {
    ias_avbvideobridge_result result = sender->init();
    if (result != IAS_AVB_RES_OK)
    {
      delete sender;
      sender = NULL;
    }
  }
  return reinterpret_cast<ias_avbvideobridge_sender*>(sender);
}


IAS_DSO_PUBLIC ias_avbvideobridge_receiver* ias_avbvideobridge_create_receiver(const char* instance_name,
                                                                              const char* receiverRole)
{
  (void) instance_name; // not used ATM

  IasAvbVideoReceiver *receiver = new IasAvbVideoReceiver(receiverRole);
  if (nullptr != receiver)
  {
    ias_avbvideobridge_result result = receiver->init();
    if (result != IAS_AVB_RES_OK)
    {
      delete receiver;
      receiver = NULL;
    }
  }
  return reinterpret_cast<ias_avbvideobridge_receiver*>(receiver);
}


IAS_DSO_PUBLIC void ias_avbvideobridge_destroy_sender(ias_avbvideobridge_sender* inst)
{
  if (NULL != inst)
  {
    delete reinterpret_cast<IasAvbVideoSender*>(inst);
  }
}


IAS_DSO_PUBLIC void ias_avbvideobridge_destroy_receiver(ias_avbvideobridge_receiver* inst)
{
  if (NULL != inst)
  {
    delete reinterpret_cast<IasAvbVideoReceiver*>(inst);
  }
}


IAS_DSO_PUBLIC ias_avbvideobridge_result ias_avbvideobridge_send_packet_H264(ias_avbvideobridge_sender* inst,
                                                              ias_avbvideobridge_buffer const * packet)
{
  ias_avbvideobridge_result res = IAS_AVB_RES_NULL_PTR;
  if (NULL != inst)
  {
    res = reinterpret_cast<IasAvbVideoSender*>(inst)->sendPacketH264(packet);
  }

  return res;
}


IAS_DSO_PUBLIC ias_avbvideobridge_result ias_avbvideobridge_send_packet_MpegTs(ias_avbvideobridge_sender* inst,
                                                                bool sph,
                                                                ias_avbvideobridge_buffer const * packet)
{
  ias_avbvideobridge_result res = IAS_AVB_RES_NULL_PTR;
  if (NULL != inst)
  {
    res = reinterpret_cast<IasAvbVideoSender*>(inst)->sendPacketMpegTs(sph, packet);
  }

  return res;
}


IAS_DSO_PUBLIC ias_avbvideobridge_result ias_avbvideobridge_register_H264_cb(ias_avbvideobridge_receiver* inst,
                                                              ias_avbvideobridge_receive_H264_cb cb,
                                                              void* user_ptr)
{
  ias_avbvideobridge_result res = IAS_AVB_RES_NULL_PTR;

  if (NULL != inst)
  {
    res = reinterpret_cast<IasAvbVideoReceiver*>(inst)->setCallback(cb, user_ptr);
  }

  return res;
}


IAS_DSO_PUBLIC ias_avbvideobridge_result ias_avbvideobridge_register_MpegTS_cb(ias_avbvideobridge_receiver* inst,
                                                                ias_avbvideobridge_receive_MpegTS_cb cb,
                                                                void* user_ptr)
{
  ias_avbvideobridge_result res = IAS_AVB_RES_NULL_PTR;

  if (NULL != inst)
  {
    res = reinterpret_cast<IasAvbVideoReceiver*>(inst)->setCallback(cb, user_ptr);
  }

  return res;
}

IAS_DSO_PUBLIC uint64_t ias_avbvideobridge_last_receiver_access(ias_avbvideobridge_receiver* inst)
{
  if (NULL != inst)
  {
    return reinterpret_cast<IasAvbVideoReceiver*>(inst)->getLastStreamWriteAccess();
  }

  return 0;
}

#if defined( __cplusplus )
}
#endif

} // namespace IasMediaTransportAvb



