/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#ifndef SoundH
#define SoundH

#include <stack>
#include <dsound.h>

typedef LPDIRECTSOUNDBUFFER PSound;

// inline Playing(PSound Sound)
//{

//}

class TSoundContainer
{
  public:
    int Concurrent;
    int Oldest;
    char Name[80];
    LPDIRECTSOUNDBUFFER DSBuffer;
    float fSamplingRate; // częstotliwość odczytana z pliku
    int iBitsPerSample; // ile bitów na próbkę
    TSoundContainer *Next;
    std::stack<LPDIRECTSOUNDBUFFER> DSBuffers;
    LPDIRECTSOUNDBUFFER GetUnique(LPDIRECTSOUND pDS);
    TSoundContainer(LPDIRECTSOUND pDS, const char *Directory, const char *strFileName, int NConcurrent);
    ~TSoundContainer();
};

class TSoundsManager
{
  private:
    static LPDIRECTSOUND pDS;
    static LPDIRECTSOUNDNOTIFY pDSNotify;
    // static char Directory[80];
    static int Count;
    static TSoundContainer *First;
    static TSoundContainer * LoadFromFile(const char *Dir, const char *Name, int Concurrent);

  public:
    // TSoundsManager(HWND hWnd);
    // static void Init(HWND hWnd, char *NDirectory);
    static void Init(HWND hWnd);
    ~TSoundsManager();
    static void Free();
    static void Init(char *Name, int Concurrent);
    static void LoadSounds(char *Directory);
    static LPDIRECTSOUNDBUFFER GetFromName(const char *Name, bool Dynamic, float *fSamplingRate = NULL);
    static void RestoreAll();
};

//---------------------------------------------------------------------------
#endif
