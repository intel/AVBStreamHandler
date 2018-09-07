/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbConfiguratorInterface.hpp
 * @brief   Interface declaration file for IasAvbConfiguratorInterface
 * @details The user is expected to provide an implementation of the interface below that
 *          passes all necessary configuration parameters and the creation sequence of
 *          predefined objects such as local streams, etc.
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBCONFIGURATORINTERFACE_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBCONFIGURATORINTERFACE_HPP

#include "IasAvbStreamHandlerTypes.hpp"

namespace IasMediaTransportAvb {

class IasAvbStreamHandlerInterface;
class IasAvbConfigRegistryInterface;

/*
 * @brief   Callback interface to configure StreamHandler from a user-provided object.
 */
class IasAvbConfiguratorInterface
{
  public:
    /**
     * @brief passes command line arguments to the configuration object, if present
     *
     * The implementation of the configuration object might use standards mechanisms
     * like get_opt or getopt_long to parse the arguments. The verbosity level is passed
     * so the config lib can adjust its own messages accordingly.
     * The registry interface is used to specify numeric and textual configuration values
     * to the stream handler. A list of the configuration values along with its names
     * can be found in the documentation.
     *
     * @param[in] argc number of arguments, including the "setup" statement
     * @param[in] argv array of arguments, including the "setup" statement as element 0
     * @param[in] verbosity the verbosity level as determined by the main app
     * @param[in] registry reference to the stream handler's registry object
     */
    virtual bool passArguments( int32_t argc, char** argv, int32_t verbosity, IasAvbConfigRegistryInterface & registry ) = 0;

    /**
     * @brief set up all preconfigured objects
     *
     * The stream handler will call this function from within its init() method as soon as the
     * internal infrastructure has been set up. The configurator now has the option of using
     * all API calls of the IasAvbStreamHandlerInterface. If the setup was successful, the
     * method should return true. On error, return false and the stream handler will abort
     * initialization and free all resources already allocated
     *
     * @returns true upon success, false upon failure
     */
    virtual bool setup(IasAvbStreamHandlerInterface* api) = 0;

  protected:
    //@{
    /// can only be created and destroyed through implementation class
    IasAvbConfiguratorInterface() {}
    ~IasAvbConfiguratorInterface() {}
    //@}
};

} // namespace IasMediaTransportAvb

/**
 * @brief The one and only function that needs to be exported by the configuration plugin
 *
 * @return reference to the instance implementing the configurator interface
 */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif //#ifdef __clang__
extern "C" IasMediaTransportAvb::IasAvbConfiguratorInterface & getIasAvbConfiguratorInterfaceInstance();

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBCONFIGURATORINTERFACE_HPP */
