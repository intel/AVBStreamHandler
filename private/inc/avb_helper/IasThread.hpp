/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#ifndef IAS_MEDIA_TRANSPORT_IAS_THREAD_HPP
#define IAS_MEDIA_TRANSPORT_IAS_THREAD_HPP


#include "IasResult.hpp"
#include "IasIRunnable.hpp"
#include <pthread.h>
#include <string>


/**
 * @brief IasMediaTransportAvb
 */
namespace IasMediaTransportAvb
{

/**
 * ThreadId.
 */
typedef pthread_t IasThreadId;

/**
 * Invalid ThreadId.
 */
extern IAS_DSO_PUBLIC IasThreadId const cIasThreadIdInvalid;

/**
 * @brief Result values for the IasThread.
 *
 * Extension to IasResult.
 */
class IAS_DSO_PUBLIC IasThreadResult: public IasResult
{
  public:
  /**
   * IasThreadResult Constructor.
   *
   * The thread result is in the group cIasResultGroupThread and in the module cIasResultModuleFoundation.
   *
   * @param[in] value the value of the thread result.
   */
  explicit IasThreadResult(uint32_t const value)
    : IasResult(value, cIasResultGroupThread, cIasResultModuleFoundation)
  {

  }

  /**
   * IasThreadResult convert constructor from base results.
   *
   * @param[in] result other result
   *
   */
  IasThreadResult(IasResult const & result)
    : IasResult(result)
  {

  }

  /*!
   *  @brief Destructor, virtual by default.
   */
  virtual ~IasThreadResult(){};

  /*!
    * Convert the result value into a string representation.
    */
   virtual std::string toString()const;

  static const IasThreadResult cThreadAlreadyStarted;               /**< Result value cThreadAlreadyStarted */
  static const IasThreadResult cThreadNotRunning;                   /**< Result value cThreadNotRunning */
  static const IasThreadResult cCreateBarrierFailed;                /**< Result value cCreateBarrierFailed */
  static const IasThreadResult cInitAttributeFailed;                /**< Result value cInitAttributeFailed */
  static const IasThreadResult cCreateThreadFailed;                 /**< Result value cCreateThreadFailed */
  static const IasThreadResult cDestroyAttributeFailed;             /**< Result value cDestroyAttributeFailed */
  static const IasThreadResult cDestroyBarrierFailed;               /**< Result value cDestroyBarrierFailed */
  static const IasThreadResult cWaitBarrierFailed;                  /**< Result value cWaitBarrierFailed */
  static const IasThreadResult cJoinThreadFailed;                   /**< Result value cJoinThreadFailed */
  static const IasThreadResult cThreadSetNameFailed;                /**< Result value cThreadSetNameFailed */
  static const IasThreadResult cThreadGetNameFailed;                /**< Result value cThreadGetNameFailed */
  static const IasThreadResult cThreadSchedulePriorityFailed;       /**< Result value cThreadSchedulePriorityFailed */
  static const IasThreadResult cThreadSchedulePriorityNotPermitted; /**< Result value cThreadSchedulePriorityNotPermitted */
  static const IasThreadResult cThreadSchedulingParameterInvalid;   /**< Result value cThreadSchedulingParameterInvalid */
};


/**
 * @brief Thread class.
 *
 * The IasThread is used to execute an IRunnable inside a separate thread.
 * On creation of the thread the pointer to the IRunnable must be passed to the thread.
 * Then a call to start will start the thread. The function will return and the IRunnable is executed inside the newly created thread.
 * A call to stop will terminate the thread and stop the execution of the IRunnable.
 *
 */
class IAS_DSO_PUBLIC IasThread
{
  public:

    /**
     * @brief Enum to specify thread scheduling policy.
     */
    enum IasThreadSchedulingPolicy
    {
      eIasSchedulingPolicyOther,    /**< the standard round-robin time-sharing policy */
      eIasSchedulingPolicyFifo,     /**< a first-in, first-out policy */
      eIasSchedulingPolicyRR,       /**< a round-robin policy */
      eIasSchedulingPolicyBatch,    /**< "batch" style execution of processes */
      eIasSchedulingPolicyIdle      /**< for running very low priority background jobs */
    };

