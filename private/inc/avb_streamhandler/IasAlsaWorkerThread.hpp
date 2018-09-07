/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAlsaEngine.hpp
 * @brief   This class implements a worker thread that maintains a list of Alsa streams in order to transfer their
 *          audio data.
 *
 * @details All Alsa streams handled by this worker must have the same parameter (period size and sample frequency).
 *          In its process method the audio data between local buffer and IPC buffer is transfered. The thread
 *          runs at a speed that is required to drive the Alsa devices (playback and capture) on the plugin side.
 * @date    2015
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_ALSAWORKERTHREAD_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_ALSAWORKERTHREAD_HPP

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_helper/IasIRunnable.hpp"
#include "avb_helper/IasThread.hpp"
#include "avb_streamhandler/IasAvbClockDomain.hpp"
#include <mutex>

namespace IasMediaTransportAvb {

class IasAlsaStreamInterface;


class /*IAS_DSO_PUBLIC*/ IasAlsaWorkerThread: public IasMediaTransportAvb::IasIRunnable
{
  public:
    /**
     *  @brief Constructor.
     */
    IasAlsaWorkerThread(DltContext& dltContext);

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAlsaWorkerThread();

    /**
     *  @brief Initialize method.
     *
     */
    virtual IasAvbProcessingResult init(IasAlsaStreamInterface *alsaStream, uint32_t alsaPeriodSize, uint32_t sampleFrequency, IasAvbClockDomain * const clockDomain);

    /**
     * @brief Starts operation.
     */
    IasAvbProcessingResult start();

    /**
     *  @brief Clean up all allocated resources.
     */
    void cleanup();

    /**
     * @brief Stops operation.
     */
    IasAvbProcessingResult stop();


    //
    // Inherited from IasIRunnable
    //

    /**
     * Function called before the run function
     *
     * @returns Value indicating success or failure
     */
    virtual IasResult beforeRun();

    /**
     * The actual run function &rarr; does the processing
     *
     *
     * Stay inside the function until all processing is finished or shutDown is called
     * If this call returns an error value, this error value is reported via the return value of \ref IasThread::start()
     * In case of an error, the thread still needs to be shutdown explicitly through calling \ref IasThread::stop()
     *
     * @returns Value indicating success or failure
     *
     * @note this value can be access through \ref IasThread::getRunThreadResult
     */
    virtual IasResult run();

    /**
     * ShutDown code
     * called when thread is going to be terminated
     *
     * Exit the \ref run function when this function is called
     *
     * @returns Value indicating success or failure
     *
     */
    virtual IasResult shutDown();

    /**
     * Function called after the run function
     *
     * @returns Value indicating success or failure
     *
     * If this call returns an error value and run() was successful, it is reported via the return value of IasThread::stop()
     */
    virtual IasResult afterRun();

    //
    // End of inherited form IasRunnable
    //


    /**
     * @brief Returns the clock domain type that is used by this worker thread
     *
     * @returns clock domain type that is used by this worker thread
     */
    inline IasAvbClockDomain * getClockDomain() const;

    /**
     * @brief Checks the parameter passed if they match to the one this worker thread is maintaining
     *
     * @returns 'true' if the parameters are matching, 'false' otherwise
     */
    bool checkParameter(IasAvbClockDomain * const clockDomain, uint32_t alsaPeriodSize, uint32_t sampleFrequency) const;

    /**
     * @brief Checks the parameter passed if they match to the specified base parameters
     *
     * @returns 'true' if the parameters are matching, 'false' otherwise
     */
    static bool checkParameter(uint32_t alsaPeriodSize, uint32_t sampleFrequency, uint32_t periodBase, uint32_t frequencyBase);

    /**
     * Adds an Alsa stream to the worker thread.
     *
     * @returns 'eIasAvbProcOK' on success or an appropriate error value otherwise
     */
    IasAvbProcessingResult addAlsaStream(IasAlsaStreamInterface *alsaStream);

    /**
     * @brief Removes an Alsa stream to the worker thread.
     *
     * @returns 'eIasAvbProcOK' on success or an appropriate error value otherwise
     */
    IasAvbProcessingResult removeAlsaStream(const IasAlsaStreamInterface * const alsaStream, bool &lastStream);

    /**
     * @brief Checks whether the stream specified by its ID is handled by this worker thread instance.
     *
     * @returns 'true' if the stream is handled by this instance, otherwise 'false'.
     */
    bool streamIsHandled(uint16_t streamId);

  private:

     /**
      *  @brief Copy constructor, private unimplemented to prevent misuse.
      */
    IasAlsaWorkerThread(IasAlsaWorkerThread const &other);

     /**
      *  @brief Assignment operator, private unimplemented to prevent misuse.
      */
    IasAlsaWorkerThread& operator=(IasAlsaWorkerThread const &other);


    /**
     *  @brief The process method. It is called from the worker thread's run method
     */
    void process(uint64_t timestamp = 0u);

    inline bool isInitialized() const;


    //
    // Member variables
    //
    static uint32_t     sInstanceCounter;
    uint32_t            mThisInstance;
    typedef std::vector<IasAlsaStreamInterface*> AlsaStreamList;
    DltContext         *mLog;
    AlsaStreamList      mAlsaStreams;     // list of Alsa streams maintained by this worker thread
    bool                mKeepRunning;     // if set to false the thread stops
    IasThread          *mThread;          // the thread object
    IasAvbClockDomain  *mClockDomain;     // type of clock domain that is used by this worker thread
    uint32_t            mAlsaPeriodSize;  // period size
    uint32_t            mSampleFrequency; // sample frequency
    uint32_t            mServiceCycle;    // counter for current service cycle
    std::mutex          mLock;
    uint32_t            mDbgCount;
};


inline IasAvbClockDomain * IasAlsaWorkerThread::getClockDomain() const
{
  return mClockDomain;
}

inline bool IasAlsaWorkerThread::isInitialized() const
{
  return (NULL != mClockDomain);
}


} // namespace IasMediaTransportAvb




#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_ALSAWORKERTHREAD_HPP */




