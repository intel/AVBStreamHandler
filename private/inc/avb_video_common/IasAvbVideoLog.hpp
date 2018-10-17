/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoLog.hpp
 * @brief   Log "container" for AVB-SH Video Bridge.
 * @details As AVB-SH Video Bridge has code that is shared with different
 *          applications, it can not rely on IasAvbStreamHandlerEnvironment.
 *          This class act as a "log multiplexer": if code is linked to
 *          AVB-SH, it uses its log facilities, if not, will use another
 *          that user provides.
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEOLOG_HPP
#define IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEOLOG_HPP

#include <string>

#include <dlt/dlt.h>

namespace IasMediaTransportAvb {

/**
 * @brief Provides log context for DLT log functions.
 *
 */
class IasAvbVideoLog
{
  public:

    /**
     * @brief Get log context
     *
     * If application is linked to AVB-SH, it's a wrapper
     * for IasAvbStreamHandlerEnvironment::getDltContext. If
     * not, will ignore `dltContextName` parameter and return
     * the DLT log context defined by the application. If
     * application did not define a DLT context, will return
     * a dummy one.
     *
     * @param[in] dltContextName Name of dlt log context
     *
     * @returns log context.
     */
    static DltContext &getDltContext(const std::string &dltContextName);

    /**
     * @brief Set log context for AVB-SH Video Bridge
     *
     * If application is not linked to AVB-SH, this method can
     * be used to define a default DLT context for AVB-SH Video Bridge
     * code.
     *
     * @param[in] dltContext DLT context to be used. If nullptr, remove any previously
     * set context
     */
    static void setDltContext(DltContext *dltContext);

  private:

    static DltContext *mDefaultContext;     //!< Default context, set via setDltContext
    static DltContext *mDltCtxDummy;        //!< Dummy context, if mDefaultContext is nullptr
};

}

#endif
