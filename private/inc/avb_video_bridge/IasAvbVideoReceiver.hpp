/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoReceiver.hpp
 * @brief   Definition of the video receiver.
 * @details The video receiver is created by a video client application that uses the video bridge library to connect
 *          to the AVB Streamhandler. Therefore it has to pass a receiver role name. This role name is used as
 *          the name for the underlying data exchange mechanism (e.g. shared memory) and is basically the name of
 *          the video stream handled by this receiver instance.
 *          To wait for video data packets coming from the AVB stack a receiver thread is created. In this thread
 *          a callback method is called which has to be registered by the client. By using this method the received
 *          data is passed to the client video application.
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_AVBVIDEOBRIDGE_AVBVIDEORECEIVER_HPP
#define IAS_MEDIATRANSPORT_AVBVIDEOBRIDGE_AVBVIDEORECEIVER_HPP

#include <string>
#include <atomic>
#include <mutex>
#include <thread>

#include "media_transport/avb_video_bridge/IasAvbVideoBridge.h"
#include "avb_video_common/IasAvbVideoShmConnection.hpp"
#include "avb_video_common/IasAvbVideoRingBuffer.hpp"


namespace IasMediaTransportAvb {

class __attribute__ ((visibility ("default"))) IasAvbVideoReceiver
{
  public:

   /*!
    * @brief Constructor
    *
    * @param[in] receiverRole The name of the receiver and so the name of the video stream.
    */
    explicit IasAvbVideoReceiver(const char* receiverRole);

    /*!
     * @brief Destructor
     */
    ~IasAvbVideoReceiver();

    /*!
     * @brief Inits the receiver class.
     *
     * @returns                IAS_AVB_RES_OK on success, otherwise an error code.
     */
    ias_avbvideobridge_result init();

    /*!
     * @brief Register a client method that is called on reception of H.264 video packets.
     *
     * @param[in] cb callback Function to be called on reception of H.264 video packets.
     * @param[in] userPtr     Pointer to some user data. Can be used to assign packets on application side.
     *
     * @returns                IAS_AVB_RES_OK on success, otherwise an error code.
     */
    ias_avbvideobridge_result setCallback(ias_avbvideobridge_receive_H264_cb cb, void* userPtr);

    /*!
     * @brief Register a client method that is called on reception of MPEG-TS video packets.
     *
     * @param[in] cb callback Function to be called on reception of MPEG-TS video packets.
     * @param[in] userPtr     Pointer to some user data. Can be used to assign packets on application side.
     *
     * @returns                IAS_AVB_RES_OK on success, otherwise an error code.
     */
    ias_avbvideobridge_result setCallback(ias_avbvideobridge_receive_MpegTS_cb cb, void* userPtr);

    // currently not used
    uint32_t getInstCounter() { return uint32_t(mNumberInstances); }

  private:

    enum receivingFormat
    {
      eFormatUnknown,   //!< type not set yet
      eFormatH264,      //!< the receiver receives data of format H.264
      eFormatMpegTs     //!< the receiver receives data of format MpegTS
    };

    /*!
     *  @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbVideoReceiver(IasAvbVideoReceiver const &other);

    /*!
     *  @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbVideoReceiver& operator=(IasAvbVideoReceiver const &other);

    /*!
     * @brief Creates the receiver thread.
     */
    ias_avbvideobridge_result createThread();

    /*!
     * @brief The receiver worker thread.
     */
    void workerThread();

    /*!
     * @brief Cleans up the resources used.
     */
    void cleanup();


    //
    // Member variables
    //

    template <typename  T>
    struct callbackData
    {
      callbackData()
       : callback(NULL)
       , userPtr(NULL)
      {}
      void clear(){ callback = NULL; userPtr = NULL; };
      bool isUnregistered() {return (callback == NULL); };

      T callback;
      void * userPtr;
    };

    // not used at the moment
    static std::atomic_int                 mNumberInstances;   //!< Count the maximum allowed instances of IasAvbBridge
    std::string                            mReceiverRole;    //!< Name of the video stream handled by this receiver
    callbackData<ias_avbvideobridge_receive_H264_cb>    mCallbackH264;    //!< Callback function for H.264 data
    callbackData<ias_avbvideobridge_receive_MpegTS_cb>  mCallbackMpegTS;  //!< Callback function for MPEG-TS data
    bool                                   mIsRunning;       //!< Indicating that the worker thread is running
    std::thread                            *mWorkerThread;   //!< Worker thread to receive data
    IasAvbVideoShmConnection               mShmConnection;   //!< Connection providing the shared memory
    IasAvbVideoRingBuffer                  *mRingBuffer;     //!< Pointer to the ring buffer
    uint32_t                               mRingBufferSize;  //!< Size of the buffer in ???
    uint16_t                               mTimeout;         //!< Timeout value for receiving data
    receivingFormat                        mFormat;          //!< Format the receiver is handling
    std::mutex                             mMutex;           //!< Mutex to prevent race condition on registering callback
};

} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBVIDEOBRIDGE_AVBVIDEORECEIVER_HPP */
