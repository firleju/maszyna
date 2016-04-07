/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#ifndef MoverH
#define MoverH
//---------------------------------------------------------------------------
#include "Mover.hpp"
// Ra: Niestety "_mover.hpp" si� nieprawid�owo generuje - przek�ada sobie TCoupling na sam koniec.
// Przy wszelkich poprawkach w "_mover.pas" trzeba skopiowa� r�cznie "_mover.hpp" do "mover.hpp" i
// poprawi� b��dy! Tak a� do wydzielnia TCoupling z Pascala do C++...
// Docelowo obs�ug� sprz�g�w (��czenie, roz��czanie, obliczanie odleg�o�ci, przesy� komend)
// trzeba przenie�� na poziom DynObj.cpp.
// Obs�ug� silninik�w te� trzeba wydzieli� do osobnego modu�u, bo ka�dy osobno mo�e mie� po�lizg.
#include "dumb3d.h"
using namespace Math3D;

enum TProblem // lista problem�w taboru, kt�re uniemo�liwiaj� jazd�
{ // flagi bitowe
    pr_Hamuje = 1, // pojazd ma za��czony hamulec lub zatarte osie
    pr_Pantografy = 2, // pojazd wymaga napompowania pantograf�w
    pr_Ostatni = 0x80000000 // ostatnia flaga bitowa
};

class TMoverParameters : public T_MoverParameters
{ // Ra: wrapper na kod pascalowy, przejmuj�cy jego funkcje
  public:
    vector3 vCoulpler[2]; // powt�rzenie wsp�rz�dnych sprz�g�w z DynObj :/
    vector3 DimHalf; // po�owy rozmiar�w do oblicze� geometrycznych
    // int WarningSignal; //0: nie trabi, 1,2: trabi syren� o podanym numerze
    unsigned char WarningSignal; // tymczasowo 8bit, ze wzgl�du na funkcje w MTools
    double fBrakeCtrlPos; // p�ynna nastawa hamulca zespolonego
    bool bPantKurek3; // kurek tr�jdrogowy (pantografu): true=po��czenie z ZG, false=po��czenie z
    // ma�� spr�ark�
    int iProblem; // flagi problem�w z taborem, aby AI nie musia�o por�wnywa�; 0=mo�e jecha�
    int iLights[2]; // bity zapalonych �wiate� tutaj, �eby da�o si� liczy� pob�r pr�du
  private:
    double CouplerDist(Byte Coupler);