    /**
     * Constructor.
     * @param runnableObject the runnable object that does the work
     * @param[in] threadName the name of the thread (optional, default="").
     * @param[in] stackSize the size of new thread's (optional, default=system default).
     *
     * @note you can see the name of the thread either in /proc/<PID>/tasks/<TID>/stat
     *       or with ps -Leo
     *       or (nicer, sorted output) ps -Leo pid,tid,class,rtprio,ni,pri,pcpu,stat,wchan:14,comm
     */
    IasThread(IasIRunnable *runnableObject, std::string const &threadName="", size_t stackSize = 0);

    /**
     * Destructor.
     */
    virtual ~IasThread();

    /**
     * Start the thread.
     *
     * @param assureRunning if assureRunning is true the start function will wait until the thread is actually running.
     *                      If not the function will return immediately.
     *                      If assureRunning=false it will not return any errors about barriers (e.g. cCreateBarrierFailed)
     *
     * @param runnableObject the runnable object that does the work or NULL if using the runnable object passed on
     *                      construction or the previous start thread call
     *
     * @returns IasThreadResult indicating success or failure
     *
     * If the method detects any errors when processing, \ref stop() is called implicitly.
     * If the method succeeds the user has to call \ref stop() to free all thread resources again.
     * To restart a thread therefore something like this should be checked before starting:
     *
     * @code
     * if ( thread.wasStarted() )
     * {
     *   IasThreadResult const stopResult = thread.stop();
     *   ...
     * }
     * IasThreadResult const startResult = thread.start();
     * ...
     * @endcode
     *
     * To relax this requirement the thread automatically calls stop() inside this call if the thread is not running any more.
     * So it is also a valid use case to call:
     *
     * @code
     * if ( thread.isRunning() )
     * {
     *   IasThreadResult const stopResult = thread.stop();
     *   ...
     * }
     * IasThreadResult const startResult = thread.start();
     * ...
     * @endcode
     */
    IasThreadResult start(bool assureRunning, IasIRunnable *runnableObject = NULL);

    /**
     * Stop the thread
     * This function will call the shutDown function of the IRunnable and then end the thread.
     *
     * This function has to be called for each successful start() call.
     * Otherwise it will be not possible to start the thread again.
     *
     * The result of the run can be access through \ref getRunThreadResult().
     *
     * @returns IasResult indicating success or failure.
     */
    IasResult stop();

    /**
     * Sets the priority of this thread.
     * @param [in] policy the scheduling policy to use. Certain policies may not be available depending on the platform.
     * @param [in] priority the scheduling.
     *
     * @returns IasThreadResult indicating success or nature of failure.
     */
    IasThreadResult setSchedulingParameters(IasThreadSchedulingPolicy policy, int32_t priority);

    /**
     * Gets the priority of this thread.
     * @param [out] policy the scheduling policy to use.
     * @param [out] priority the scheduling.
     *
     * @returns IasThreadResult indicating success or nature of failure.
     */
    IasThreadResult getSchedulingParameters(IasThreadSchedulingPolicy & policy, int32_t & priority);

    /**
     * Signals the thread with the specified signal.
     * @param [in] signum the signal to send to the thread.
     *
     * @returns IasThreadResult indicating success or nature of failure.
     */
    IasThreadResult signal(const int32_t signum);

    /**
     * Get result from thread run.
     */
    IasResult getRunThreadResult() const { return mRunThreadResult; }

    /*!
     * Query if the thread is currently running.
     *
     * @returns \c true if the thread is running, otherwise \c false.
     */
    bool isRunning() { return getCurrentThreadState()&cTHREAD_STATE_RUNNING_FLAG; }

    /*!
     * Query if the thread was previously started.
     *
     * @returns \c true if the thread was started, otherwise \c false.
     */
    bool wasStarted() { return getCurrentThreadState()&cTHREAD_STATE_STARTED_FLAG; }

    /*!
     * Get the name of the thread given at construction time.
     *
     * @returns the name of the thread.
     */
    std::string getName() const;

    /*!
     * Get the id of the thread that was assigned to the thread on thread creation time by the OS.
     *
     * @returns the id of the thread.
     */
    IasThreadId getThreadId() const { return mThreadId; }

    /**
     * Sets the name of the thread with the specified ID.
     * @param [in] name The name to give to the thread
     *
     * @returns IasThreadResult indicating success or nature of failure
     */
    static IasThreadResult setThreadName(const IasThreadId threadId, const std::string & name);

    /**
     * Gets the name of the thread with the specified ID.
     * @param [out] name the thread's name.
     *
     * @returns IasThreadResult indicating success or nature of failure.
     */
    static IasThreadResult getThreadName(const IasThreadId threadId, std::string & name);

