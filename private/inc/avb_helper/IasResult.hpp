/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#ifndef IAS_MEDIA_TRANSPORT_IAS_RESULT_HPP
#define IAS_MEDIA_TRANSPORT_IAS_RESULT_HPP

#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"
#include "ias_visibility.h"
#include <dlt/dlt_cpp_extension.hpp>
#include <ostream>
#include <string>

/**
 * Macro to provide a convenient way to log IasResult within DLT.
 */
#define IAS_DLT_IASRESULT(VALUE) DLT_STRING(VALUE.toString().c_str())

/**
 * @brief Ias
 */
namespace IasMediaTransportAvb
{

  /**
   * @brief All result groups available for the subsystem Foundation
   */
  enum IasResultFoundationGroups {
      cIasResultGroupBasic//!< cIasResultGroupBasic
    , cIasResultGroupThread//!< cIasResultGroupThread
    , cIasResultGroupSignal//!< cIasResultGroupSignal
    , cIasResultGroupMutex//!< cIasResultGroupMutex
    , cIasResultGroupNetwork//!< cIasResultGroupNetwork
    , cIasResultGroupErrno//!< cIasResultGroupErrno the value of the error is an errno system value/
  };

  /**
   * @brief Constants defining result modules/
   */
  static const uint16_t cIasResultModuleFoundation = 0x0u;
  static const uint16_t cIasResultModuleSystembus = 0x1u;
  static const uint16_t cIasResultModuleMonitoringAndLifeCycle = 0x2u;
  static const uint16_t cIasResultModuleSoftwareUpdateDownload = 0x3u;
  static const uint16_t cIasResultModuleEarlyApplication = 0x4u;
  static const uint16_t cIasResultModuleLogAndTrace = 0x5u;

  /**
   * @brief Base class used to store result values of an operation. The base class provides the most common result values needed.
   *
   * To specify application specific result values inherit from this class. To distinguish result values from each other the
   * result module and resultGroup must specify a unique value pair.
   *
   * Note, if you want to add a new group of results, make sure that you add an entry for the module, e.g.
   *
   * @code
   * static const uint16_t cIasresultModuleMYSUBSYSTEM = 0xVAL;
   * @endcode
   *
   * The result group can be specified within the sub-system.
   */
  class IAS_DSO_PUBLIC IasResult
  {
    public:
      /**
       * IasResult constructor.
       *
       * This constructor sets the group id to cIasResultGroupBasic and the module id to cIasResultModuleFoundation.
       *
       *  @param[in] resultValue the result value of this result.
       */
      explicit IasResult(uint32_t const resultValue)
        : mValue(resultValue)
        , mGroup(cIasResultGroupBasic)
        , mModule(cIasResultModuleFoundation)
      {}

      /**
       * @brief IasResult constructor.
       *
       * This constructor sets all parts of the result.
       *
       *  @param[in] resultValue the result value of this result.
       *  @param[in] resultGroupId  the group id of this result.
       *  @param[in] resultModuleId  the module id of this result.
       */
      IasResult(uint32_t const resultValue, uint16_t const resultGroupId, uint16_t const resultModuleId)
        : mValue(resultValue)
        , mGroup(resultGroupId)
        , mModule(resultModuleId)
      {}

      /*!
       *  @brief Destructor, virtual by default.
       */
      virtual ~IasResult(){};

      static const IasResult cOk;                   //!< all ok
      static const IasResult cFailed;               //!< something failed, not further specified
      static const IasResult cAlreadyInitialized;   //!< the object is already initialized
      static const IasResult cNotInitialized;       //!< the object is not initialized
      static const IasResult cInitFailed;           //!< the initialization of the object failed
      static const IasResult cObjectInvalid;        //!< the object is invalid
      static const IasResult cCleanupFailed;        //!< the cleanup of the object failed
      static const IasResult cParameterInvalid;     //!< one of the function parameters is invalid
      static const IasResult cOutOfMemory;          //!< the application is out of memory
      static const IasResult cObjectNotFound;       //!< the element is has been not found
      static const IasResult cNotSupported;         //!< element is not supported
      static const IasResult cTryAgain;             //!< please try again

      /**
       * Result operator ==
       *
       * The value, group and module of the result are compared.
       *
       * @param[in] left result to compare.
       * @param[in] right result to compare.
       *
       * @result comparison result.
       */
      friend bool operator==(IasResult const & left, IasResult const & right);

      /**
       * Result operator !=
       *
       * The value, group and module of the result are compared.
       *
       * @param[in] left result to compare.
       * @param[in] right result to compare.
       *
       * @result comparison result.
       */
      friend bool operator!=(IasResult const & left, IasResult const & right);

      /**
       * Returns the value of the result
       *
       * @result get the plain value of the result
       */
      inline uint32_t getValue()const
      {
        return mValue;
      }

      /**
       * @brief Set the value of the result to the given errno value.
       *
       * @param[in] errnoValue the error value that is stored as an errno.
       *
       * Sets the value of the error to the given \c errnoValue and the group of the error
       * to cIasResultGroupErrno.
       *
       */
      inline void setErrnoValue(uint32_t const errnoValue)
      {
        mGroup = cIasResultGroupErrno;
        mValue = errnoValue;
      }

      /*!
       * Convert the result value into a string representation.
       */
      virtual std::string toString()const;

    protected:
      uint32_t mValue;
      uint16_t mGroup;
      uint16_t mModule;

    private:
      IasResult() = delete;
  };

  // See documentation in class definition.
  inline bool operator==(IasResult const & left, IasResult const & right)
  {
    return (left.mValue == right.mValue)
        && (left.mGroup == right.mGroup)
        && (left.mModule == right.mModule);
  }

  // See documentation in class definition.
  inline bool operator!=(IasResult const & left, IasResult const & right)
  {
    return (left.mValue != right.mValue)
        || (left.mGroup != right.mGroup)
        || (left.mModule != right.mModule);
  }

  /**
   * @brief Function to check if a result value signals a success.
   * @param result.
   * @return true if result signals a success.
   * @return false if result signals a failure.
   */
  static inline bool IAS_SUCCEEDED(IasResult const &result)
  {
    return (result == IasResult::cOk ? true : false);
  }

  /**
   * @brief Function to check if a result value signals a failure.
   * @param result.
   * @return true if result signals a failure.
   * @return false if result signals a success.
   */
  static inline bool IAS_FAILED(IasResult const &result)
  {
    return (result != IasResult::cOk ? true : false);
  }

}

template<>
IAS_DSO_PUBLIC int32_t logToDlt(DltContextData &log, IasMediaTransportAvb::IasResult const & value);


/**
 * @brief Overloaded ostream operator for IasResult.
 */
inline std::ostream& operator<< (std::ostream& out, IasMediaTransportAvb::IasResult const &result )
{
  return out << result.toString();
}

#endif // IAS_MEDIA_TRANSPORT_IAS_RESULT_HPP
