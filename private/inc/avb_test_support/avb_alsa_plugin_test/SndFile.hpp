/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    SndFile.hpp
 * @brief   This is just a little helper class to store audio data in a WAV file.
 *
 * @date    2013
 */

// sndfile
#include <sndfile.h>
#include <string>
#include <stdint.h>

class SndFile
{
  public:

    enum Result
    {
      eOK = 0,
      eERR
    };

    SndFile(uint32_t numChannels, uint32_t sampleRate, uint32_t format, uint32_t bufferSize);
    ~SndFile();

    Result create(std::string fileName);
    Result write(uint32_t ch, float *buffer, uint32_t numSamples);
    Result close();

  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    SndFile(SndFile const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    SndFile& operator=(SndFile const &other);

    //
    // Members
    //

    SNDFILE     *mFile;               ///< file pointer
    std::string mFileName;            ///< name of WAV file
    SF_INFO     mInfo;                ///< info struct that holds num of channels, samplerate, ...
    uint32_t    mNumChannels;         ///< number of channels
    uint32_t    mBufferSize;          ///< size of the audio buffer in ALSA frames (num of ALSA frames)
    uint32_t    mSamplesWritten;      ///< number of samples that have been written
    float       *mInterleavedAudio;   ///< pointer to the interleaved audio buffer

};


// *** EOF ***