  public:
    TMoverParameters(double VelInitial, AnsiString TypeNameInit, AnsiString NameInit,
                     int LoadInitial, AnsiString LoadTypeInitial, int Cab);
    // obs�uga sprz�g�w
    double Distance(const TLocation &Loc1, const TLocation &Loc2, const TDimension &Dim1,
                    const TDimension &Dim2);
    double Distance(const vector3 &Loc1, const vector3 &Loc2, const vector3 &Dim1,
                    const vector3 &Dim2);
    bool Attach(Byte ConnectNo, Byte ConnectToNr, TMoverParameters *ConnectTo, Byte CouplingType,
                bool Forced = false);
    bool Attach(Byte ConnectNo, Byte ConnectToNr, T_MoverParameters *ConnectTo, Byte CouplingType,
                bool Forced = false);
    int DettachStatus(Byte ConnectNo);
    bool Dettach(Byte ConnectNo);
    void SetCoupleDist();
    bool DirectionForward();
    void BrakeLevelSet(double b);
    bool BrakeLevelAdd(double b);
    bool IncBrakeLevel(); // wersja na u�ytek AI
    bool DecBrakeLevel();
    bool ChangeCab(int direction);
    bool CurrentSwitch(int direction);
    void UpdateBatteryVoltage(double dt);
    double ComputeMovement(double dt, double dt1, const TTrackShape &Shape, TTrackParam &Track,
                           TTractionParam &ElectricTraction, const TLocation &NewLoc,
                           TRotation &NewRot);
    double FastComputeMovement(double dt, const TTrackShape &Shape, TTrackParam &Track,
                               const TLocation &NewLoc, TRotation &NewRot);
    double ShowEngineRotation(int VehN);
    // double GetTrainsetVoltage(void);
    // bool Physic_ReActivation(void);
    // double LocalBrakeRatio(void);
    // double ManualBrakeRatio(void);
    // double PipeRatio(void);
    // double RealPipeRatio(void);
    // double BrakeVP(void);
    // bool DynamicBrakeSwitch(bool Switch);
    // bool SendCtrlBroadcast(AnsiString CtrlCommand, double ctrlvalue);
    // bool SendCtrlToNext(AnsiString CtrlCommand, double ctrlvalue, double dir);
    // bool CabActivisation(void);
    // bool CabDeactivisation(void);
    // bool IncMainCtrl(int CtrlSpeed);
    // bool DecMainCtrl(int CtrlSpeed);
    // bool IncScndCtrl(int CtrlSpeed);
    // bool DecScndCtrl(int CtrlSpeed);
    // bool AddPulseForce(int Multipler);
    // bool SandDoseOn(void);
    // bool SecuritySystemReset(void);
    // void SecuritySystemCheck(double dt);
    // bool BatterySwitch(bool State);
    // bool EpFuseSwitch(bool State);
    // bool IncBrakeLevelOld(void);
    // bool DecBrakeLevelOld(void);
    // bool IncLocalBrakeLevel(Byte CtrlSpeed);
    // bool DecLocalBrakeLevel(Byte CtrlSpeed);
    // bool IncLocalBrakeLevelFAST(void);
    // bool DecLocalBrakeLevelFAST(void);
    // bool IncManualBrakeLevel(Byte CtrlSpeed);
    // bool DecManualBrakeLevel(Byte CtrlSpeed);
    // bool EmergencyBrakeSwitch(bool Switch);
    // bool AntiSlippingBrake(void);
    // bool BrakeReleaser(Byte state);
    // bool SwitchEPBrake(Byte state);
    // bool AntiSlippingButton(void);
    // bool IncBrakePress(double &brake, double PressLimit, double dp);
    // bool DecBrakePress(double &brake, double PressLimit, double dp);
    // bool BrakeDelaySwitch(Byte BDS);
    // bool IncBrakeMult(void);
    // bool DecBrakeMult(void);
    // void UpdateBrakePressure(double dt);
    // void UpdatePipePressure(double dt);
    // void CompressorCheck(double dt);
    void UpdatePantVolume(double dt);
    // void UpdateScndPipePressure(double dt);
    // void UpdateBatteryVoltage(double dt);
    // double GetDVc(double dt);
    // void ComputeConstans(void);
    // double ComputeMass(void);
    // double Adhesive(double staticfriction);
    // double TractionForce(double dt);
    // double FrictionForce(double R, Byte TDamage);
    // double BrakeForce(const TTrackParam &Track);
    // double CouplerForce(Byte CouplerN, double dt);
    // void CollisionDetect(Byte CouplerN, double dt);
    // double ComputeRotatingWheel(double WForce, double dt, double n);
    // bool SetInternalCommand(AnsiString NewCommand, double NewValue1, double
    // NewValue2);
    // double GetExternalCommand(AnsiString &Command);
    // bool RunCommand(AnsiString command, double CValue1, double CValue2);
    // bool RunInternalCommand(void);
    // void PutCommand(AnsiString NewCommand, double NewValue1, double NewValue2, const
    // TLocation
    //	&NewLocation);
    // bool DirectionBackward(void);
    // bool MainSwitch(bool State);
    // bool ConverterSwitch(bool State);
    // bool CompressorSwitch(bool State);
    void ConverterCheck();
    // bool FuseOn(void);
    // bool FuseFlagCheck(void);
    // void FuseOff(void);
    int ShowCurrent(Byte AmpN);
    // double v2n(void);
    // double current(double n, double U);
    // double Momentum(double I);
    // double MomentumF(double I, double Iw, Byte SCP);
    // bool CutOffEngine(void);
    // bool MaxCurrentSwitch(bool State);
    // bool ResistorsFlagCheck(void);
    // bool MinCurrentSwitch(bool State);
    // bool AutoRelaySwitch(bool State);
    // bool AutoRelayCheck(void);
    // bool dizel_EngageSwitch(double state);
    // bool dizel_EngageChange(double dt);
    // bool dizel_AutoGearCheck(void);
    // double dizel_fillcheck(Byte mcp);
    // double dizel_Momentum(double dizel_fill, double n, double dt);
    // bool dizel_Update(double dt);
    // bool LoadingDone(double LSpeed, AnsiString LoadInit);
    // void ComputeTotalForce(double dt, double dt1, bool FullVer);
    // double ComputeMovement(double dt, double dt1, const TTrackShape &Shape,
    // TTrackParam &Track
    //	, TTractionParam &ElectricTraction, const TLocation &NewLoc, TRotation &NewRot);
    // double FastComputeMovement(double dt, const TTrackShape &Shape, TTrackParam
    // &Track, const
    //	TLocation &NewLoc, TRotation &NewRot);
    // bool ChangeOffsetH(double DeltaOffset);
    // T_MoverParameters(double VelInitial, AnsiString TypeNameInit, AnsiString NameInit,
    // int LoadInitial
    //	, AnsiString LoadTypeInitial, int Cab);
    // bool LoadChkFile(AnsiString chkpath);
    // bool CheckLocomotiveParameters(bool ReadyFlag, int Dir);
    // AnsiString EngineDescription(int what);
    // bool DoorLeft(bool State);
    // bool DoorRight(bool State);
    // bool DoorBlockedFlag(void);
    // bool PantFront(bool State);
    // bool PantRear(bool State);
    //
};

#endif
