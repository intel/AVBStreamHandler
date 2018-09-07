/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbStreamHandlerEventInterface.hpp
 * @brief   Small interface to provide link and stream status information
 *          to the stream handler.
 * @details This is a pure virtual interface class that has to be
 *          implemented by the stream handler.
 * @date    2013
 */

#ifndef IASVBSTREAMHANDLEREVENTINTERFACE_HPP_
#define IASVBSTREAMHANDLEREVENTINTERFACE_HPP_

#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"

namespace IasMediaTransportAvb {

/**
 * @brief helper interface class giving the stream handler information about link
 *        and stream status
 */
class IasAvbStreamHandlerEventInterface
{
  public:
    //{@
    /**
     * @brief Reports the current link status
     *
     * It is called by the transmit engine on changes of the link state.
     *
     * @param[in] linkIsUp     'true' if the link is up, otherwise 'false'
     */
    virtual void updateLinkStatus(const bool linkIsUp) = 0;

    /**
     * @brief Reports the state of a certain stream.
     *
     * This method is called by the receive engine on any changes of the
     * state of a certain stream.
     *
     * @param[in] streamIsUp     'true' if the stream is up, otherwise 'false'
     */
    virtual void updateStreamStatus(uint64_t streamId, IasAvbStreamState status) = 0;
    //@}


  protected:
    //@{
    /// can only be created and destroyed through implementation class
    IasAvbStreamHandlerEventInterface() {}
    ~IasAvbStreamHandlerEventInterface() {}
    //@}
};

} // namespace


#endif // IASVBSTREAMHANDLEREVENTINTERFACE_HPP_
