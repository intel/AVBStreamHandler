/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbClockDriverInterface.hpp
 * @brief   Small interface to provide config values to the stream handler.
 * @details This is a pure virtual interface class that has to be
 *          implemented by clock driver modules.
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_IASAVBCLOCKDRIVERINTERFACE_HPP
#define IAS_MEDIATRANSPORT_IASAVBCLOCKDRIVERINTERFACE_HPP

#include "IasAvbStreamHandlerTypes.hpp"
#include "avb_helper/ias_visibility.h"
#include <string>

namespace IasMediaTransportAvb {

/**
 * @brief helper interface class giving the clock driver access to configuration data
 */
class IAS_DSO_PUBLIC IasAvbRegistryQueryInterface
{
  public:
    //{@
    /**
     * @brief query a numerical or textual configuration value
     *
     * Retrieves a numerical or textual value registered under the given key name.
     * If no entry is found under the given key, false is returned and the content
     * of value remains unchanged.
     *
     * Key names can be taken from the API documentation of the Media Transport Subsystem.
     * They also can be defined by a custom-made configuration API, if they start with
     * "clockdriver.config."
     *
     * @param[in] key     name of the configuration value
     * @param[out] value   the value
     * @returns true if a a value could be retrieved
     */
    virtual bool queryConfigValue(const std::string &key, uint64_t &value) const = 0;
    virtual bool queryConfigValue(const std::string &key, std::string &value) const = 0;
    //@}

    /**
     * @brief convenience wrapper to handle integer types other than uint64_t
     *
     * All numerical values are stored in the registry in uint64_t encoding.
     * If another integer type is to be used, this wrapper template is
     * used to convert between the types, as long as an automatic
     * conversion is possible, depending on the strictness configuration
     * of the compiler. For more details, see above.
     *
     * @param[in] key     name of the configuration value
     * @param[out] value   the value
     * @returns true if a a value could be retrieved
     */
    template <class T>
    inline bool queryConfigValue(const std::string &key, T &value) const;


  protected:
    //@{
    /// can only be created and destroyed through implementation class
    IasAvbRegistryQueryInterface() {}
    ~IasAvbRegistryQueryInterface() {}
    //@}
};


/**
 * @brief interface class for interaction with PLL driver modules
 */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif //#ifdef __clang__
class __attribute__ ((visibility ("default"))) IasAvbClockDriverInterface
{
  public:
    /**
     * @brief initializes the clock driver
     *
     * This method is called during stream handler initialization,
     * after passArguments() of the config library has been called.
     * So the config library has the option of passing confguration
     * values to the clock driver by putting them into the registry.
     * The key names of all user-specific registry entries controlling
     * the clock driver should start with "clockdriver.config.".
     *
     * If anything other than eIasAvbResultOk is returned by the method,
     * stream handler initialization is aborted.
     *
     * @param[in] registry interface to query registry values
     * @return eIasAvbResultOk upon success
     */
    virtual IasAvbResult init(const IasAvbRegistryQueryInterface &registry) = 0;

    /**
     * @brief clean up all resources used by the clock driver
     *
     * This method is called upon shutdown of the stream handler. The
     * clock driver should release all resources (e.g. closing device files)
     * in the implementation of the method.
     */
    virtual void cleanup() = 0;

    /**
     * @brief update oscillator frequency
     *
     * By calling this method, the frequency control loop, of the stream
     * handler's clock recovery module, indicates that the frequency of the
     * oscillator should either be increased (value > 1.0) or decreased (value < 1.0)
     * by the given factor. If the clock driver has already reached its upper or
     * lower frequency limit, the request should be ignored.
     * Note that the change factors will typically be very close to 1.0!
     *
     * The driverId value is passed to the clock driver by the stream handler unchanged
     * from the value specified in the @ref IasAvbStreamHandlerInterface::setClockRecoveryParams
     * call.
     *
     * @param[in] driverId value passed to the driver to identify different clocks
     * @param[in] relVal frequency change factor as described above
     */
    virtual void updateRelative(uint32_t driverId, double relVal) = 0;

  protected:
    //@{
    /// can only be created and destroyed through implementation class
    IasAvbClockDriverInterface() {}
    ~IasAvbClockDriverInterface() {}
    //@}
};


/**
 * @brief The one and only function that needs to be exported by the shared library object
 *
 * @return reference to the clock driver object
 */
extern "C" IasAvbClockDriverInterface & getIasAvbClockDriverInterfaceInstance();




template <class T>
inline bool IasAvbRegistryQueryInterface::queryConfigValue(const std::string &key, T &value) const
{
  bool ret = false;

  uint64_t val = value; // this assignment will provoke compiler warnings or errors when T is incompatible

  if (queryConfigValue(key, val))
  {
    value = T(val);
    ret = true;
  }

  return ret;
}


} // namespace


#endif // IAS_MEDIATRANSPORT_IASAVBCLOCKDRIVERINTERFACE_HPP
