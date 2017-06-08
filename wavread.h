/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

//-----------------------------------------------------------------------------
// File: WavRead.h
//
// Desc: Support for loading and playing Wave files using DirectSound sound
//       buffers.
//
// Copyright (c) 1999 Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

#include <mmsystem.h>
#include <string>

HRESULT WaveOpenFile(std::string const &Filename, HMMIO *phmmioIn, WAVEFORMATEX **ppwfxInfo,
                     MMCKINFO *pckInRIFF);
HRESULT WaveStartDataRead(HMMIO *phmmioIn, MMCKINFO *pckIn, MMCKINFO *pckInRIFF);
HRESULT WaveReadFile(HMMIO hmmioIn, UINT cbRead, BYTE *pbDest, MMCKINFO *pckIn, UINT *cbActualRead);

//-----------------------------------------------------------------------------
// Name: class CWaveSoundRead
// Desc: A class to read in sound data from a Wave file
//-----------------------------------------------------------------------------
class CWaveSoundRead
{
  public:
    WAVEFORMATEX *m_pwfx; // Pointer to WAVEFORMATEX structure
    HMMIO m_hmmioIn{ NULL }; // MM I/O handle for the WAVE
    MMCKINFO m_ckIn; // Multimedia RIFF chunk
    MMCKINFO m_ckInRiff; // Use in opening a WAVE file

  public:
    CWaveSoundRead();
    ~CWaveSoundRead();

    HRESULT Open(std::string const &Filename);
    HRESULT Reset();
    HRESULT Read(UINT nSizeToRead, BYTE *pbData, UINT *pnSizeRead);
    HRESULT Close();
};