    /**
     * Sets the scheduling parameters of the thread with the specified ID.
     * @param [in] threadId the ID of the thread.
     * @param [in] policy the scheduling policy.
     * @param [in] priority the scheduling priority.
     *
     * @returns IasThreadResult indicating success or nature of failure.
     */
    static IasThreadResult setSchedulingParameters(const IasThreadId threadId, IasThreadSchedulingPolicy policy, int32_t priority);

    /**
     * Sets the scheduling parameters of the thread with the specified ID.
     * @param [in] threadId the ID of the thread.
     * @param [out] policy the scheduling policy.
     * @param [out] priority the scheduling priority.
     *
     * @returns IasThreadResult indicating success or nature of failure.
     */
    static IasThreadResult getSchedulingParameters(const IasThreadId threadId, IasThreadSchedulingPolicy & policy, int32_t & priority);

    /**
     * Signals the thread with the specified ID with the specified signal.
     * @param [in] threadId the ID of the thread.
     * @param [in] signum the signal to send to the thread.
     *
     * @returns IasThreadResult indicating success or nature of failure.
     */
    static IasThreadResult signal(const IasThreadId threadId, const int32_t signum);

  private:

    /**
     * Processes / validates the specified scheduling parameters.
     * @param [in] policy the scheduling policy as represented by the IAS type IasThreadSchedulingPolicy.
     * @param [in|out] priority the scheduling priority, corrected if out-of-range.
     * @param [out] priority the scheduling policy as represented in integer form (for passing to pthread functions).
     *
     * @returns IasThreadResult indicating success or nature of failure.
     */
    static IasThreadResult processSchedulingParameters(IasThreadSchedulingPolicy policy, int32_t & priority, int32_t & schedulingPolicy);

    /**
     * Commits the specified scheduling parameters to the thread with the specified ID.
     * @param [in] threadId the ID of the thread.
     * @param [in] policy the scheduling policy.
     * @param [in] priority the scheduling policy.
     *
     * @returns IasThreadResult indicating success or nature of failure.
     */
    static IasThreadResult commitSchedulingParameters(const IasThreadId threadId, const int32_t policy, const int32_t priority);

  protected:
    /**
     * Static run just calls the run member function.
     *
     * @param[in] arg void pointer to the thread object that should be run.
     */
    static void * run(void * arg);

    /**
     * Run function of the thread.
     *
     * This function will call the following functions of the IRunnable:
     * 1. beforeRun
     * 2. run
     * 3. afterRun
     */
    void run();

  private:
    /**
     * @brief Set thread's name in kernel, it will show up in ps/top.
     *
     * It will also request the thread's name from the kernel to make sure that getName() will return the same value.
     *
     * @Note threadName must not be longer than 16 characters, will be truncated.
     *
     * @result failure or success.
     */
    IasThreadResult setThreadName();

    std::string mThreadName;
    size_t mStackSize;

    bool mAssureRunning;

    static const uint8_t cTHREAD_STATE_INVALID = 0x0u;
    static const uint8_t cTHREAD_STATE_STARTED_FLAG = 0x1u;
    static const uint8_t cTHREAD_STATE_RUNNING_FLAG  = 0x2u;
    static const uint8_t cTHREAD_STATE_IS_STARTING_FLAG = 0x4u;
    static const uint8_t cTHREAD_STATE_IS_STOPPING_FLAG = 0x8u;
    uint8_t mThreadState;
    uint8_t inline getCurrentThreadState() { return __sync_fetch_and_or(&mThreadState, 0u); }


    IasThreadId mThreadId;
    IasIRunnable * mRunnableObject;

    pthread_barrier_t mThreadStartedBarrier;
    bool mThreadStartedBarrierInitialized;
    IasThreadResult mStartThreadResult;
    IasThreadResult mRunThreadResult;
    int32_t mSchedulingPolicy;
    int32_t mSchedulingPriority;

    DltContext *mLog;

};

} //namespace IAS



template<>
IAS_DSO_PUBLIC int32_t logToDlt(DltContextData &log, IasMediaTransportAvb::IasThreadResult const & value);

template<>
int32_t logToDlt(DltContextData & log, IasMediaTransportAvb::IasThread::IasThreadSchedulingPolicy const & value);

#endif /* IAS_MEDIA_TRANSPORT_IAS_THREAD_HPP */
