/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbConfigRegistryInterface.hpp
 * @brief   Small interface to provide config values to the stream handler.
 * @details This is a pure virtual interface class that will be
 *          implemented by the AVB stream handler. It is separated
 *          from the main API since it is used at another point in time.
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_IASAVBCONFIGREGISTRYINTERFACE_HPP
#define IAS_MEDIATRANSPORT_IASAVBCONFIGREGISTRYINTERFACE_HPP

#include "IasAvbStreamHandlerTypes.hpp"
#include "avb_helper/ias_visibility.h"
#include "media_transport/avb_streamhandler_api/IasAvbRegistryKeys.hpp"
#include <string>

namespace IasMediaTransportAvb {


/**
 * @brief The API of the AVB stream handler
 *
 * This is a pure virtual interface class that will be
 * implemented by the AVB stream handler. It is separated
 * from the main API since it is used at another point in time.
 */
class IAS_DSO_PUBLIC IasAvbConfigRegistryInterface
{
  protected:
    //@{
    /// Implementation object cannot be created or destroyed through this interface
    IasAvbConfigRegistryInterface() {}
    /* non-virtual */ ~IasAvbConfigRegistryInterface() {}
    //@}

  public:
    //{@
    /**
     * @brief sets a numerical or textual configuration value
     *
     * Registers a numerical or textual value under the given key name.
     *
     * @param[in] key     name of the configuration value
     * @param[in] value   the value
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult setConfigValue(const std::string &key, uint64_t value) = 0;
    virtual IasAvbResult setConfigValue(const std::string &key, const std::string &value) = 0;
    //@}

};


} // namespace


#endif // IAS_MEDIATRANSPORT_IASAVBCONFIGREGISTRYINTERFACE_HPP
