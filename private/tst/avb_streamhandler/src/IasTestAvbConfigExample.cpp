/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
* @file IasTestAvbConfigurationBase.cpp
* @brief Test file for IasAvbConfigurationBase
* @date 2018
*/

#include "gtest/gtest.h"

#define private public
#define protected public
//#include "media_transport/avb_configuration_reference/IasAvbConfigExample.cpp"
#include "media_transport/avb_configuration/IasAvbConfigurationBase.hpp"
#undef protected
#undef private

#include "avb_streamhandler/IasAvbStreamHandler.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbConfigRegistryInterface.hpp"
#include "test_common/IasAvbConfigurationInfo.hpp"
#include "test_common/IasSpringVilleInfo.hpp"

#include <string>
#include <iostream>

extern std::string cIasChannelNamePolicyTest;
extern int  verbosity;

namespace IasMediaTransportAvb
{


} /* IasMediaTransportAvb */
