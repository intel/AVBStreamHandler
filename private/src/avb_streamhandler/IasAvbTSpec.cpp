/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbTSpec.cpp
 * @brief   This is the implementation of the IasAvbTSpec class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbTSpec.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"


namespace IasMediaTransportAvb
{

uint8_t IasAvbTSpec::mPrioTable[cIasAvbNumSupportedClasses] =
{
    3, // eIasAvbClassA
    2  // eIasAvbClassB
};

uint8_t IasAvbTSpec::mIdTable[cIasAvbNumSupportedClasses] =
{
    2, // eIasAvbClassA
    3  // eIasAvbClassB
};


// class measurement time (aka observation interval in ns
uint32_t IasAvbTSpec::mClassMeasurementTimeTable[cIasAvbNumSupportedClasses] =
{
    125000,  // eIasAvbClassA
    250000   // eIasAvbClassB
};

// presentation time offset: default = max transit time - observation interval
uint32_t IasAvbTSpec::mPresentationTimeOffsetTable[cIasAvbNumSupportedClasses] =
{
     2000000 - 125000, // eIasAvbClassA
    10000000 - 250000  // eIasAvbClassB (10ms instead of 50ms - prefer AVnu Automotive over IEEE default)
};

void IasAvbTSpec::initTables()
{
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(std::string(IasRegKeys::cTSpecVlanId)      + "high", mIdTable[IasAvbSrClass::eIasAvbSrClassHigh]);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(std::string(IasRegKeys::cTSpecVlanPrio)    + "high", mPrioTable[IasAvbSrClass::eIasAvbSrClassHigh]);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(std::string(IasRegKeys::cTSpecPresTimeOff) + "high", mPresentationTimeOffsetTable[IasAvbSrClass::eIasAvbSrClassHigh]);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(std::string(IasRegKeys::cTSpecInterval)    + "high", mClassMeasurementTimeTable[IasAvbSrClass::eIasAvbSrClassHigh]);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(std::string(IasRegKeys::cTSpecVlanId)      + "low", mIdTable[IasAvbSrClass::eIasAvbSrClassLow]);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(std::string(IasRegKeys::cTSpecVlanPrio)    + "low", mPrioTable[IasAvbSrClass::eIasAvbSrClassLow]);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(std::string(IasRegKeys::cTSpecPresTimeOff) + "low", mPresentationTimeOffsetTable[IasAvbSrClass::eIasAvbSrClassLow]);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(std::string(IasRegKeys::cTSpecInterval)    + "low", mClassMeasurementTimeTable[IasAvbSrClass::eIasAvbSrClassLow]);
}

} // namespace IasMediaTransportAvb
