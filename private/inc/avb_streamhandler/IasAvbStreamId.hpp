/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbStreamId.hpp
 * @brief   The definition of the IasAvbStreamId class.
 * @details This class is used to store a stream ID.
 * @date    2013
 */

#ifndef IASAVBSTREAMID_HPP_
#define IASAVBSTREAMID_HPP_

#include "avb_streamhandler/IasAvbTypes.hpp"
#include <cstring>


namespace IasMediaTransportAvb {


class /*IAS_DSO_PUBLIC*/ IasAvbStreamId
{
  public:
    /**
     *  @brief Constructors.
     */
    IasAvbStreamId();
    explicit IasAvbStreamId(uint8_t * streamId);
    explicit IasAvbStreamId(uint64_t streamId);

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbStreamId();

#if 0 // we want to use the compiler-generated versions in this case
    /**
     * @brief Assignment operator.
     */
    IasAvbStreamId& operator=(IasAvbStreamId const &other);

    /**
     * @brief Copy constructor
     */
    IasAvbStreamId(IasAvbStreamId const &other);
#endif

    void copyStreamIdToBuffer(uint8_t * buf, size_t bufSize) const;

    void setStreamId(const uint8_t * streamId);

    void setDynamicStreamId();

    inline operator uint64_t() const;
    inline bool operator<(IasAvbStreamId const & other) const;

  private:

    //
    // Members
    //

    uint64_t mStreamId;
};


inline IasAvbStreamId::operator uint64_t() const
{
  return mStreamId;
}

inline bool IasAvbStreamId::operator<(IasAvbStreamId const & other) const
{
  return mStreamId < other.mStreamId;
}

} // namespace IasMediaTransportAvb

#endif /* IASAVBSTREAMID_HPP_ */
