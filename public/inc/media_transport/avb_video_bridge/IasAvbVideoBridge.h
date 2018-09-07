/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoBridge.h
 * @brief   This is the public header of the AVB Video Bridge library.
 * @details This header declares the C-functions, the buffer stuct and result values provided by the bridge library.
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_AVBVIDEOBRIDGE_AVBVIDEOBRIDGE_H
#define IAS_MEDIATRANSPORT_AVBVIDEOBRIDGE_AVBVIDEOBRIDGE_H


#include <stddef.h>


#if defined( __cplusplus )
extern "C"
{
#endif


/**
 * @brief Result values for used by this interface.
 *
 */
typedef enum _ias_avbvideobridge_result
{
  IAS_AVB_RES_OK,               //!< Operation successful
  IAS_AVB_RES_FAILED,           //!< Operation failed (e.g. register callback for a sender instance)
  IAS_AVB_RES_OUT_OF_MEMORY,    //!< Out of memory during memory allocation
  IAS_AVB_RES_NULL_PTR,         //!< One of the parameters was a null pointer
  IAS_AVB_RES_TIMEOUT,          //!< Timeout occurred
  IAS_AVB_RES_NO_SPACE,         //!< No space left in ring buffer
  IAS_AVB_RES_EMPTY,            //!< Ringbuffer is empty (no data available)
  IAS_AVB_RES_PAYLOAD_TOO_LARGE //!< Payload to be copied exceeds buffer size provided in SHM
} ias_avbvideobridge_result;


/**
 * @brief Declaration of the buffer used for streaming data
 *
 * A buffer contains a pointer to some data and the size of the data.
 * The buffer always belongs to the caller of a function. The data
 * is valid throughout the call. The caller is responsible for the
 * memory management.
 */
typedef struct _ias_avbvideobridge_buffer
{
  /**
   * Size of the data in bytes
   */
  size_t size;

  /**
   * Pointer to the data
   */
  void * data;
} ias_avbvideobridge_buffer;


/**
 * @brief declaration struct representing an avbvideobridge sender instance.
 *
 * Instance capable to send data via AVB
 */
struct ias_avbvideobridge_sender;


/**
 * @brief declaration struct representing an avbvideobridge receiver instance.
 *
 * Instance capable to receive data via AVB
 */
struct ias_avbvideobridge_receiver;


/**
 * @brief Create a sender instance of the avbvideobridge.
 *
 * @param[in] sender_role The rolename of the sender instance to create.
 *
 * @return A pointer to the instance or nullptr in case of an
 * error (e.g., if the avbvideobridge cannot be initialized). This
 * instance can only be used for sending streams.
 * There is no general limit in the amount of instances. The
 * application calling this function is responsible to destroy
 * the instances by calling the destroy method after use (e.g. during system shutdown).
 */
ias_avbvideobridge_sender* ias_avbvideobridge_create_sender(const char* sender_role);


/**
 * @brief Create a receiver instance of the avbvideobridge.
 *
 * @param[in] instance_name The instance name of the receiver.
 * @param[in] receiver_role The rolename of the receiver instance to create.
 *
 * @return A pointer to the instance or nullptr in case of an
 * error (e.g., if the avbvideobridge cannot be initialized). This
 * instance can only be used for receiving streams.
 * There is no general limit in the amount of instances. The
 * application calling this function is responsible to destroy
 * the instances by calling the destroy method after use (e.g. during system shutdown).
 */
ias_avbvideobridge_receiver* ias_avbvideobridge_create_receiver(const char* instance_name, const char* sender_role);


/**
 * @brief Destroy the previously created avbvideobridge sender instance.
 *
 * @param[in] inst instance The pointer to the sender instance to destroy.
 */
void ias_avbvideobridge_destroy_sender(ias_avbvideobridge_sender* inst);


/**
 * @brief Destroy the previously created avbvideobridge receiver instance.
 *
 * @param[in] inst instance The pointer to the receiver instance to destroy.
 */
void ias_avbvideobridge_destroy_receiver(ias_avbvideobridge_receiver* inst);


/**
 * @brief Push H.264 data packet.
 *
 * @param[in] inst The instance to send from.
 * @param[in] packet The buffer to send.
 */
ias_avbvideobridge_result ias_avbvideobridge_send_packet_H264(ias_avbvideobridge_sender* inst, ias_avbvideobridge_buffer const * packet);


/**
 * @brief Push MPEG-TS data packet.
 *
 * @param[in] inst The instance to send from.
 * @param[in] sph Flag if a source packet header (sph) is used.
 * @param[in] packet The buffer to send.
 */
ias_avbvideobridge_result ias_avbvideobridge_send_packet_MpegTs(ias_avbvideobridge_sender* inst , bool sph, ias_avbvideobridge_buffer const * packet);


/**
 * @brief definition of the H.264 receiver callback function.
 *
 * @param[in] inst The instance which received the packet.
 * @param[in] packet The buffer received by the instance.
 * @param[in] user_ptr The user-data pointer. The value can be set on callback registration.
 *
 */
typedef void (*ias_avbvideobridge_receive_H264_cb)(ias_avbvideobridge_receiver* inst, ias_avbvideobridge_buffer const * packet, void* user_ptr);


/**
 * @brief definition of the MPEG-TS receiver callback function.
 *
 * @param[in] inst The instance which received the packet.
 * @param[in] sph Flag if a source packet header (sph) is used.
 * @param[in] packet The buffer received by the instance.
 * @param[in] user_ptr The user-data pointer. The value can be set on callback registration.
 *
 */
typedef void (*ias_avbvideobridge_receive_MpegTS_cb)(ias_avbvideobridge_receiver* inst, bool sph, ias_avbvideobridge_buffer const * packet, void* user_ptr);


/**
 * @brief Register the callback function for H264 receiver.
 *
 * It is only possible to register one H264 callback per receiver. An error will be returned on re-registration.
 *
 * @param[in] inst Instance which is asked to call the callback.
 * @param[in] cb Pointer to the callback.
 * @param[in] user_ptr The user-data pointer. The value is passed back into the callback.
 */
ias_avbvideobridge_result ias_avbvideobridge_register_H264_cb(ias_avbvideobridge_receiver* inst, ias_avbvideobridge_receive_H264_cb cb, void* user_ptr);


/**
 * @brief Register the callback function for MpegTS receiver.
 *
 * It is only possible to register one MpegTS callback per receiver. An error will be returned on re-registration.
 *
 * @param[in] inst Instance which is asked to call the callback.
 * @param[in] cb Pointer to the callback.
 * @param[in] user_ptr The user-data pointer. The value is passed back into the callback.
 */
ias_avbvideobridge_result ias_avbvideobridge_register_MpegTS_cb(ias_avbvideobridge_receiver* inst, ias_avbvideobridge_receive_MpegTS_cb cb, void* user_ptr);


#if defined( __cplusplus )
}
#endif


#endif /* IAS_MEDIATRANSPORT_AVBVIDEOBRIDGE_AVBVIDEOBRIDGE_H */
