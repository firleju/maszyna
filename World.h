/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include "Camera.h"
#include "Ground.h"
#include "sky.h"
#include "mczapkie/mover.h"

class TWorld
{
    void InOutKey();
    void FollowView(bool wycisz = true);
    void DistantView();

  public:
    bool Init(HWND NhWnd, HDC hDC);
    HWND hWnd;
    GLvoid glPrint(const char *fmt);
    void OnKeyDown(int cKey);
    void OnKeyUp(int cKey);
    // void UpdateWindow();
    void OnMouseMove(double x, double y);
    void OnCommandGet(DaneRozkaz *pRozkaz);
    bool Update();
    void TrainDelete(TDynamicObject *d = NULL);
    TWorld();
    ~TWorld();
    // double Aspect;
  private:
    std::string OutText1; // teksty na ekranie
    std::string OutText2;
    std::string OutText3;
    std::string OutText4;
    void ShowHints();
    bool Render();
    TCamera Camera;
    TGround Ground;
    TTrain *Train;
    TDynamicObject *pDynamicNearest;
    bool Paused;
    GLuint base; // numer DL dla znaków w napisach
    GLuint light; // numer tekstury dla smugi
    TSky Clouds;
    TEvent *KeyEvents[10]; // eventy wyzwalane z klawiaury
    TMoverParameters *mvControlled; // wskaźnik na człon silnikowy, do wyświetlania jego parametrów
    int iCheckFPS; // kiedy znów sprawdzić FPS, żeby wyłączać optymalizacji od razu do zera
    double fTime50Hz; // bufor czasu dla komunikacji z PoKeys
    double fTimeBuffer; // bufor czasu aktualizacji dla stałego kroku fizyki
    double fMaxDt; //[s] krok czasowy fizyki (0.01 dla normalnych warunków)
    int iPause; // wykrywanie zmian w zapauzowaniu
    double VelPrev; // poprzednia prędkość
    int tprev; // poprzedni czas
    double Acc; // przyspieszenie styczne
  public:
    void ModifyTGA(std::string const &dir = "");
    void CreateE3D(std::string const &dir = "", bool dyn = false);
    void CabChange(TDynamicObject *old, TDynamicObject *now);
};
//---------------------------------------------------------------------------

