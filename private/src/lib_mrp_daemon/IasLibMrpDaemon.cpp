/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file    IasLibMrpDaemon.cpp
 *  @brief   Library for the PTP daemon.
 *  @date    2013
 */

#include "IasAvbTypes.hpp"
#include "lib_mrp_daemon/IasLibMrpDaemon.hpp"


namespace IasMediaTransportAvb {

/**
 *  Constructor.
 */
IasLibMrpDaemon::IasLibMrpDaemon()
//  ,mConfig(config)
{
}

/**
 *  Destructor.
 */
IasLibMrpDaemon::~IasLibMrpDaemon()
{
  cleanUp();
}

/**
 *  Initialization method.
 */
IasAvbProcessingResult IasLibMrpDaemon::init()
{
  IasAvbProcessingResult error = eIasAvbProcOK;

  return error;
}

/**
 *  @brief Cleanup allocated resources.
 */
void IasLibMrpDaemon::cleanUp()
{

}

} // namespace IasMediaTransportAvb

