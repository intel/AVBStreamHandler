/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLibMrpDaemon.hpp
 * @brief   Library class for the PTP daemon.
 * @date    2013
 */


#ifndef IASLIBMRPDAEMON_HPP_
#define IASLIBMRPDAEMON_HPP_

#include "avb/ipcdef.hpp"

namespace IasMediaTransportAvb {

class /* IAS_DSO_PUBLIC */ IasLibMrpDaemon
{
  public:
    /**
     *  @brief Constructor.
     */
    IasLibMrpDaemon();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasLibMrpDaemon();

    /**
     *  @brief Allocate resources.
     */
    IasAvbProcessingResult init();

    /**
     *  @brief Cleanup allocated resources.
     */
    void cleanUp();

    /**
     * @brief  Get the maximum number of filter stages that can be used for
     *         each audio channel.
     *
     * @return The maximum number of filter stages.
     */
    IasAvbProcessingResult getCurrentNetworkScalingFactors(gPtpTimeData* td);


  private:

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasLibMrpDaemon(IasLibMrpDaemon const &other);

    /**
     *  @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasLibMrpDaemon& operator=(IasLibMrpDaemon const &other); //lint !e1704

}; // class IasLibMrpDaemon

}  // namespace IasMediaTransport

#endif /* IASLIBMRPDAEMON_HPP_ */
