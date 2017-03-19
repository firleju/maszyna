﻿/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "Mover.h"
#include "../globals.h"
#include "../logs.h"
#include "Oerlikon_ESt.h"
#include "../parser.h"
//---------------------------------------------------------------------------

// Ra: tu należy przenosić funcje z mover.pas, które nie są z niego wywoływane.
// Jeśli jakieś zmienne nie są używane w mover.pas, też można je przenosić.
// Przeniesienie wszystkiego na raz zrobiło by zbyt wielki chaos do ogarnięcia.

const double dEpsilon = 0.01; // 1cm (zależy od typu sprzęgu...)
const double CouplerTune = 0.1; // skalowanie tlumiennosci

inline long Trunc(float f)
{
    return (long)f;
}

inline long ROUND(float f)
{
    return Trunc(f + 0.5f);
}

inline double sqr(double val) // SQR() zle liczylo w current() ...
{
    return val * val;
}

double ComputeCollision(double &v1, double &v2, double m1, double m2, double beta, bool vc)
{ // oblicza zmiane predkosci i przyrost pedu wskutek kolizji
    assert( beta < 1.0 );

    if( ( v1 < v2 ) && ( vc == true ) )
		return 0;
	else
	{
		double sum = m1 + m2;
        double w1 = ( m2 * v2 * 2.0 + v1 * ( m1 - m2 ) ) / sum;
        double w2 = ( m1 * v1 * 2.0 + v2 * ( m2 - m1 ) ) / sum;
		v1 = w1 * std::sqrt(1.0 - beta); // niejawna zmiana predkosci wskutek zderzenia
		v2 = w2 * std::sqrt(1.0 - beta);
		return m1 * (w2 - w1) * (1 - beta);
	}
}

int DirPatch(int Coupler1, int Coupler2)
{ // poprawka dla liczenia sil przy ustawieniu przeciwnym obiektow
	return (Coupler1 != Coupler2 ? 1 : -1);
}

int DirF(int CouplerN)
{
	switch (CouplerN)
	{
	case 0:
		return -1;
	case 1:
		return 1;
	default:
		return 0;
	}
}

// *************************************************************************************************
// Q: 20160716
// Obliczanie natężenie prądu w silnikach
// *************************************************************************************************
double TMoverParameters::current(double n, double U)
{
    // wazna funkcja - liczy prad plynacy przez silniki polaczone szeregowo lub rownolegle
    // w zaleznosci od polozenia nastawnikow MainCtrl i ScndCtrl oraz predkosci obrotowej n
    // a takze wywala bezpiecznik nadmiarowy gdy za duzy prad lub za male napiecie
    // jest takze mozliwosc uszkodzenia silnika wskutek nietypowych parametrow
    double const ep09resED = 5.8; // TODO: dobrac tak aby sie zgadzalo ze wbudzeniem

    double R, MotorCurrent;
    double Rz, Delta, Isf;
    double Mn; // przujmuje int, ale dla poprawnosci obliczeń
    double Bn;
    int SP = 0;
    double U1; // napiecie z korekta

    MotorCurrent = 0;
    // i dzialanie hamulca ED w EP09
    if (DynamicBrakeType == dbrake_automatic)
    {
        if (((Hamulec->GetEDBCP() < 0.25) && (Vadd < 1)) || (BrakePress > 2.1))
            DynamicBrakeFlag = false;
        else if ((BrakePress > 0.25) && (Hamulec->GetEDBCP() > 0.25))
            DynamicBrakeFlag = true;
        DynamicBrakeFlag = (DynamicBrakeFlag && ConverterFlag);
    }

    // wylacznik cisnieniowy yBARC - to jest chyba niepotrzebne tutaj   Q: no to usuwam...

    // BrakeSubsystem = ss_LSt;
    // if (BrakeSubsystem == ss_LSt) WriteLog("LSt");
    // if (BrakeSubsystem == ss_LSt) // zrobiona funkcja virtualna
    if (DynamicBrakeFlag)
    {
        Hamulec->SetED(abs(Im / 350)); // hamulec ED na EP09 dziala az do zatrzymania lokomotywy
        //-     WriteLog("A");
    }
    else
    {
        Hamulec->SetED(0);
        //-     WriteLog("B");
    }

    ResistorsFlag = (RList[MainCtrlActualPos].R > 0.01); // and (!DelayCtrlFlag)
    ResistorsFlag =
        (ResistorsFlag || ((DynamicBrakeFlag == true) && (DynamicBrakeType == dbrake_automatic)));

    if ((TrainType == dt_ET22) && (DelayCtrlFlag) && (MainCtrlActualPos > 1))
        Bn = 1.0 - 1.0 / RList[MainCtrlActualPos].Bn;
    else
        Bn = 1; // to jest wykonywane dla EU07

    R = RList[MainCtrlActualPos].R * Bn + CircuitRes;

    if ((TrainType != dt_EZT) || (Imin != IminLo) ||
        (!ScndS)) // yBARC - boczniki na szeregu poprawnie
        Mn = RList[MainCtrlActualPos].Mn; // to jest wykonywane dla EU07
    else
        Mn = RList[MainCtrlActualPos].Mn * RList[MainCtrlActualPos].Bn;

    //  writepaslog("#",
    //  "C++-----------------------------------------------------------------------------");
    //  writepaslog("MCAP           ", IntToStr(MainCtrlActualPos));
    //  writepaslog("SCAP           ", IntToStr(ScndCtrlActualPos));
    //  writepaslog("n              ", FloatToStr(n));
    //  writepaslog("StLinFlag      ", BoolToYN(StLinFlag));
    //  writepaslog("DelayCtrlFlag  ", booltoYN(DelayCtrlFlag));
    //  writepaslog("Bn             ", FloatToStr(Bn));
    //  writepaslog("R              ", FloatToStr(R));
    //  writepaslog("Mn             ", IntToStr(Mn));
    //  writepaslog("RList[MCAP].Bn ", FloatToStr(RList[MainCtrlActualPos].Bn));
    //  writepaslog("RList[MCAP].Mn ", FloatToStr(RList[MainCtrlActualPos].Mn));
    //  writepaslog("RList[MCAP].R  ", FloatToStr(RList[MainCtrlActualPos].R));

    // z Megapacka ... bylo tutaj zakomentowane    Q: no to usuwam...

    if (DynamicBrakeFlag && (!FuseFlag) && (DynamicBrakeType == dbrake_automatic) &&
        ConverterFlag && Mains) // hamowanie EP09   //TUHEX
    {
        MotorCurrent =
            -Max0R(MotorParam[0].fi * (Vadd / (Vadd + MotorParam[0].Isat) - MotorParam[0].fi0), 0) *
            n * 2.0 / ep09resED; // TODO: zrobic bardziej uniwersalne nie tylko dla EP09
    }
    else if ((RList[MainCtrlActualPos].Bn == 0) || (!StLinFlag))
        MotorCurrent = 0; // wylaczone
    else
    { // wlaczone...
        SP = ScndCtrlActualPos;

        if (ScndCtrlActualPos < 255) // tak smiesznie bede wylaczal
        {
            if (ScndInMain)
                if (!(RList[MainCtrlActualPos].ScndAct == 255))
                    SP = RList[MainCtrlActualPos].ScndAct;

            Rz = Mn * WindingRes + R;

            if (DynamicBrakeFlag) // hamowanie
            {
                if (DynamicBrakeType > 1)
                {
					// if DynamicBrakeType<>dbrake_automatic then
					// MotorCurrent:=-fi*n/Rz  {hamowanie silnikiem na oporach rozruchowych}
					/*               begin
					U:=0;
					Isf:=Isat;
					Delta:=SQR(Isf*Rz+Mn*fi*n-U)+4*U*Isf*Rz;
					MotorCurrent:=(U-Isf*Rz-Mn*fi*n+SQRT(Delta))/(2*Rz)
					end*/
					if ((DynamicBrakeType == dbrake_switch) && (TrainType == dt_ET42))
					{ // z Megapacka
						Rz = WindingRes + R;
						MotorCurrent =
							-MotorParam[SP].fi * n / Rz; //{hamowanie silnikiem na oporach rozruchowych}
					}
				}
                else
                    MotorCurrent = 0; // odciecie pradu od silnika
            }
            else
            {
                U1 = U + Mn * n * MotorParam[SP].fi0 * MotorParam[SP].fi;
                //          writepaslog("U1             ", FloatToStr(U1));
                //          writepaslog("Isat           ", FloatToStr(MotorParam[SP].Isat));
                //          writepaslog("fi             ", FloatToStr(MotorParam[SP].fi));
                Isf = Sign(U1) * MotorParam[SP].Isat;
                //          writepaslog("Isf            ", FloatToStr(Isf));
                Delta = sqr(Isf * Rz + Mn * MotorParam[SP].fi * n - U1) +
                        4.0 * U1 * Isf * Rz; // 105 * 1.67 + Mn * 140.9 * 20.532 - U1
                //          DeltaQ = Isf * Rz + Mn * MotorParam[SP].fi * n - U1 + 4 * U1 * Isf * Rz;
                //          writepaslog("Delta          ", FloatToStr(Delta));
                //          writepaslog("DeltaQ         ", FloatToStr(DeltaQ));
                //          writepaslog("U              ", FloatToStr(U));
                if (Mains)
                {
                    if (U > 0)
                        MotorCurrent =
                            (U1 - Isf * Rz - Mn * MotorParam[SP].fi * n + sqrt(Delta)) / (2.0 * Rz);
                    else
                        MotorCurrent =
                            (U1 - Isf * Rz - Mn * MotorParam[SP].fi * n - sqrt(Delta)) / (2.0 * Rz);
                }
                else
                    MotorCurrent = 0;
            } // else DBF

        } // 255
        else
            MotorCurrent = 0;
    }
    //  writepaslog("MotorCurrent   ", FloatToStr(MotorCurrent));

	if ((DynamicBrakeType == dbrake_switch) && ((BrakePress > 2.0) || (PipePress < 3.6)))
	{
		Im = 0;
		MotorCurrent = 0;
		// Im:=0;
		Itot = 0;
	}
	else
		Im = MotorCurrent;

    EnginePower = abs(Itot) * (1 + RList[MainCtrlActualPos].Mn) * abs(U);

    // awarie
    MotorCurrent = abs(Im); // zmienna pomocnicza

	if (MotorCurrent > 0)
	{
		if (FuzzyLogic(abs(n), nmax * 1.1, p_elengproblem))
			if (MainSwitch(false))
				EventFlag = true; /*zbyt duze obroty - wywalanie wskutek ognia okreznego*/
		if (TestFlag(DamageFlag, dtrain_engine))
			if (FuzzyLogic(MotorCurrent, (double)ImaxLo / 10.0, p_elengproblem))
				if (MainSwitch(false))
					EventFlag = true; /*uszkodzony silnik (uplywy)*/
		if ((FuzzyLogic(abs(Im), Imax * 2, p_elengproblem) ||
			FuzzyLogic(abs(n), nmax * 1.11, p_elengproblem)))
			/*       or FuzzyLogic(Abs(U/Mn),2*NominalVoltage,1)) then */ /*poprawic potem*/
			if ((SetFlag(DamageFlag, dtrain_engine)))
				EventFlag = true;
		/*! dorobic grzanie oporow rozruchowych i silnika*/
	}

    return Im;
}

// *************************************************************************************************
//  główny konstruktor
// *************************************************************************************************
TMoverParameters::TMoverParameters(double VelInitial, std::string TypeNameInit,
                                   std::string NameInit, int LoadInitial,
                                   std::string LoadTypeInitial,
                                   int Cab) ://: T_MoverParameters(VelInitial, TypeNameInit,
                                            //NameInit, LoadInitial, LoadTypeInitial, Cab)
TypeName( TypeNameInit ),
ActiveCab( Cab ),
LoadType( LoadTypeInitial ),
Load( LoadInitial ),
Name( NameInit )
{
    WriteLog(
        "------------------------------------------------------");
    WriteLog("init default physic values for " + NameInit + ", [" + TypeNameInit + "], [" +
             LoadTypeInitial + "]");
    Dim = TDimension();
    DimHalf.x = 0.5 * Dim.W; // połowa szerokości, OX jest w bok?
    DimHalf.y = 0.5 * Dim.L; // połowa długości, OY jest do przodu?
    DimHalf.z = 0.5 * Dim.H; // połowa wysokości, OZ jest w górę?

    // BrakeLevelSet(-2); //Pascal ustawia na 0, przestawimy na odcięcie (CHK jest jeszcze nie wczytane!)
    iLights[ 0 ] = 0;
    iLights[ 1 ] = 0; //światła zgaszone

    // inicjalizacja stalych
    for (int b = 0; b < ResArraySize + 1; ++b)
    {
        RList[b] = TScheme();
    }
    RlistSize = 0;
    for(int b = 0; b < MotorParametersArraySize + 1; ++b) {
        MotorParam[ b ] = TMotorParameters();
    }

    for (int b = 0; b < 2; ++b)
        for (int k = 0; k < 17; ++k)
            Lights[b][k] = 0;

    for (int b = -1; b <= MainBrakeMaxPos; ++b)
    {
        BrakePressureTable[b].PipePressureVal = 0.0;
        BrakePressureTable[b].BrakePressureVal = 0.0;
        BrakePressureTable[b].FlowSpeedVal = 0.0;
    }
    // with BrakePressureTable[-2] do  {pozycja odciecia}
    {
        BrakePressureTable[-2].PipePressureVal = -1.0;
        BrakePressureTable[-2].BrakePressureVal = -1.0;
        BrakePressureTable[-2].FlowSpeedVal = 0.0;
    }
    for( int b = 0; b < 4; ++b ) {
        BrakeDelay[ b ] = 0.0;
    }

    for (int b = 0; b < 2; ++b) // Ra: kto tu zrobił "for b:=1 to 2 do" ???
    {
        Couplers[b].CouplerType = NoCoupler;
        Couplers[b].SpringKB = 1.0;
        Couplers[b].SpringKC = 1.0;
        Couplers[b].DmaxB = 0.1;
        Couplers[b].FmaxB = 1000.0;
        Couplers[b].DmaxC = 0.1;
        Couplers[b].FmaxC = 1000.0;
    }
    for(int b = 0; b < 2; ++b ) {
        HVCouplers[ b ][ 0 ] = 0.0;
        HVCouplers[ b ][ 1 ] = 0.0;
    }

    for( int b = 0; b < 3; ++b ) {
        BrakeCylMult[ b ] = 0.0;
    }

    for( int b = 0; b < 26; ++b ) {
        eimc[ b ] = 0.0;
    }
	eimc[eimc_p_eped] = 1.5;

    // inicjalizacja zmiennych}
    // Loc:=LocInitial; //Ra: to i tak trzeba potem przesunąć, po ustaleniu pozycji na torze
    // (potrzebna długość)
    // Rot:=RotInitial;
    for (int b = 0; b < 2; ++b)
    {
        Couplers[b].AllowedFlag = 3; // domyślnie hak i hamulec, inne trzeba włączyć jawnie w FIZ
        Couplers[b].CouplingFlag = 0;
        Couplers[b].Connected = NULL;
        Couplers[b].ConnectedNr = 0; // Ra: to nie ma znaczenia jak nie podłączony
        Couplers[b].Render = false;
        Couplers[b].CForce = 0.0;
        Couplers[b].Dist = 0.0;
        Couplers[b].CheckCollision = false;
    }

    for (int b = 0; b < 5; ++b)
    {
        MaxBrakePress[b] = 0.0;
    }

    Vel = abs(VelInitial);
    V = VelInitial / 3.6;

    for( int b = 0; b < 21; b++ ) {
        eimv[ b ] = 0.0;
    }

    RunningShape.Len = 1.0;

    RunningTrack.CategoryFlag = CategoryFlag;
    RunningTrack.Width = TrackW;
    RunningTrack.friction = Steel2Steel_friction;
    RunningTrack.QualityFlag = 20;
    RunningTrack.DamageFlag = 0;
    RunningTrack.Velmax = 100.0; // dla uzytku maszynisty w ai_driver}

    RunningTraction.TractionVoltage = 0.0;
    RunningTraction.TractionFreq = 0.0;
    RunningTraction.TractionMaxCurrent = 0.0;
    RunningTraction.TractionResistivity = 1.0;

    SecuritySystem.SystemType = 0;
    SecuritySystem.AwareDelay = -1.0;
    SecuritySystem.SoundSignalDelay = -1.0;
    SecuritySystem.EmergencyBrakeDelay = -1.0;
    SecuritySystem.Status = 0;
    SecuritySystem.SystemTimer = 0.0;
    SecuritySystem.SystemBrakeCATimer = 0.0;
    SecuritySystem.SystemBrakeSHPTimer = 0.0; // hunter-091012
    SecuritySystem.VelocityAllowed = -1;
    SecuritySystem.NextVelocityAllowed = -1;
    SecuritySystem.RadioStop = false; // domyślnie nie ma
    SecuritySystem.AwareMinSpeed = 0.1 * Vmax;
    s_CAtestebrake = false;
    // ABu 240105:
    // CouplerNr[0]:=1;
    // CouplerNr[1]:=0;

    // TO POTEM TU UAKTYWNIC A WYWALIC Z CHECKPARAM}
    //{
    //  if Pos(LoadTypeInitial,LoadAccepted)>0 then
    //   begin
    //}

    //{
    //   end
    //  else Load:=0;
    // }
};

double TMoverParameters::Distance(const TLocation &Loc1, const TLocation &Loc2,
                                  const TDimension &Dim1, const TDimension &Dim2)
{ // zwraca odległość pomiędzy pojazdami (Loc1) i (Loc2) z uwzględnieneim ich długości (kule!)
    return hypot(Loc2.X - Loc1.X, Loc1.Y - Loc2.Y) - 0.5 * (Dim2.L + Dim1.L);
};
/*
double TMoverParameters::Distance(const vector3 &s1, const vector3 &s2, const vector3 &d1,
                                  const vector3 &d2){
    // obliczenie odległości prostopadłościanów o środkach (s1) i (s2) i wymiarach (d1) i (d2)
    // return 0.0; //będzie zgłaszać warning - funkcja do usunięcia, chyba że się przyda...
};
*/
double TMoverParameters::CouplerDist(int Coupler)
{ // obliczenie odległości pomiędzy sprzęgami (kula!)
    return Couplers[Coupler].CoupleDist =
               Distance(Loc, Couplers[Coupler].Connected->Loc, Dim,
                        Couplers[Coupler].Connected->Dim); // odległość pomiędzy sprzęgami (kula!)
};

bool TMoverParameters::Attach(int ConnectNo, int ConnectToNr, TMoverParameters *ConnectTo,
                               int CouplingType, bool Forced)
{ //łączenie do swojego sprzęgu (ConnectNo) pojazdu (ConnectTo) stroną (ConnectToNr)
    // Ra: zwykle wykonywane dwukrotnie, dla każdego pojazdu oddzielnie
    // Ra: trzeba by odróżnić wymóg dociśnięcia od uszkodzenia sprzęgu przy podczepianiu AI do
    // składu
    if (ConnectTo) // jeśli nie pusty
    {
        if (ConnectToNr != 2)
            Couplers[ConnectNo].ConnectedNr = ConnectToNr; // 2=nic nie podłączone
        TCouplerType ct = ConnectTo->Couplers[Couplers[ConnectNo].ConnectedNr]
                              .CouplerType; // typ sprzęgu podłączanego pojazdu
        Couplers[ConnectNo].Connected =
            ConnectTo; // tak podpiąć (do siebie) zawsze można, najwyżej będzie wirtualny
        CouplerDist(ConnectNo); // przeliczenie odległości pomiędzy sprzęgami
        if (CouplingType == ctrain_virtual)
            return false; // wirtualny więcej nic nie robi
        if (Forced ? true : ((Couplers[ConnectNo].CoupleDist <= dEpsilon) &&
                             (Couplers[ConnectNo].CouplerType != NoCoupler) &&
                             (Couplers[ConnectNo].CouplerType == ct)))
        { // stykaja sie zderzaki i kompatybilne typy sprzegow, chyba że łączenie na starcie
            if (Couplers[ConnectNo].CouplingFlag ==
                ctrain_virtual) // jeśli wcześniej nie było połączone
            { // ustalenie z której strony rysować sprzęg
                Couplers[ConnectNo].Render = true; // tego rysować
                ConnectTo->Couplers[Couplers[ConnectNo].ConnectedNr].Render = false; // a tego nie
            };
            Couplers[ConnectNo].CouplingFlag = CouplingType; // ustawienie typu sprzęgu
            // if (CouplingType!=ctrain_virtual) //Ra: wirtualnego nie łączymy zwrotnie!
            //{//jeśli łączenie sprzęgiem niewirtualnym, ustawiamy połączenie zwrotne
            ConnectTo->Couplers[Couplers[ConnectNo].ConnectedNr].CouplingFlag = CouplingType;
            ConnectTo->Couplers[Couplers[ConnectNo].ConnectedNr].Connected = this;
            ConnectTo->Couplers[Couplers[ConnectNo].ConnectedNr].CoupleDist =
                Couplers[ConnectNo].CoupleDist;
            return true;
            //}
            // podłączenie nie udało się - jest wirtualne
        }
    }
    return false; // brak podłączanego pojazdu, zbyt duża odległość, niezgodny typ sprzęgu, brak
    // sprzęgu, brak haka
};

// to jest już niepotrzebne bo nie ma Delphi
//bool TMoverParameters::Attach(int ConnectNo, int ConnectToNr, TMoverParameters *ConnectTo,
//                              int CouplingType, bool Forced)
//{ //łączenie do (ConnectNo) pojazdu (ConnectTo) stroną (ConnectToNr)
//    return Attach(ConnectNo, ConnectToNr, (TMoverParameters *)ConnectTo, CouplingType, Forced);
//};

int TMoverParameters::DettachStatus(int ConnectNo)
{ // Ra: sprawdzenie, czy odległość jest dobra do rozłączania
    // powinny być 3 informacje: =0 sprzęg już rozłączony, <0 da się rozłączyć. >0 nie da się
    // rozłączyć
    if (!Couplers[ConnectNo].Connected)
        return 0; // nie ma nic, to rozłączanie jest OK
    if ((Couplers[ConnectNo].CouplingFlag & ctrain_coupler) == 0)
        return -Couplers[ConnectNo].CouplingFlag; // hak nie połączony - rozłączanie jest OK
    if (TestFlag(DamageFlag, dtrain_coupling))
        return -Couplers[ConnectNo].CouplingFlag; // hak urwany - rozłączanie jest OK
    // ABu021104: zakomentowane 'and (CouplerType<>Articulated)' w warunku, nie wiem co to bylo, ale
    // za to teraz dziala odczepianie... :) }
    // if (CouplerType==Articulated) return false; //sprzęg nie do rozpięcia - może być tylko urwany
    // Couplers[ConnectNo].CoupleDist=Distance(Loc,Couplers[ConnectNo].Connected->Loc,Dim,Couplers[ConnectNo].Connected->Dim);
    CouplerDist(ConnectNo);
    if (Couplers[ConnectNo].CouplerType == Screw ? Couplers[ConnectNo].CoupleDist < 0.0 : true)
        return -Couplers[ConnectNo].CouplingFlag; // można rozłączać, jeśli dociśnięty
    return (Couplers[ConnectNo].CoupleDist > 0.2) ? -Couplers[ConnectNo].CouplingFlag :
                                                    Couplers[ConnectNo].CouplingFlag;
};

bool TMoverParameters::Dettach(int ConnectNo)
{ // rozlaczanie
    if (!Couplers[ConnectNo].Connected)
        return true; // nie ma nic, to odczepiono
    // with Couplers[ConnectNo] do
    int i = DettachStatus(ConnectNo); // stan sprzęgu
    if (i < 0)
    { // gdy scisniete zderzaki, chyba ze zerwany sprzeg (wirtualnego nie odpinamy z drugiej strony)
        // Couplers[ConnectNo].Connected=NULL; //lepiej zostawic bo przeciez trzeba kontrolowac
        // zderzenia odczepionych
        Couplers[ConnectNo].Connected->Couplers[Couplers[ConnectNo].ConnectedNr].CouplingFlag =
            0; // pozostaje sprzęg wirtualny
        Couplers[ConnectNo].CouplingFlag = 0; // pozostaje sprzęg wirtualny
        return true;
    }
    else if (i > 0)
    { // odłączamy węże i resztę, pozostaje sprzęg fizyczny, który wymaga dociśnięcia (z wirtualnym
        // nic)
        Couplers[ConnectNo].CouplingFlag &= ctrain_coupler;
        Couplers[ConnectNo].Connected->Couplers[Couplers[ConnectNo].ConnectedNr].CouplingFlag =
            Couplers[ConnectNo].CouplingFlag;
    }
    return false; // jeszcze nie rozłączony
};

void TMoverParameters::SetCoupleDist()
{ // przeliczenie odległości sprzęgów
    if (Couplers[0].Connected)
    {
        CouplerDist(0);
        if (CategoryFlag & 2)
        { // Ra: dla samochodów zderzanie kul to za mało
        }
    }
    if (Couplers[1].Connected)
    {
        CouplerDist(1);
        if (CategoryFlag & 2)
        { // Ra: dla samochodów zderzanie kul to za mało
        }
    }
};

bool TMoverParameters::DirectionForward()
{
    if ((MainCtrlPosNo > 0) && (ActiveDir < 1) && (MainCtrlPos == 0))
    {
        ++ActiveDir;
        DirAbsolute = ActiveDir * CabNo;
        if (DirAbsolute)
            if (Battery) // jeśli bateria jest już załączona
                BatterySwitch(true); // to w ten oto durny sposób aktywuje się CA/SHP
        SendCtrlToNext("Direction", ActiveDir, CabNo);
        return true;
    }
    else if ((ActiveDir == 1) && (MainCtrlPos == 0) && (TrainType == dt_EZT))
        return MinCurrentSwitch(true); //"wysoki rozruch" EN57
    return false;
};

// Nastawianie hamulców

void TMoverParameters::BrakeLevelSet(double b)
{ // ustawienie pozycji hamulca na wartość (b) w zakresie od -2 do BrakeCtrlPosNo
    // jedyny dopuszczalny sposób przestawienia hamulca zasadniczego
    if (fBrakeCtrlPos == b)
        return; // nie przeliczać, jak nie ma zmiany
    fBrakeCtrlPos = b;
    BrakeCtrlPosR = b;
    if (fBrakeCtrlPos < Handle->GetPos(bh_MIN))
        fBrakeCtrlPos = Handle->GetPos(bh_MIN); // odcięcie
    else if (fBrakeCtrlPos > Handle->GetPos(bh_MAX))
        fBrakeCtrlPos = Handle->GetPos(bh_MAX);
    int x = static_cast<int>(std::floor(fBrakeCtrlPos)); // jeśli odwołujemy się do BrakeCtrlPos w pośrednich, to musi być
    // obcięte a nie zaokrągone
    while ((x > BrakeCtrlPos) && (BrakeCtrlPos < BrakeCtrlPosNo)) // jeśli zwiększyło się o 1
        if (!IncBrakeLevelOld()) // T_MoverParameters::
            break; // wyjście awaryjne
    while ((x < BrakeCtrlPos) && (BrakeCtrlPos >= -1)) // jeśli zmniejszyło się o 1
        if (!DecBrakeLevelOld()) // T_MoverParameters::
            break;
    BrakePressureActual = BrakePressureTable[BrakeCtrlPos]; // skopiowanie pozycji
    /*
    //youBy: obawiam sie, ze tutaj to nie dziala :P
    //Ra 2014-03: było tak zrobione, że działało - po każdej zmianie pozycji była wywoływana ta
    funkcja
    // if (BrakeSystem==Pneumatic?BrakeSubsystem==Oerlikon:false) //tylko Oerlikon akceptuje ułamki
     if(false)
      if (fBrakeCtrlPos>0.0)
      {//wartości pośrednie wyliczamy tylko dla hamowania
       double u=fBrakeCtrlPos-double(x); //ułamek ponad wartość całkowitą
       if (u>0.0)
       {//wyliczamy wartości ważone
        BrakePressureActual.PipePressureVal+=-u*BrakePressureActual.PipePressureVal+u*BrakePressureTable[BrakeCtrlPos+1+2].PipePressureVal;
        //BrakePressureActual.BrakePressureVal+=-u*BrakePressureActual.BrakePressureVal+u*BrakePressureTable[BrakeCtrlPos+1].BrakePressureVal;
    //to chyba nie będzie tak działać, zwłaszcza w EN57
        BrakePressureActual.FlowSpeedVal+=-u*BrakePressureActual.FlowSpeedVal+u*BrakePressureTable[BrakeCtrlPos+1+2].FlowSpeedVal;
       }
      }
    */
};

bool TMoverParameters::BrakeLevelAdd(double b)
{ // dodanie wartości (b) do pozycji hamulca (w tym ujemnej)
    // zwraca false, gdy po dodaniu było by poza zakresem
    BrakeLevelSet(fBrakeCtrlPos + b);
    return b > 0.0 ? (fBrakeCtrlPos < BrakeCtrlPosNo) :
                     (BrakeCtrlPos > -1.0); // true, jeśli można kontynuować
};

bool TMoverParameters::IncBrakeLevel()
{ // nowa wersja na użytek AI, false gdy osiągnięto pozycję BrakeCtrlPosNo
    return BrakeLevelAdd(1.0);
};

bool TMoverParameters::DecBrakeLevel()
{
    return BrakeLevelAdd(-1.0);
}; // nowa wersja na użytek AI, false gdy osiągnięto pozycję -1

bool TMoverParameters::ChangeCab(int direction)
{ // zmiana kabiny i resetowanie ustawien
    if (abs(ActiveCab + direction) < 2)
    {
        //  if (ActiveCab+direction=0) then LastCab:=ActiveCab;
        ActiveCab = ActiveCab + direction;
        if ((BrakeSystem == Pneumatic) && (BrakeCtrlPosNo > 0))
        {
            //    if (BrakeHandle==FV4a)   //!!!POBIERAĆ WARTOŚĆ Z KLASY ZAWORU!!!
            //     BrakeLevelSet(-2); //BrakeCtrlPos=-2;
            //    else if ((BrakeHandle==FVel6)||(BrakeHandle==St113))
            //     BrakeLevelSet(2);
            //    else
            //     BrakeLevelSet(1);
            BrakeLevelSet(Handle->GetPos(bh_NP));
            LimPipePress = PipePress;
            ActFlowSpeed = 0;
        }
        else
            // if (TrainType=dt_EZT) and (BrakeCtrlPosNo>0) then
            //  BrakeCtrlPos:=5; //z Megapacka
            // else
            //    BrakeLevelSet(0); //BrakeCtrlPos=0;
            BrakeLevelSet(Handle->GetPos(bh_NP));
        //   if not TestFlag(BrakeStatus,b_dmg) then
        //    BrakeStatus:=b_off; //z Megapacka
        MainCtrlPos = 0;
        ScndCtrlPos = 0;
        // Ra: to poniżej jest bez sensu - można przejść nie wyłączając
        // if ((EngineType!=DieselEngine)&&(EngineType!=DieselElectric))
        //{
        // Mains=false;
        // CompressorAllow=false;
        // ConverterAllow=false;
        //}
        // ActiveDir=0;
        // DirAbsolute=0;
        return true;
    }
    return false;
};

bool TMoverParameters::CurrentSwitch(int direction)
{ // rozruch wysoki (true) albo niski (false)
    // Ra: przeniosłem z Train.cpp, nie wiem czy ma to sens
    if (MaxCurrentSwitch(direction != 0))
    {
        if (TrainType != dt_EZT)
            return (MinCurrentSwitch(direction != 0));
    }
    if (EngineType == DieselEngine) // dla 2Ls150
        if (ShuntModeAllow)
            if (ActiveDir == 0) // przed ustawieniem kierunku
                ShuntMode = ( direction != 0 );
    return false;
};

void TMoverParameters::UpdatePantVolume(double dt)
{ // KURS90 - sprężarka pantografów; Ra 2014-07: teraz jest to zbiornik rozrządu, chociaż to jeszcze
    // nie tak
    if (EnginePowerSource.SourceType == CurrentCollector) // tylko jeśli pantografujący
    {
        // Ra 2014-07: zasadniczo, to istnieje zbiornik rozrządu i zbiornik pantografów - na razie
        // mamy razem
        // Ra 2014-07: kurek trójdrogowy łączy spr.pom. z pantografami i wyłącznikiem ciśnieniowym
        // WS
        // Ra 2014-07: zbiornika rozrządu nie pompuje się tu, tylko pantografy; potem można zamknąć
        // WS i odpalić resztę
        if ((TrainType == dt_EZT) ? (PantPress < ScndPipePress) :
                                    bPantKurek3) // kurek zamyka połączenie z ZG
        { // zbiornik pantografu połączony ze zbiornikiem głównym - małą sprężarką się tego nie
            // napompuje
            // Ra 2013-12: Niebugocław mówi, że w EZT nie ma potrzeby odcinać kurkiem
            PantPress = EnginePowerSource.CollectorParameters
                            .MaxPress; // ograniczenie ciśnienia do MaxPress (tylko w pantografach!)
            if (PantPress > ScndPipePress)
                PantPress = ScndPipePress; // oraz do ScndPipePress
            PantVolume = (PantPress + 1.0) * 0.1; // objętość, na wypadek odcięcia kurkiem
        }
        else
        { // zbiornik główny odcięty, można pompować pantografy
            if (PantCompFlag && Battery) // włączona bateria i mała sprężarka
                PantVolume += dt * (TrainType == dt_EZT ? 0.003 : 0.005) *
                              (2.0 * 0.45 - ((0.1 / PantVolume / 10) - 0.1)) /
                              0.45; // napełnianie zbiornika pantografów
            // Ra 2013-12: Niebugocław mówi, że w EZT nabija 1.5 raz wolniej niż jak było 0.005
            PantPress = (10.0 * PantVolume) - 1.0; // tu by się przydała objętość zbiornika
        }
        if (!PantCompFlag && (PantVolume > 0.1))
            PantVolume -= dt * 0.0003; // nieszczelności: 0.0003=0.3l/s
        if (Mains) // nie wchodzić w funkcję bez potrzeby
            if (EngineType == ElectricSeriesMotor) // nie dotyczy... czego właściwie?
                if (PantPress < EnginePowerSource.CollectorParameters.MinPress)
                    if ((TrainType & (dt_EZT | dt_ET40 | dt_ET41 | dt_ET42)) ?
                            (GetTrainsetVoltage() < EnginePowerSource.CollectorParameters.MinV) :
                            true) // to jest trochę proteza; zasilanie członu może być przez sprzęg
                        // WN
                        if (MainSwitch(false))
                            EventFlag = true; // wywalenie szybkiego z powodu niskiego ciśnienia
        if (TrainType != dt_EZT) // w EN57 pompuje się tylko w silnikowym
            // pierwotnie w CHK pantografy miały również rozrządcze EZT
            for (int b = 0; b <= 1; ++b)
                if (TestFlag(Couplers[b].CouplingFlag, ctrain_controll))
                    if (Couplers[b].Connected->PantVolume <
                        PantVolume) // bo inaczej trzeba w obydwu członach przestawiać
                        Couplers[b].Connected->PantVolume =
                            PantVolume; // przekazanie ciśnienia do sąsiedniego członu
        // czy np. w ET40, ET41, ET42 pantografy członów mają połączenie pneumatyczne?
        // Ra 2014-07: raczej nie - najpierw się załącza jeden człon, a potem można podnieść w
        // drugim
    }
    else
    { // a tu coś dla SM42 i SM31, aby pokazywać na manometrze
        PantPress = CntrlPipePress;
    }
};

void TMoverParameters::UpdateBatteryVoltage(double dt)
{ // przeliczenie obciążenia baterii
    double sn1 = 0.0,
           sn2 = 0.0,
           sn3 = 0.0,
           sn4 = 0.0,
           sn5 = 0.0; // Ra: zrobić z tego amperomierz NN
    if ((BatteryVoltage > 0) && (EngineType != DieselEngine) && (EngineType != WheelsDriven) &&
        (NominalBatteryVoltage > 0))
    {
        if ((NominalBatteryVoltage / BatteryVoltage < 1.22) && Battery)
        { // 110V
            if (!ConverterFlag)
                sn1 = (dt * 2.0); // szybki spadek do ok 90V
            else
                sn1 = 0;
            if (ConverterFlag)
                sn2 = -(dt * 2.0); // szybki wzrost do 110V
            else
                sn2 = 0;
            if (Mains)
                sn3 = (dt * 0.05);
            else
                sn3 = 0;
            if (iLights[0] & 63) // 64=blachy, nie ciągną prądu //rozpisać na poszczególne
                // żarówki...
                sn4 = dt * 0.003;
            else
                sn4 = 0;
            if (iLights[1] & 63) // 64=blachy, nie ciągną prądu
                sn5 = dt * 0.001;
            else
                sn5 = 0;
        };
        if ((NominalBatteryVoltage / BatteryVoltage >= 1.22) && Battery)
        { // 90V
            if (PantCompFlag)
                sn1 = (dt * 0.0046);
            else
                sn1 = 0;
            if (ConverterFlag)
                sn2 = -(dt * 50); // szybki wzrost do 110V
            else
                sn2 = 0;
            if (Mains)
                sn3 = (dt * 0.001);
            else
                sn3 = 0;
            if (iLights[0] & 63) // 64=blachy, nie ciągną prądu
                sn4 = (dt * 0.0030);
            else
                sn4 = 0;
            if (iLights[1] & 63) // 64=blachy, nie ciągną prądu
                sn5 = (dt * 0.0010);
            else
                sn5 = 0;
        };
        if (!Battery)
        {
            if (NominalBatteryVoltage / BatteryVoltage < 1.22)
                sn1 = dt * 50;
            else
                sn1 = 0;
            sn2 = dt * 0.000001;
            sn3 = dt * 0.000001;
            sn4 = dt * 0.000001;
            sn5 = dt * 0.000001; // bardzo powolny spadek przy wyłączonych bateriach
        };
        BatteryVoltage -= (sn1 + sn2 + sn3 + sn4 + sn5);
        if (NominalBatteryVoltage / BatteryVoltage > 1.57)
            if (MainSwitch(false) && (EngineType != DieselEngine) && (EngineType != WheelsDriven))
                EventFlag = true; // wywalanie szybkiego z powodu zbyt niskiego napiecia
        if (BatteryVoltage > NominalBatteryVoltage)
            BatteryVoltage = NominalBatteryVoltage; // wstrzymanie ładowania pow. 110V
        if (BatteryVoltage < 0.01)
            BatteryVoltage = 0.01;
    }
    else if (NominalBatteryVoltage == 0)
        BatteryVoltage = 0;
    else
        BatteryVoltage = 90;
};

/* Ukrotnienie EN57:
1 //układ szeregowy
2 //układ równoległy
3 //bocznik 1
4 //bocznik 2
5 //bocznik 3
6 //do przodu
7 //do tyłu
8 //1 przyspieszenie
9 //minus obw. 2 przyspieszenia
10 //jazda na oporach
11 //SHP
12A //podnoszenie pantografu przedniego
12B //podnoszenie pantografu tylnego
13A //opuszczanie pantografu przedniego
13B //opuszczanie wszystkich pantografów
14 //załączenie WS
15 //rozrząd (WS, PSR, wał kułakowy)
16 //odblok PN
18 //sygnalizacja przetwornicy głównej
19 //luzowanie EP
20 //hamowanie EP
21 //rezerwa** (1900+: zamykanie drzwi prawych)
22 //zał. przetwornicy głównej
23 //wył. przetwornicy głównej
24 //zał. przetw. oświetlenia
25 //wył. przetwornicy oświetlenia
26 //sygnalizacja WS
28 //sprężarka
29 //ogrzewanie
30 //rezerwa* (1900+: zamykanie drzwi lewych)
31 //otwieranie drzwi prawych
32H //zadziałanie PN siln. trakcyjnych
33 //sygnał odjazdu
34 //rezerwa (sygnalizacja poślizgu)
35 //otwieranie drzwi lewych
ZN //masa
*/

// *****************************************************************************
// Q: 20160714
// Oblicza iloraz aktualnej pozycji do maksymalnej hamulca pomocnicznego
// *****************************************************************************
double TMoverParameters::LocalBrakeRatio(void)
{
    double LBR;
    if (BrakeHandle == MHZ_EN57)
        if ((BrakeOpModeFlag >= bom_EP))
            LBR = Handle->GetEP(BrakeCtrlPosR);
        else
            LBR = 0;
	else
	{
		if (LocalBrakePosNo > 0)
			LBR = (double)LocalBrakePos / LocalBrakePosNo;
		else
			LBR = 0;
	}
    // if (TestFlag(BrakeStatus, b_antislip))
    //   LBR = Max0R(LBR, PipeRatio) + 0.4;
    return LBR;
}

// *****************************************************************************
// Q: 20160714
// Oblicza iloraz aktualnej pozycji do maksymalnej hamulca ręcznego
// *****************************************************************************
double TMoverParameters::ManualBrakeRatio(void)
{
    double MBR;

    if (ManualBrakePosNo > 0)
        MBR = (double)ManualBrakePos / ManualBrakePosNo;
    else
        MBR = 0;
    return MBR;
}

// *****************************************************************************
// Q: 20160713
// Zwraca objętość
// *****************************************************************************
double TMoverParameters::BrakeVP(void)
{
    if (BrakeVVolume > 0)
        return Volume / (10.0 * BrakeVVolume);
    else
        return 0;
}

// *****************************************************************************
// Q: 20160713
// Zwraca iloraz różnicy między przewodem kontrolnym i głównym oraz DeltaPipePress
// *****************************************************************************
double TMoverParameters::RealPipeRatio(void)
{
    double rpp;

    if (DeltaPipePress > 0)
        rpp = (CntrlPipePress - PipePress) / (DeltaPipePress);
    else
        rpp = 0;
    return rpp;
}

// *****************************************************************************
// Q: 20160713
// Zwraca iloraz ciśnienia w przewodzie do DeltaPipePress
// *****************************************************************************
double TMoverParameters::PipeRatio(void)
{
    double pr;

    if (DeltaPipePress > 0)
        if (false) // SPKS!! no to jak nie wchodzimy to po co branch?
        {
            if ((3.0 * PipePress) > (HighPipePress + LowPipePress + LowPipePress))
                pr = (HighPipePress - Min0R(HighPipePress, PipePress)) /
                     (DeltaPipePress * 4.0 / 3.0);
            else
                pr = (HighPipePress - 1.0 / 3.0 * DeltaPipePress - Max0R(LowPipePress, PipePress)) /
                     (DeltaPipePress * 2.0 / 3.0);
            //if (not TestFlag(BrakeStatus, b_Ractive))
            //    and(BrakeMethod and 1 = 0) and TestFlag(BrakeDelays, bdelay_R) and (Power < 1) and
            //        (BrakeCtrlPos < 1) then pr : = Min0R(0.5, pr);
            //if (Compressor > 0.5)
            //    then pr : = pr * 1.333; // dziwny rapid wywalamy
        }
        else
            pr = (HighPipePress - Max0R(LowPipePress, Min0R(HighPipePress, PipePress))) /
                 DeltaPipePress;
    else
        pr = 0;
    return pr;
}

// *************************************************************************************************
// Q: 20160716
// Wykrywanie kolizji
// *************************************************************************************************
void TMoverParameters::CollisionDetect(int CouplerN, double dt)
{
    double CCF, Vprev, VprevC;
    bool VirtualCoupling;

    CCF = 0;
    //   with Couplers[CouplerN] do
    auto &coupler = Couplers[ CouplerN ];

    if (coupler.Connected != nullptr)
    {
        VirtualCoupling = (coupler.CouplingFlag == ctrain_virtual);
        Vprev = V;
        VprevC = coupler.Connected->V;
        switch (CouplerN)
        {
        case 0:
            CCF =
                ComputeCollision(
                    V,
                    coupler.Connected->V,
                    TotalMass,
                    coupler.Connected->TotalMass,
                    (coupler.beta + coupler.Connected->Couplers[coupler.ConnectedNr].beta) / 2.0,
                    VirtualCoupling)
                / (dt);
            break; // yB: ej ej ej, a po
        case 1:
            CCF =
                ComputeCollision(
                    coupler.Connected->V,
                    V, coupler.Connected->TotalMass,
                    TotalMass,
                    (coupler.beta + coupler.Connected->Couplers[coupler.ConnectedNr].beta) / 2.0,
                    VirtualCoupling)
                / (dt);
            break; // czemu tu jest +0.01??
        }
        AccS = AccS + (V - Vprev) / dt; // korekta przyspieszenia o siły wynikające ze zderzeń?
        coupler.Connected->AccS += (coupler.Connected->V - VprevC) / dt;
        if ((coupler.Dist > 0) && (!VirtualCoupling))
            if (FuzzyLogic(abs(CCF), 5.0 * (coupler.FmaxC + 1.0), p_coupldmg))
            { //! zerwanie sprzegu
                if (SetFlag(DamageFlag, dtrain_coupling))
                    EventFlag = true;

                if ((coupler.CouplingFlag & ctrain_pneumatic) == ctrain_pneumatic)
                    EmergencyBrakeFlag = true; // hamowanie nagle - zerwanie przewodow hamulcowych
                coupler.CouplingFlag = 0;

                switch (CouplerN) // wyzerowanie flag podlaczenia ale ciagle sa wirtualnie polaczone
                {
                case 0:
                    coupler.Connected->Couplers[1].CouplingFlag = 0;
                    break;
                case 1:
                    coupler.Connected->Couplers[0].CouplingFlag = 0;
                    break;
                }
            }
    }
}

// *************************************************************************************************
// Oblicza przemieszczenie taboru
// *************************************************************************************************
double TMoverParameters::ComputeMovement(double dt, double dt1, const TTrackShape &Shape,
                                         TTrackParam &Track, TTractionParam &ElectricTraction,
                                         const TLocation &NewLoc, TRotation &NewRot)
{
    const double Vepsilon = 1e-5;
    const double  Aepsilon = 1e-3; // ASBSpeed=0.8;
    int b;
    double Vprev, AccSprev, d, hvc;

    // T_MoverParameters::ComputeMovement(dt, dt1, Shape, Track, ElectricTraction, NewLoc, NewRot);
    // // najpierw kawalek z funkcji w pliku mover.pas
	TotalCurrent = 0;
	hvc = Max0R(Max0R(PantFrontVolt, PantRearVolt), ElectricTraction.TractionVoltage * 0.9);
    for (b = 0; b < 2; b++) // przekazywanie napiec
        if (((Couplers[b].CouplingFlag & ctrain_power) == ctrain_power) ||
            (((Couplers[b].CouplingFlag & ctrain_heating) == ctrain_heating) && (Heating)))
        {
            HVCouplers[1 - b][1] =
                Max0R(abs(hvc), Couplers[b].Connected->HVCouplers[Couplers[b].ConnectedNr][1] -
                                    HVCouplers[b][0] * 0.02);
        }
        else
            HVCouplers[1 - b][1] = abs(hvc) - HVCouplers[b][0] * 0.02;
    // Max0R(Abs(Voltage),0);
    //      end;

    hvc = HVCouplers[0][1] + HVCouplers[1][1];

    if ((abs(PantFrontVolt) + abs(PantRearVolt) < 1) &&
		(hvc > 1)) // bez napiecia, ale jest cos na sprzegach:
	{
		for (b = 0; b < 2; ++b) // przekazywanie pradow
			if (((Couplers[b].CouplingFlag & ctrain_power) == ctrain_power) ||
				(((Couplers[b].CouplingFlag & ctrain_heating) == ctrain_heating) &&
					(Heating))) // jesli spiety
			{
				HVCouplers[b][0] =
					Couplers[b].Connected->HVCouplers[1 - Couplers[b].ConnectedNr][0] +
					Itot * HVCouplers[b][1] / hvc; // obciążenie rozkladane stosownie do napiec
			}
			else // pierwszy pojazd
			{
				HVCouplers[b][0] = Itot * HVCouplers[b][1] / hvc;
			}
	}
	else
	{
		if (((Couplers[0].CouplingFlag & ctrain_power) == ctrain_power) ||
			(((Couplers[0].CouplingFlag & ctrain_heating) == ctrain_heating) && (Heating)))
			TotalCurrent +=
			Couplers[0].Connected->HVCouplers[1 - Couplers[0].ConnectedNr][0];
		if (((Couplers[1].CouplingFlag & ctrain_power) == ctrain_power) ||
			(((Couplers[1].CouplingFlag & ctrain_heating) == ctrain_heating) && (Heating)))
			TotalCurrent +=
			Couplers[1].Connected->HVCouplers[1 - Couplers[1].ConnectedNr][0];
		HVCouplers[0][0] = 0;
		HVCouplers[1][0] = 0;
	}

    if (!TestFlag(DamageFlag, dtrain_out))
    { // Ra: to przepisywanie tu jest bez sensu
        RunningShape = Shape;
        RunningTrack = Track;
        RunningTraction = ElectricTraction;

        //if (!DynamicBrakeFlag)
        //    RunningTraction.TractionVoltage = ElectricTraction.TractionVoltage /*-
        //                                      abs(ElectricTraction.TractionResistivity *
        //                                          (Itot + HVCouplers[0][0] + HVCouplers[1][0]))*/;
        //else
        //    RunningTraction.TractionVoltage =
        //        ElectricTraction.TractionVoltage /*-
        //        abs(ElectricTraction.TractionResistivity * Itot *
        //            0)*/; // zasadniczo ED oporowe nie zmienia napięcia w sieci
    }

    if (CategoryFlag == 4)
        OffsetTrackV = TotalMass / (Dim.L * Dim.W * 1000.0);
    else if (TestFlag(CategoryFlag, 1) && TestFlag(RunningTrack.CategoryFlag, 1))
        if (TestFlag(DamageFlag, dtrain_out))
        {
            OffsetTrackV = -0.2;
            OffsetTrackH = Sign(RunningShape.R) * 0.2;
        }

    Loc = NewLoc;
    Rot = NewRot;
    NewRot.Rx = 0;
    NewRot.Ry = 0;
    NewRot.Rz = 0;

    if (dL == 0) // oblicz przesuniecie}
    {
        Vprev = V;
        AccSprev = AccS;
        // dt:=ActualTime-LastUpdatedTime;                             //przyrost czasu
        // przyspieszenie styczne
        AccS = (FTotal / TotalMass + AccSprev) /
               2.0; // prawo Newtona ale z wygladzaniem (średnia z poprzednim)

        if (TestFlag(DamageFlag, dtrain_out))
            AccS = -Sign(V) * g * 1; // random(0.0, 0.1)

        // przyspieszenie normalne
        if (abs(Shape.R) > 0.01)
            AccN = sqr(V) / Shape.R + g * Shape.dHrail / TrackW; // Q: zamieniam SQR() na sqr()
        else
            AccN = g * Shape.dHrail / TrackW;

        // szarpanie
        if (FuzzyLogic((10.0 + Track.DamageFlag) * Mass * Vel / Vmax, 500000.0,
                       p_accn)) // Ra: czemu tu masa bez ładunku?
            AccV = sqrt((1.0 + Track.DamageFlag) * Random(floor(50.0 * Mass / 1000000.0)) * Vel /
                        (Vmax * (10.0 + (Track.QualityFlag & 31))));
        else
            AccV = AccV / 2.0;

        if (AccV > 1.0)
            AccN += (7.0 - Random(5)) * (100.0 + Track.DamageFlag / 2.0) * AccV / 2000.0;

        // wykolejanie na luku oraz z braku szyn
        if (TestFlag(CategoryFlag, 1))
        {
            if (FuzzyLogic((AccN / g) * (1.0 + 0.1 * (Track.DamageFlag && dtrack_freerail)),
                           TrackW / Dim.H, 1) ||
                TestFlag(Track.DamageFlag, dtrack_norail))
                if (SetFlag(DamageFlag, dtrain_out))
                {
                    EventFlag = true;
                    Mains = false;
                    RunningShape.R = 0;
                    if (TestFlag(Track.DamageFlag, dtrack_norail))
                        DerailReason = 1; // Ra: powód wykolejenia: brak szyn
                    else
                        DerailReason = 2; // Ra: powód wykolejenia: przewrócony na łuku
                }
            // wykolejanie na poszerzeniu toru
            if (FuzzyLogic(abs(Track.Width - TrackW), TrackW / 10.0, 1))
                if (SetFlag(DamageFlag, dtrain_out))
                {
                    EventFlag = true;
                    Mains = false;
                    RunningShape.R = 0;
                    DerailReason = 3; // Ra: powód wykolejenia: za szeroki tor
                }
        }
        // wykolejanie wkutek niezgodnosci kategorii toru i pojazdu
        if (!TestFlag(RunningTrack.CategoryFlag, CategoryFlag))
            if (SetFlag(DamageFlag, dtrain_out))
            {
                EventFlag = true;
                Mains = false;
                DerailReason = 4; // Ra: powód wykolejenia: nieodpowiednia trajektoria
            }
        V += (3.0 * AccS - AccSprev) * dt / 2.0; // przyrost predkosci
        if (TestFlag(DamageFlag, dtrain_out))
            if (Vel < 1)
            {
                V = 0;
                AccS = 0;
            }

        if ((V * Vprev <= 0) && (abs(FStand) > abs(FTrain))) // tlumienie predkosci przy hamowaniu
        { // zahamowany
            V = 0;
            // AccS:=0; //Ra 2014-03: ale siła grawitacji działa, więc nie może być zerowe
        }

        //   { dL:=(V+AccS*dt/2)*dt;                                      //przyrost dlugosci czyli
        //   przesuniecie
        dL = (3.0 * V - Vprev) * dt / 2.0; // metoda Adamsa-Bashfortha}
        // ale jesli jest kolizja (zas. zach. pedu) to...}
        for (b = 0; b < 2; b++)
            if (Couplers[b].CheckCollision)
                CollisionDetect(b, dt); // zmienia niejawnie AccS, V !!!

    } // liczone dL, predkosc i przyspieszenie

    if (Power > 1.0) // w rozrządczym nie (jest błąd w FIZ!) - Ra 2014-07: teraz we wszystkich
        UpdatePantVolume(dt); // Ra 2014-07: obsługa zbiornika rozrządu oraz pantografów

    if (EngineType == WheelsDriven)
        d = (double)CabNo * dL; // na chwile dla testu
    else
        d = dL;
    DistCounter += fabs(dL) / 1000.0;
    dL = 0;

    // koniec procedury, tu nastepuja dodatkowe procedury pomocnicze

    // sprawdzanie i ewentualnie wykonywanie->kasowanie poleceń
    if (LoadStatus > 0) // czas doliczamy tylko jeśli trwa (roz)ładowanie
        LastLoadChangeTime += dt; // czas (roz)ładunku

    RunInternalCommand();

    // automatyczny rozruch
    if (EngineType == ElectricSeriesMotor)

        if (AutoRelayCheck())
            SetFlag(SoundFlag, sound_relay);

    if (EngineType == DieselEngine)
        if (dizel_Update(dt))
            SetFlag(SoundFlag, sound_relay);
    // uklady hamulcowe:
    if (VeselVolume > 0)
        Compressor = CompressedVolume / VeselVolume;
    else
    {
        Compressor = 0;
        CompressorFlag = false;
    };
    ConverterCheck();
    if (CompressorSpeed > 0.0) // sprężarka musi mieć jakąś niezerową wydajność
        CompressorCheck(dt); //żeby rozważać jej załączenie i pracę
    UpdateBrakePressure(dt);
    UpdatePipePressure(dt);
    UpdateBatteryVoltage(dt);
    UpdateScndPipePressure(dt); // druga rurka, youBy
    // hamulec antypoślizgowy - wyłączanie
    if ((BrakeSlippingTimer > 0.8) && (ASBType != 128)) // ASBSpeed=0.8
        Hamulec->ASB(0);
    // SetFlag(BrakeStatus,-b_antislip);
    BrakeSlippingTimer += dt;
    // sypanie piasku - wyłączone i piasek się nie kończy - błędy AI
    // if AIControllFlag then
    // if SandDose then
    //  if Sand>0 then
    //   begin
    //     Sand:=Sand-NPoweredAxles*SandSpeed*dt;
    //     if Random<dt then SandDose:=false;
    //   end
    //  else
    //   begin
    //     SandDose:=false;
    //     Sand:=0;
    //   end;
    // czuwak/SHP
    // if (Vel>10) and (not DebugmodeFlag) then
    if (!DebugModeFlag)
        SecuritySystemCheck(dt1);
    return d;
};

// *************************************************************************************************
// Oblicza przemieszczenie taboru - uproszczona wersja
// *************************************************************************************************

double TMoverParameters::FastComputeMovement(double dt, const TTrackShape &Shape,
                                             TTrackParam &Track, const TLocation &NewLoc,
                                             TRotation &NewRot)
{
    double Vprev, AccSprev, d;
    int b;
    // T_MoverParameters::FastComputeMovement(dt, Shape, Track, NewLoc, NewRot);

    Loc = NewLoc;
    Rot = NewRot;
    NewRot.Rx = 0.0;
    NewRot.Ry = 0.0;
    NewRot.Rz = 0.0;

    if (dL == 0) // oblicz przesuniecie
    {
        Vprev = V;
        AccSprev = AccS;
        // dt =ActualTime-LastUpdatedTime;                               //przyrost czasu
        // przyspieszenie styczne
        AccS = (FTotal / TotalMass + AccSprev) /
               2.0; // prawo Newtona ale z wygladzaniem (średnia z poprzednim)

        if (TestFlag(DamageFlag, dtrain_out))
            AccS = -Sign(V) * g * 1; // * random(0.0, 0.1)
        // przyspieszenie normalne}
        //     if Abs(Shape.R)>0.01 then
        //      AccN:=SQR(V)/Shape.R+g*Shape.dHrail/TrackW
        //     else AccN:=g*Shape.dHrail/TrackW;

        // szarpanie}
        if (FuzzyLogic((10.0 + Track.DamageFlag) * Mass * Vel / Vmax, 500000.0, p_accn))
        {
            AccV = sqrt((1.0 + Track.DamageFlag) * Random(floor(50.0 * Mass / 1000000.0)) * Vel /
                        (Vmax * (10.0 + (Track.QualityFlag & 31)))); // Trunc na floor, czy dobrze?
        }
        else
            AccV = AccV / 2.0;

        if (AccV > 1.0)
            AccN += (7.0 - Random(5)) * (100.0 + Track.DamageFlag / 2.0) * AccV / 2000.0;

        //     {wykolejanie na luku oraz z braku szyn}
        //     if TestFlag(CategoryFlag,1) then
        //      begin
        //       if FuzzyLogic((AccN/g)*(1+0.1*(Track.DamageFlag and
        //       dtrack_freerail)),TrackW/Dim.H,1)
        //       or TestFlag(Track.DamageFlag,dtrack_norail) then
        //        if SetFlag(DamageFlag,dtrain_out) then
        //         begin
        //           EventFlag:=true;
        //           MainS:=false;
        //           RunningShape.R:=0;
        //         end;
        //     {wykolejanie na poszerzeniu toru}
        //        if FuzzyLogic(Abs(Track.Width-TrackW),TrackW/10,1) then
        //         if SetFlag(DamageFlag,dtrain_out) then
        //          begin
        //            EventFlag:=true;
        //            MainS:=false;
        //            RunningShape.R:=0;
        //          end;
        //      end;
        //     {wykolejanie wkutek niezgodnosci kategorii toru i pojazdu}
        //     if not TestFlag(RunningTrack.CategoryFlag,CategoryFlag) then
        //      if SetFlag(DamageFlag,dtrain_out) then
        //        begin
        //          EventFlag:=true;
        //          MainS:=false;
        //        end;

        V += (3.0 * AccS - AccSprev) * dt / 2.0; // przyrost predkosci

        if (TestFlag(DamageFlag, dtrain_out))
            if (Vel < 1)
            {
                V = 0;
                AccS = 0; // Ra 2014-03: ale siła grawitacji działa, więc nie może być zerowe
            }

        if ((V * Vprev <= 0) && (abs(FStand) > abs(FTrain))) // tlumienie predkosci przy hamowaniu
        { // zahamowany}
            V = 0;
            AccS = 0;
        }
        dL = (3.0 * V - Vprev) * dt / 2.0; // metoda Adamsa-Bashfortha
        // ale jesli jest kolizja (zas. zach. pedu) to...
        for (b = 0; b < 2; b++)
            if (Couplers[b].CheckCollision)
                CollisionDetect(b, dt); // zmienia niejawnie AccS, V !!!
    } // liczone dL, predkosc i przyspieszenie
    // QQQ
    if (Power > 1.0) // w rozrządczym nie (jest błąd w FIZ!)
        UpdatePantVolume(dt); // Ra 2014-07: obsługa zbiornika rozrządu oraz pantografów
    if (EngineType == WheelsDriven)
        d = (double)CabNo * dL; // na chwile dla testu
    else
        d = dL;
    DistCounter += fabs(dL) / 1000.0;
    dL = 0;

    // koniec procedury, tu nastepuja dodatkowe procedury pomocnicze

    // sprawdzanie i ewentualnie wykonywanie->kasowanie poleceń
    if (LoadStatus > 0) // czas doliczamy tylko jeśli trwa (roz)ładowanie
        LastLoadChangeTime += dt; // czas (roz)ładunku

    RunInternalCommand();

    if (EngineType == DieselEngine)
        if (dizel_Update(dt))
            SetFlag(SoundFlag, sound_relay);
    // uklady hamulcowe:
    if (VeselVolume > 0)
        Compressor = CompressedVolume / VeselVolume;
    else
    {
        Compressor = 0;
        CompressorFlag = false;
    };
    ConverterCheck();
    if (CompressorSpeed > 0.0) // sprężarka musi mieć jakąś niezerową wydajność
        CompressorCheck(dt); //żeby rozważać jej załączenie i pracę
    UpdateBrakePressure(dt);
    UpdatePipePressure(dt);
    UpdateScndPipePressure(dt); // druga rurka, youBy
    UpdateBatteryVoltage(dt);
    // hamulec antyposlizgowy - wyłączanie
    if ((BrakeSlippingTimer > 0.8) && (ASBType != 128)) // ASBSpeed=0.8
        Hamulec->ASB(0);
    BrakeSlippingTimer += dt;
    return d;
};

double TMoverParameters::ShowEngineRotation(int VehN)
{ // Zwraca wartość prędkości obrotowej silnika wybranego pojazdu. Do 3 pojazdów (3×SN61).
    int b;
    switch (VehN)
    { // numer obrotomierza
    case 1:
        return fabs(enrot);
    case 2:
        for (b = 0; b <= 1; ++b)
            if (TestFlag(Couplers[b].CouplingFlag, ctrain_controll))
                if (Couplers[b].Connected->Power > 0.01)
                    return fabs(Couplers[b].Connected->enrot);
        break;
    case 3: // to nie uwzględnia ewentualnego odwrócenia pojazdu w środku
        for (b = 0; b <= 1; ++b)
            if (TestFlag(Couplers[b].CouplingFlag, ctrain_controll))
                if (Couplers[b].Connected->Power > 0.01)
                    if (TestFlag(Couplers[b].Connected->Couplers[b].CouplingFlag, ctrain_controll))
                        if (Couplers[b].Connected->Couplers[b].Connected->Power > 0.01)
                            return fabs(Couplers[b].Connected->Couplers[b].Connected->enrot);
        break;
    };
    return 0.0;
};

void TMoverParameters::ConverterCheck()
{ // sprawdzanie przetwornicy
    if (ConverterAllow && Mains)
        ConverterFlag = true;
    else
        ConverterFlag = false;
};

double TMoverParameters::ShowCurrent(int AmpN)
{ // Odczyt poboru prądu na podanym amperomierzu
    switch (EngineType)
    {
    case ElectricInductionMotor:
        switch (AmpN)
        { // do asynchronicznych
        case 1:
            return WindingRes * Mm / Vadd;
        case 2:
            return dizel_fill * WindingRes;
        default:
            return ShowCurrentP(AmpN); // T_MoverParameters::
        }
        break;
    case DieselElectric:
        return fabs(Im);
        break;
    default:
        return ShowCurrentP(AmpN); // T_MoverParameters::
    }
};

// *************************************************************************************************
// queuedEU
// *************************************************************************************************

// *************************************************************************************************
// Q: 20160710
// zwiększenie nastawinika
// *************************************************************************************************

bool TMoverParameters::IncMainCtrl(int CtrlSpeed)
{
	// basic fail conditions:
    if( ( MainCtrlPosNo <= 0 )
     || ( CabNo == 0 ) ) {
		// nie ma sterowania
		return false;
	}
	if( ( TrainType == dt_ET22 ) && ( ScndCtrlPos != 0 ) ) {
        // w ET22 nie da się kręcić nastawnikiem przy włączonym boczniku
		return false;
	}
	if( ( TrainType == dt_EZT ) && ( ActiveDir == 0 ) ) {
        // w EZT nie da się załączyć pozycji bez ustawienia kierunku
		return false;
	}

    bool OK = false;
        if (MainCtrlPos < MainCtrlPosNo)
        {
		switch( EngineType ) {
                case None:
                case Dumb:
                case DieselElectric:
                case ElectricInductionMotor:
                {
				if( CtrlSpeed > 1 ) {
					OK = ( IncMainCtrl( 1 )
						&& IncMainCtrl( CtrlSpeed - 1 ) ); // a fail will propagate up the recursion chain. should this be || instead?
				}
				else {
					++MainCtrlPos;
                        OK = true;
                    }
                    break;
                }

                case ElectricSeriesMotor:
                {
				if( ActiveDir == 0 ) { return false; }

				if( CtrlSpeed > 1 ) {
					// szybkie przejœcie na bezoporow¹
					if( TrainType == dt_ET40 ) {
						break; // this means ET40 won't react at all to fast acceleration command. should it issue just IncMainCtrl(1) instead?
                                }
					while( ( RList[ MainCtrlPos ].R > 0.0 )
						&& IncMainCtrl( 1 ) ) {
						// all work is done in the loop header
						;
					}
					// OK:=true ; {takie chamskie, potem poprawie} <-Ra: kto mia³ to
					// poprawiæ i po co?
					if( ActiveDir == -1 ) {
						while( ( RList[ MainCtrlPos ].Bn > 1 )
							&& IncMainCtrl( 1 ) ) {
							--MainCtrlPos;
						}
					}
					OK = false; // shouldn't this be part of the loop above?
					// if (TrainType=dt_ET40)  then
					// while Abs (Im)>IminHi do
					//   dec(MainCtrlPos);
					//  OK:=false ;
				}
				else { // CtrlSpeed == 1
					++MainCtrlPos;
					OK = true;
					if( Imax == ImaxHi ) {
						if( RList[ MainCtrlPos ].Bn > 1 ) {
							if( true == MaxCurrentSwitch( false )) {
								// wylaczanie wysokiego rozruchu
								SetFlag( SoundFlag, sound_relay );
							} // Q TODO:
                                //         if (EngineType=ElectricSeriesMotor) and (MainCtrlPos=1)
                                //         then
                                //           MainCtrlActualPos:=1;
                                //
							if( TrainType == dt_ET42 ) {
								--MainCtrlPos;
								OK = false;
                            }
						}
					}
					if( ActiveDir == -1 ) {
						if( ( TrainType != dt_PseudoDiesel )
						 && ( RList[ MainCtrlPos ].Bn > 1 )	) {
                            // blokada wejścia na równoległą podczas jazdy do tyłu
							--MainCtrlPos;
                            OK = false;
                        }
					}
                        //
                        // if (TrainType == "et40")
                        //  if (Abs(Im) > IminHi)
                        //   {
                        //    MainCtrlPos--; //Blokada nastawnika po przekroczeniu minimalnego pradu
                        //    OK = false;
                        //   }
                        //}
                                }

				if( ( TrainType == dt_ET42 ) && ( true == DynamicBrakeFlag ) ) {
					if( MainCtrlPos > 20 ) {
						MainCtrlPos = 20;
                        OK = false;
                    }
				}
                    // return OK;
                    break;
                }

                case DieselEngine:
                {
				if( CtrlSpeed > 1 ) {
					while( MainCtrlPos < MainCtrlPosNo ) {
						IncMainCtrl( 1 );
                    }
                    }
				else {
					++MainCtrlPos;
					if( MainCtrlPos > 0 ) { CompressorAllow = true; }
					else                  { CompressorAllow = false; }
				}
				OK = true;
                    break;
                }

                case WheelsDriven:
                {
				OK = AddPulseForce( CtrlSpeed );
                    break;
                }
                } // switch EngineType of
        }
    else {// MainCtrlPos>=MainCtrlPosNo
		if( true == CoupledCtrl ) {
            // wspólny wał nastawnika jazdy i bocznikowania
			if( ScndCtrlPos < ScndCtrlPosNo ) { // 3<3 -> false
				++ScndCtrlPos;
                OK = true;
            }
			else {
                OK = false;
        }
		}
    }

    if( true == OK )
        {
            SendCtrlToNext("MainCtrl", MainCtrlPos, CabNo); //???
            SendCtrlToNext("ScndCtrl", ScndCtrlPos, CabNo);
        }

    // hunter-101012: poprawka
    // poprzedni warunek byl niezbyt dobry, bo przez to przy trzymaniu +
    // styczniki tkwily na tej samej pozycji (LastRelayTime byl caly czas 0 i rosl
    // po puszczeniu plusa)

    if (OK)
    {
        if (DelayCtrlFlag)
        {
            if ((LastRelayTime >= InitialCtrlDelay) && (MainCtrlPos == 1))
                LastRelayTime = 0;
        }
        else if (LastRelayTime > CtrlDelay)
            LastRelayTime = 0;
    }
    return OK;
}

// *****************************************************************************
// Q: 20160710
// zmniejszenie nastawnika
// *****************************************************************************
bool TMoverParameters::DecMainCtrl(int CtrlSpeed)
{
    bool OK = false;
    // basic fail conditions:
    if( ( MainCtrlPosNo <= 0 )
     || ( CabNo == 0 ) ) {
		// nie ma sterowania
        OK = false;
    }
    else
    {
        if (MainCtrlPos > 0)
        {
            if ((TrainType != dt_ET22) ||
                (ScndCtrlPos == 0)) // Ra: ET22 blokuje nastawnik przy boczniku
            {
                if (CoupledCtrl && (ScndCtrlPos > 0))
                {
                    ScndCtrlPos--; // wspolny wal
                    OK = true;
                }
                else
                    switch (EngineType)
                    {
                    case None:
                    case Dumb:
                    case DieselElectric:
                    case ElectricInductionMotor:
                    {
                        if (((CtrlSpeed == 1) &&
                             /*(ScndCtrlPos==0) and*/ (EngineType != DieselElectric)) ||
                            ((CtrlSpeed == 1) && (EngineType == DieselElectric)))
                        {
                            MainCtrlPos--;
                            OK = true;
                        }
                        else if (CtrlSpeed > 1)
                            OK = (DecMainCtrl(1) && DecMainCtrl(2)); // CtrlSpeed-1);
                        break;
                    }

                    case ElectricSeriesMotor:
                    {
                        if (CtrlSpeed == 1) /*and (ScndCtrlPos=0)*/
                        {
                            MainCtrlPos--;
                            //                if (MainCtrlPos=0) and (ScndCtrlPos=0) and
                            //                (TrainType<>dt_ET40)and(TrainType<>dt_EP05) then
                            //                 StLinFlag:=false;
                            //                if (MainCtrlPos=0) and (TrainType<>dt_ET40) and
                            //                (TrainType<>dt_EP05) then
                            //                 MainCtrlActualPos:=0; //yBARC: co to tutaj robi? ;)
                            OK = true;
                        }
                        else if (CtrlSpeed > 1) /*and (ScndCtrlPos=0)*/
                        {
                            OK = true;
                            if (RList[MainCtrlPos].R == 0) // Q: tu zrobilem = ;]
                                DecMainCtrl(1);
                            while ((RList[MainCtrlPos].R > 0) && DecMainCtrl(1))
                                ; // takie chamskie, potem poprawie}
                        }
                        break;
                    }

                    case DieselEngine:
                    {
                        if (CtrlSpeed == 1)
                        {
                            MainCtrlPos--;
                            OK = true;
                        }
                        else if (CtrlSpeed > 1)
                        {
                            while ((MainCtrlPos > 0) || (RList[MainCtrlPos].Mn > 0))
                                DecMainCtrl(1);
                            OK = true;
                        }
                        break;
                    }
                    } // switch EngineType
            }
        }
        else if (EngineType == WheelsDriven)
            OK = AddPulseForce(-CtrlSpeed);
        else
            OK = false;

        if (OK)
        {
            /*OK:=*/SendCtrlToNext("MainCtrl", MainCtrlPos, CabNo); // hmmmm...???!!!
            /*OK:=*/SendCtrlToNext("ScndCtrl", ScndCtrlPos, CabNo);
        }
    }
    // if OK then LastRelayTime:=0;
    // hunter-101012: poprawka
    if (OK)
    {
        if (DelayCtrlFlag)
        {
            if (LastRelayTime >= InitialCtrlDelay)
                LastRelayTime = 0;
        }
        else if (LastRelayTime > CtrlDownDelay)
            LastRelayTime = 0;
    }
    return OK;
}

// *************************************************************************************************
// Q: 20160710
// zwiększenie bocznika
// *************************************************************************************************
bool TMoverParameters::IncScndCtrl(int CtrlSpeed)
{
    bool OK = false;

    if ((MainCtrlPos == 0) && (CabNo != 0) && (TrainType == dt_ET42) && (ScndCtrlPos == 0) &&
        (DynamicBrakeFlag))
    {
        OK = DynamicBrakeSwitch(false);
    }
    else if ((ScndCtrlPosNo > 0) && (CabNo != 0) &&
             !((TrainType == dt_ET42) &&
               ((Imax == ImaxHi) || ((DynamicBrakeFlag) && (MainCtrlPos > 0)))))
    {
        //     if (RList[MainCtrlPos].R=0) and (MainCtrlPos>0) and (ScndCtrlPos<ScndCtrlPosNo) and
        //     (not CoupledCtrl) then
        if ((ScndCtrlPos < ScndCtrlPosNo) && (!CoupledCtrl) &&
            ((EngineType != DieselElectric) || (!AutoRelayFlag)))
        {
            if (CtrlSpeed == 1)
            {
                ScndCtrlPos++;
            }
            else if (CtrlSpeed > 1)
            {
                ScndCtrlPos = ScndCtrlPosNo; // takie chamskie, potem poprawie
            }
            OK = true;
        }
        else // nie mozna zmienic
            OK = false;
        if (OK)
        {
            /*OK:=*/SendCtrlToNext("MainCtrl", MainCtrlPos, CabNo); //???
            /*OK:=*/SendCtrlToNext("ScndCtrl", ScndCtrlPos, CabNo);
        }
    }
    else // nie ma sterowania
        OK = false;
    // if OK then LastRelayTime:=0;
    // hunter-101012: poprawka
    if (OK)
        if (LastRelayTime > CtrlDelay)
            LastRelayTime = 0;

	if ((OK) && (EngineType == ElectricInductionMotor))
		if ((Vmax < 250))
			ScndCtrlActualPos = Round(Vel + 0.5f);
		else
			ScndCtrlActualPos = Round(Vel * 1.0 / 2 + 0.5);

    return OK;
}

// *************************************************************************************************
// Q: 20160710
// zmniejszenie bocznika
// *************************************************************************************************
bool TMoverParameters::DecScndCtrl(int CtrlSpeed)
{
    bool OK = false;

    if ((MainCtrlPos == 0) && (CabNo != 0) && (TrainType == dt_ET42) && (ScndCtrlPos == 0) &&
        !(DynamicBrakeFlag) && (CtrlSpeed == 1))
    {
        // Ra: AI wywołuje z CtrlSpeed=2 albo gdy ScndCtrlPos>0
        OK = DynamicBrakeSwitch(true);
    }
    else if ((ScndCtrlPosNo > 0) && (CabNo != 0))
    {
        if ((ScndCtrlPos > 0) && (!CoupledCtrl) &&
            ((EngineType != DieselElectric) || (!AutoRelayFlag)))
        {
            if (CtrlSpeed == 1)
            {
                ScndCtrlPos--;
            }
            else if (CtrlSpeed > 1)
            {
                ScndCtrlPos = 0; // takie chamskie, potem poprawie
            }
            OK = true;
        }
        else
            OK = false;
        if (OK)
        {
            /*OK:=*/SendCtrlToNext("MainCtrl", MainCtrlPos, CabNo); //???
            /*OK:=*/SendCtrlToNext("ScndCtrl", ScndCtrlPos, CabNo);
        }
    }
    else
        OK = false;
    // if OK then LastRelayTime:=0;
    // hunter-101012: poprawka
    if (OK)
        if (LastRelayTime > CtrlDownDelay)
            LastRelayTime = 0;

	if ((OK) && (EngineType == ElectricInductionMotor))
		ScndCtrlActualPos = 0;

    return OK;
}

// *************************************************************************************************
// Q: 20160710
// załączenie rozrządu
// *************************************************************************************************
bool TMoverParameters::CabActivisation(void) 
{
    bool OK = false;

    OK = (CabNo == 0); // numer kabiny, z której jest sterowanie
    if (OK)
    {
        CabNo = ActiveCab; // sterowanie jest z kabiny z obsadą
        DirAbsolute = ActiveDir * CabNo;
        SendCtrlToNext("CabActivisation", 1, CabNo);
    }
    return OK;
}

// *************************************************************************************************
// Q: 20160710
// wyłączenie rozrządu
// *************************************************************************************************
bool TMoverParameters::CabDeactivisation(void) 
{
    bool OK = false;

    OK = (CabNo == ActiveCab); // o ile obsada jest w kabinie ze sterowaniem
    if (OK)
    {
        CabNo = 0;
        DirAbsolute = ActiveDir * CabNo;
        DepartureSignal = false; // nie buczeć z nieaktywnej kabiny
        SendCtrlToNext("CabActivisation", 0, ActiveCab); // CabNo==0!
    }
    return OK;
}

// *************************************************************************************************
// Q: 20160710
// Siła napędzająca drezynę po naciśnięciu wajhy
// *************************************************************************************************
bool TMoverParameters::AddPulseForce(int Multipler)
{
    bool APF;
    if ((EngineType == WheelsDriven) && (EnginePowerSource.SourceType == InternalSource) &&
        (EnginePowerSource.PowerType == BioPower))
    {
        ActiveDir = CabNo;
        DirAbsolute = ActiveDir * CabNo;
        if (Vel > 0)
            PulseForce = Min0R(1000.0 * Power / (abs(V) + 0.1), Ftmax);
        else
            PulseForce = Ftmax;
        if (PulseForceCount > 1000.0)
            PulseForce = 0;
        else
            PulseForce = PulseForce * Multipler;
        PulseForceCount = PulseForceCount + abs(Multipler);
        APF = (PulseForce > 0);
    }
    else
        APF = false;
    return APF;
}

// *************************************************************************************************
// Q: 20160713
// sypanie piasku
// *************************************************************************************************
bool TMoverParameters::SandDoseOn(void)
{
    bool SDO;
    if (SandCapacity > 0)
    {
        SDO = true;
        if (SandDose)
            SandDose = false;
        else if (Sand > 0)
            SandDose = true;
        if (CabNo != 0)
            SendCtrlToNext("SandDoseOn", 1, CabNo);
    }
    else
        SDO = false;

    return SDO;
}

void TMoverParameters::SSReset(void)
{ // funkcja pomocnicza dla SecuritySystemReset - w Delphi Reset()
    SecuritySystem.SystemTimer = 0;

    if (TestFlag(SecuritySystem.Status, s_aware))
    {
        SecuritySystem.SystemBrakeCATimer = 0;
        SecuritySystem.SystemSoundCATimer = 0;
        SetFlag(SecuritySystem.Status, -s_aware);
        SetFlag(SecuritySystem.Status, -s_CAalarm);
        SetFlag(SecuritySystem.Status, -s_CAebrake);
        //   EmergencyBrakeFlag = false; //YB-HN
        SecuritySystem.VelocityAllowed = -1;
    }
    else if (TestFlag(SecuritySystem.Status, s_active))
    {
        SecuritySystem.SystemBrakeSHPTimer = 0;
        SecuritySystem.SystemSoundSHPTimer = 0;
        SetFlag(SecuritySystem.Status, -s_active);
        SetFlag(SecuritySystem.Status, -s_SHPalarm);
        SetFlag(SecuritySystem.Status, -s_SHPebrake);
        //   EmergencyBrakeFlag = false; //YB-HN
        SecuritySystem.VelocityAllowed = -1;
    }
}

// *****************************************************************************
// Q: 20160710
// zbicie czuwaka / SHP
// *****************************************************************************
// hunter-091012: rozbicie alarmow, dodanie testu czuwaka
bool TMoverParameters::SecuritySystemReset(void) // zbijanie czuwaka/SHP
{
    // zbijanie czuwaka/SHP
    bool SSR = false;
    // with SecuritySystem do
    if ((SecuritySystem.SystemType > 0) && (SecuritySystem.Status > 0))
    {
        SSR = true;
        if ((TrainType == dt_EZT) ||
            (ActiveDir != 0)) // Ra 2014-03: w EZT nie trzeba ustawiać kierunku
            if (!TestFlag(SecuritySystem.Status, s_CAebrake) ||
                !TestFlag(SecuritySystem.Status, s_SHPebrake))
                SSReset();
        // else
        //  if EmergencyBrakeSwitch(false) then
        //   Reset;
    }
    else
        SSR = false;
    //  SendCtrlToNext('SecurityReset',0,CabNo);
    return SSR;
}

// *************************************************************************************************
// Q: 20160711
// sprawdzanie stanu CA/SHP
// *************************************************************************************************
void TMoverParameters::SecuritySystemCheck(double dt)
{
    // Ra: z CA/SHP w EZT jest ten problem, że w rozrządczym nie ma kierunku, a w silnikowym nie ma
    // obsady
    // poza tym jest zdefiniowany we wszystkich 3 członach EN57
	if ((!Radio))
		EmergencyBrakeSwitch(false);

    if ((SecuritySystem.SystemType > 0) && (SecuritySystem.Status > 0) &&
        (Battery)) // Ra: EZT ma teraz czuwak w rozrządczym
    {
        // CA
        if (Vel >=
            SecuritySystem
                .AwareMinSpeed) // domyślnie predkość większa od 10% Vmax, albo podanej jawnie w FIZ
        {
            SecuritySystem.SystemTimer += dt;
            if (TestFlag(SecuritySystem.SystemType, 1) &&
                TestFlag(SecuritySystem.Status, s_aware)) // jeśli świeci albo miga
                SecuritySystem.SystemSoundCATimer += dt;
            if (TestFlag(SecuritySystem.SystemType, 1) &&
                TestFlag(SecuritySystem.Status, s_CAalarm)) // jeśli buczy
                SecuritySystem.SystemBrakeCATimer += dt;
            if (TestFlag(SecuritySystem.SystemType, 1))
                if ((SecuritySystem.SystemTimer > SecuritySystem.AwareDelay) &&
                    (SecuritySystem.AwareDelay >= 0)) //-1 blokuje
                    if (!SetFlag(SecuritySystem.Status, s_aware)) // juz wlaczony sygnal swietlny
                        if ((SecuritySystem.SystemSoundCATimer > SecuritySystem.SoundSignalDelay) &&
                            (SecuritySystem.SoundSignalDelay >= 0))
                            if (!SetFlag(SecuritySystem.Status,
                                         s_CAalarm)) // juz wlaczony sygnal dzwiekowy
                                if ((SecuritySystem.SystemBrakeCATimer >
                                     SecuritySystem.EmergencyBrakeDelay) &&
                                    (SecuritySystem.EmergencyBrakeDelay >= 0))
                                    SetFlag(SecuritySystem.Status, s_CAebrake);

            // SHP
            if (TestFlag(SecuritySystem.SystemType, 2) &&
                TestFlag(SecuritySystem.Status, s_active)) // jeśli świeci albo miga
                SecuritySystem.SystemSoundSHPTimer += dt;
            if (TestFlag(SecuritySystem.SystemType, 2) &&
                TestFlag(SecuritySystem.Status, s_SHPalarm)) // jeśli buczy
                SecuritySystem.SystemBrakeSHPTimer += dt;
            if (TestFlag(SecuritySystem.SystemType, 2) && TestFlag(SecuritySystem.Status, s_active))
                if ((Vel > SecuritySystem.VelocityAllowed) && (SecuritySystem.VelocityAllowed >= 0))
                    SetFlag(SecuritySystem.Status, s_SHPebrake);
                else if (((SecuritySystem.SystemSoundSHPTimer > SecuritySystem.SoundSignalDelay) &&
                          (SecuritySystem.SoundSignalDelay >= 0)) ||
                         ((Vel > SecuritySystem.NextVelocityAllowed) &&
                          (SecuritySystem.NextVelocityAllowed >= 0)))
                    if (!SetFlag(SecuritySystem.Status,
                                 s_SHPalarm)) // juz wlaczony sygnal dzwiekowy}
                        if ((SecuritySystem.SystemBrakeSHPTimer >
                             SecuritySystem.EmergencyBrakeDelay) &&
                            (SecuritySystem.EmergencyBrakeDelay >= 0))
                            SetFlag(SecuritySystem.Status, s_SHPebrake);

        } // else SystemTimer:=0;

        // TEST CA
        if (TestFlag(SecuritySystem.Status, s_CAtest)) // jeśli świeci albo miga
            SecuritySystem.SystemBrakeCATestTimer += dt;
        if (TestFlag(SecuritySystem.SystemType, 1))
            if (TestFlag(SecuritySystem.Status, s_CAtest)) // juz wlaczony sygnal swietlny
                if ((SecuritySystem.SystemBrakeCATestTimer > SecuritySystem.EmergencyBrakeDelay) &&
                    (SecuritySystem.EmergencyBrakeDelay >= 0))
                    s_CAtestebrake = true;

        // wdrazanie hamowania naglego
        //        if TestFlag(Status,s_SHPebrake) or TestFlag(Status,s_CAebrake) or
        //        (s_CAtestebrake=true) then
        //         EmergencyBrakeFlag:=true;  //YB-HN
    }
    else if (!Battery)
    { // wyłączenie baterii deaktywuje sprzęt
		EmergencyBrakeSwitch(false);
        // SecuritySystem.Status = 0; //deaktywacja czuwaka
    }
}

// *************************************************************************************************
// Q: 20160710
// włączenie / wyłączenie baterii
// *************************************************************************************************
bool TMoverParameters::BatterySwitch(bool State)
{
    bool BS = false;
    // Ra: ukrotnienie załączania baterii jest jakąś fikcją...
    if (Battery != State)
    {
        Battery = State;
    }
    if (Battery == true)
        SendCtrlToNext("BatterySwitch", 1, CabNo);
    else
        SendCtrlToNext("BatterySwitch", 0, CabNo);
    BS = true;

    if ((Battery) && (ActiveCab != 0)) /*|| (TrainType==dt_EZT)*/
        SecuritySystem.Status = (SecuritySystem.Status | s_waiting); // aktywacja czuwaka
    else
        SecuritySystem.Status = 0; // wyłączenie czuwaka

    return BS;
}

// *************************************************************************************************
// Q: 20160710
// włączenie / wyłączenie hamulca elektro-pneumatycznego
// *************************************************************************************************
bool TMoverParameters::EpFuseSwitch(bool State)
{
    if (EpFuse != State)
    {
        EpFuse = State;
        return true;
    }
    else
        return false;
    // if (EpFuse == true) SendCtrlToNext("EpFuseSwitch", 1, CabNo)
    //  else SendCtrlToNext("EpFuseSwitch", 0, CabNo);
}

// *************************************************************************************************
// Q: 20160710
// kierunek do tyłu
// *************************************************************************************************
bool TMoverParameters::DirectionBackward(void)
{
    bool DB = false;
    if ((ActiveDir == 1) && (MainCtrlPos == 0) && (TrainType == dt_EZT))
        if (MinCurrentSwitch(false))
        {
            DB = true; //
            return DB; // exit;  TODO: czy dobrze przetlumaczone?
        }
    if ((MainCtrlPosNo > 0) && (ActiveDir > -1) && (MainCtrlPos == 0))
    {
        if (EngineType == WheelsDriven)
            CabNo--;
        //    else
        ActiveDir--;
        DirAbsolute = ActiveDir * CabNo;
        if (DirAbsolute != 0)
            if (Battery) // jeśli bateria jest już załączona
                BatterySwitch(true); // to w ten oto durny sposób aktywuje się CA/SHP
        DB = true;
        SendCtrlToNext("Direction", ActiveDir, CabNo);
    }
    else
        DB = false;
    return DB;
}

// *************************************************************************************************
// Q: 20160710
// załączenie przycisku przeciwpoślizgowego
// *************************************************************************************************
bool TMoverParameters::AntiSlippingButton(void)
{
    return (AntiSlippingBrake() || SandDoseOn());
}

// *************************************************************************************************
// Q: 20160713
// włączenie / wyłączenie obwodu głownego
// *************************************************************************************************
bool TMoverParameters::MainSwitch(bool State)
{
    bool MS;

    MS = false; // Ra: przeniesione z końca
    if ((Mains != State) && (MainCtrlPosNo > 0))
    {
        if ((State == false) ||
            ((ScndCtrlPos == 0) && ((ConvOvldFlag == false) || (TrainType == dt_EZT)) &&
             (LastSwitchingTime > CtrlDelay) && !TestFlag(DamageFlag, dtrain_out) &&
             !TestFlag(EngDmgFlag, 1)))
        {
            if (Mains) // jeśli był załączony
                SendCtrlToNext("MainSwitch", int(State),
                               CabNo); // wysłanie wyłączenia do pozostałych?
            Mains = State;
            if (Mains) // jeśli został załączony
                SendCtrlToNext("MainSwitch", int(State),
                               CabNo); // wyslanie po wlaczeniu albo przed wylaczeniem
            MS = true; // wartość zwrotna
            LastSwitchingTime = 0;
            if ((EngineType == DieselEngine) && Mains)
            {
                dizel_enginestart = State;
            }
			if (((TrainType == dt_EZT) && (!State)))
				ConvOvldFlag = true;
			// if (State=false) then //jeśli wyłączony
            // begin
            // SetFlag(SoundFlag,sound_relay); //hunter-091012: przeniesione do Train.cpp, zeby sie
            // nie zapetlal
            // if (SecuritySystem.Status<>12) then
            //  SecuritySystem.Status:=0; //deaktywacja czuwaka; Ra: a nie baterią?
            // end
            // else
            // if (SecuritySystem.Status<>12) then
            // SecuritySystem.Status:=s_waiting; //aktywacja czuwaka
        }
    }
    // else MainSwitch:=false;
    return MS;
}

// *************************************************************************************************
// Q: 20160713
// włączenie / wyłączenie przetwornicy
// *************************************************************************************************
bool TMoverParameters::ConverterSwitch(bool State)
{
    bool CS = false; // Ra: normalnie chyba false?
    if (ConverterAllow != State)
    {
        ConverterAllow = State;
        CS = true;
        if (CompressorPower == 2)
            CompressorAllow = ConverterAllow;
    }
    if (ConverterAllow == true)
        SendCtrlToNext("ConverterSwitch", 1, CabNo);
    else
        SendCtrlToNext("ConverterSwitch", 0, CabNo);

    return CS;
}

// *************************************************************************************************
// Q: 20160713
// włączenie / wyłączenie sprężarki
// *************************************************************************************************
bool TMoverParameters::CompressorSwitch(bool State)
{
    bool CS = false; // Ra: normalnie chyba tak?
    // if State=true then
    //  if ((CompressorPower=2) and (not ConverterAllow)) then
    //   State:=false; //yB: to juz niepotrzebne
    if ((CompressorAllow != State) && (CompressorPower < 2))
    {
        CompressorAllow = State;
        CS = true;
    }
    if (CompressorAllow == true)
        SendCtrlToNext("CompressorSwitch", 1, CabNo);
    else
        SendCtrlToNext("CompressorSwitch", 0, CabNo);

    return CS;
}

// *************************************************************************************************
// Q: 20160711
// zwiększenie nastawy hamulca
// *************************************************************************************************
bool TMoverParameters::IncBrakeLevelOld(void)
{
    bool IBLO = false;

    if ((BrakeCtrlPosNo > 0) /*and (LocalBrakePos=0)*/)
    {
        if (BrakeCtrlPos < BrakeCtrlPosNo)
        {
            BrakeCtrlPos++;
            //      BrakeCtrlPosR = BrakeCtrlPos;

            // youBy: wywalilem to, jak jest EP, to sa przenoszone sygnaly nt. co ma robic, a nie
            // poszczegolne pozycje;
            //       wystarczy spojrzec na Knorra i Oerlikona EP w EN57; mogly ze soba
            //       wspolapracowac
            //{
            //        if (BrakeSystem==ElectroPneumatic)
            //          if (BrakePressureActual.BrakeType==ElectroPneumatic)
            //           {
            //           BrakeStatus = ord(BrakeCtrlPos > 0);
            //             SendCtrlToNext("BrakeCtrl", BrakeCtrlPos, CabNo);
            //           }
            //          else SendCtrlToNext("BrakeCtrl", -2, CabNo);
            //        else
            //         if (!TestFlag(BrakeStatus,b_dmg))
            //          BrakeStatus = b_on;}

            // youBy: EP po nowemu

            IBLO = true;
            if ((BrakePressureActual.PipePressureVal < 0) &&
                (BrakePressureTable[BrakeCtrlPos - 1].PipePressureVal > 0))
                LimPipePress = PipePress;

			//ten kawałek jest bez sensu gdyż nic nie robił. Zakomntowałem. GF 20161124
            //if (BrakeSystem == ElectroPneumatic)
            //    if (BrakeSubsystem != ss_K)
            //    {
            //        if ((BrakeCtrlPos * BrakeCtrlPos) == 1)
            //        {
            //            //                SendCtrlToNext('Brake',BrakeCtrlPos,CabNo);
            //            //                SetFlag(BrakeStatus,b_epused);
            //        }
            //        else
            //        {
            //            //                SendCtrlToNext('Brake',0,CabNo);
            //            //                SetFlag(BrakeStatus,-b_epused);
            //        }
            //    }
        }
        else
        {
            IBLO = false;
            //        if (BrakeSystem == Pneumatic)
            //         EmergencyBrakeSwitch(true);
        }
    }
    else
        IBLO = false;

    return IBLO;
}

// *****************************************************************************
// Q: 20160711
// zmniejszenie nastawy hamulca
// *****************************************************************************
bool TMoverParameters::DecBrakeLevelOld(void)
{
    bool DBLO = false;

    if ((BrakeCtrlPosNo > 0) /*&& (LocalBrakePos == 0)*/)
    {
        if (BrakeCtrlPos > -1 - int(BrakeHandle == FV4a))
        {
            BrakeCtrlPos--;
            //        BrakeCtrlPosR:=BrakeCtrlPos;
            //if (EmergencyBrakeFlag)
            //{
            //    EmergencyBrakeFlag = false; //!!!
            //    SendCtrlToNext("Emergency_brake", 0, CabNo);
            //}

            // youBy: wywalilem to, jak jest EP, to sa przenoszone sygnaly nt. co ma robic, a nie
            // poszczegolne pozycje;
            //       wystarczy spojrzec na Knorra i Oerlikona EP w EN57; mogly ze soba
            //       wspolapracowac
            /*
                    if (BrakeSystem == ElectroPneumatic)
                      if (BrakePressureActual.BrakeType == ElectroPneumatic)
                       {
            //             BrakeStatus =ord(BrakeCtrlPos > 0);
                         SendCtrlToNext("BrakeCtrl",BrakeCtrlPos,CabNo);
                       }
                      else SendCtrlToNext('BrakeCtrl',-2,CabNo);
            //        else}
            //         if (not TestFlag(BrakeStatus,b_dmg) and (not
            TestFlag(BrakeStatus,b_release))) then
            //          BrakeStatus:=b_off;   {luzowanie jesli dziala oraz nie byl wlaczony
            odluzniacz
            */

            // youBy: EP po nowemu
            DBLO = true;
            //        if ((BrakePressureTable[BrakeCtrlPos].PipePressureVal<0.0) &&
            //        (BrakePressureTable[BrakeCtrlPos+1].PipePressureVal > 0))
            //          LimPipePress:=PipePress;

			// to nic nie robi. Zakomentowałem. GF 20161124
            //if (BrakeSystem == ElectroPneumatic)
            //    if (BrakeSubsystem != ss_K)
            //    {
            //        if ((BrakeCtrlPos * BrakeCtrlPos) == 1)
            //        {
            //            //                SendCtrlToNext("Brake", BrakeCtrlPos, CabNo);
            //            //                SetFlag(BrakeStatus, b_epused);
            //        }
            //        else
            //        {
            //            //                SendCtrlToNext("Brake", 0, CabNo);
            //            //                SetFlag(BrakeStatus, -b_epused);
            //        }
            //    }
            //    for b:=0 to 1 do  {poprawic to!}
            //     with Couplers[b] do
            //      if CouplingFlag and ctrain_controll=ctrain_controll then
            //       Connected^.BrakeCtrlPos:=BrakeCtrlPos;
            //
        }
        else
            DBLO = false;
    }
    else
        DBLO = false;

    return DBLO;
}

// *************************************************************************************************
// Q: 20160711
// zwiększenie nastawy hamulca pomocnicznego
// *************************************************************************************************
bool TMoverParameters::IncLocalBrakeLevel(int CtrlSpeed)
{
    bool IBL;
     if ((LocalBrakePos < LocalBrakePosNo) /*and (BrakeCtrlPos<1)*/)
    {
        while ((LocalBrakePos < LocalBrakePosNo) && (CtrlSpeed > 0))
        {
            LocalBrakePos++;
//            LocalBrakePosA = static_cast<double>(LocalBrakePos) / LocalBrakePosNo; // temporary hack until i figure out how this element is supposed to work
            CtrlSpeed--;
        }
        IBL = true;
    }
    else
        IBL = false;
    UnBrake = true;

    return IBL;
}

// *************************************************************************************************
// Q: 20160711
// zmniejszenie nastawy hamulca pomocniczego
// *************************************************************************************************
bool TMoverParameters::DecLocalBrakeLevel(int CtrlSpeed)
{
    bool DBL;
    if (LocalBrakePos > 0)
    {
        while ((CtrlSpeed > 0) && (LocalBrakePos > 0))
        {
            LocalBrakePos--;
//            LocalBrakePosA = static_cast<double>( LocalBrakePos ) / LocalBrakePosNo; // temporary hack until i figure out how this element is supposed to work
            CtrlSpeed--;
        }
        DBL = true;
    }
    else
        DBL = false;
    UnBrake = true;

    return DBL;
}

// *************************************************************************************************
// Q: 20160711
// ustawienie pozycji kranu pomocniczego na masymalną wartość
// *************************************************************************************************
bool TMoverParameters::IncLocalBrakeLevelFAST(void)
{
    bool ILBLF;
    if (LocalBrakePos < LocalBrakePosNo)
    {
        LocalBrakePos = LocalBrakePosNo;
//        LocalBrakePosA = static_cast<double>( LocalBrakePos ) / LocalBrakePosNo; // temporary hack until i figure out how this element is supposed to work
        ILBLF = true;
    }
    else
        ILBLF = false;
    UnBrake = true;
    return ILBLF;
}

// *************************************************************************************************
// Q: 20160711
// ustawienie pozycji hamulca pomocniczego na minimalną
// *************************************************************************************************
bool TMoverParameters::DecLocalBrakeLevelFAST(void)
{
    bool DLBLF;
    if (LocalBrakePos > 0)
    {
        LocalBrakePos = 0;
//        LocalBrakePosA = static_cast<double>( LocalBrakePos ) / LocalBrakePosNo; // temporary hack until i figure out how this element is supposed to work
        DLBLF = true;
    }
    else
        DLBLF = false;
    UnBrake = true;
    return DLBLF;
}

// *************************************************************************************************
// Q: 20160711
// zwiększenie nastawy hamulca ręcznego
// *************************************************************************************************
bool TMoverParameters::IncManualBrakeLevel(int CtrlSpeed)
{
    bool IMBL;
    if (ManualBrakePos < ManualBrakePosNo) /*and (BrakeCtrlPos<1)*/
    {
        while ((ManualBrakePos < ManualBrakePosNo) && (CtrlSpeed > 0))
        {
            ManualBrakePos++;
            CtrlSpeed--;
        }
        IMBL = true;
    }
    else
        IMBL = false;

    return IMBL;
}

// *************************************************************************************************
// Q: 20160711
// zmniejszenie nastawy hamulca ręcznego
// *************************************************************************************************
bool TMoverParameters::DecManualBrakeLevel(int CtrlSpeed)
{
    bool DMBL;
    if (ManualBrakePos > 0)
    {
        while ((CtrlSpeed > 0) && (ManualBrakePos > 0))
        {
            ManualBrakePos--;
            CtrlSpeed--;
        }
        DMBL = true;
    }
    else
        DMBL = false;
    return DMBL;
}

// *************************************************************************************************
// Q: 20160713
// reczne przelaczanie hamulca elektrodynamicznego
// *************************************************************************************************
bool TMoverParameters::DynamicBrakeSwitch(bool Switch)
{
    bool DBS;

    if ((DynamicBrakeType == dbrake_switch) && (MainCtrlPos == 0))
    {
        DynamicBrakeFlag = Switch;
        DBS = true;
        for (int b = 0; b < 2; b++)
            //  with Couplers[b] do
            if (TestFlag(Couplers[b].CouplingFlag, ctrain_controll))
                Couplers[b].Connected->DynamicBrakeFlag = Switch;
        // end;
        // if (DynamicBrakeType=dbrake_passive) and (TrainType=dt_ET42) then
        // begin
        // DynamicBrakeFlag:=false;
        // DynamicBrakeSwitch:=false;
    }
    else
        DBS = false;

    return DBS;
}

// *************************************************************************************************
// Q: 20160711
// włączenie / wyłączenie hamowania awaryjnego
// *************************************************************************************************
bool TMoverParameters::EmergencyBrakeSwitch(bool Switch)
{
    bool EBS;
    if ((BrakeSystem != Individual) && (BrakeCtrlPosNo > 0))
    {
        if ((!EmergencyBrakeFlag) && Switch)
        {
            EmergencyBrakeFlag = Switch;
            EBS = true;
        }
        else
        {
            if ((abs(V) < 0.1) &&
                (Switch == false)) // odblokowanie hamulca bezpieczenistwa tylko po zatrzymaniu
            {
                EmergencyBrakeFlag = Switch;
                EBS = true;
            }
            else
                EBS = false;
        }
    }
    else
        EBS = false; // nie ma hamulca bezpieczenstwa gdy nie ma hamulca zesp.

    return EBS;
}

// *************************************************************************************************
// Q: 20160710
// hamowanie przeciwpoślizgowe
// *************************************************************************************************
bool TMoverParameters::AntiSlippingBrake(void)
{
    bool ASB = false; // Ra: przeniesione z końca
    if (ASBType == 1)
    {
        ASB = true; // SPKS!!
        Hamulec->ASB(1);
        BrakeSlippingTimer = 0;
    }
    return ASB;
}

// *************************************************************************************************
// Q: 20160711
// włączenie / wyłączenie odluźniacza
// *************************************************************************************************
bool TMoverParameters::BrakeReleaser(int state)
{
    bool OK = true; //false tylko jeśli nie uda się wysłać, GF 20161124
    Hamulec->Releaser(state);
    if (CabNo != 0) // rekurencyjne wysłanie do następnego
        OK = SendCtrlToNext("BrakeReleaser", state, CabNo);
    return OK;
}

// *************************************************************************************************
// Q: 20160711
// włączenie / wyłączenie hamulca elektro-pneumatycznego
// *************************************************************************************************
bool TMoverParameters::SwitchEPBrake(int state)
{
    bool OK;
    double temp;

    OK = false;
    if ((BrakeHandle == St113) && (ActiveCab != 0))
    {
        if (state > 0)
            temp = Handle->GetCP(); // TODO: przetlumaczyc
        else
            temp = 0;
        Hamulec->SetEPS(temp);
        SendCtrlToNext("Brake", temp, CabNo);
    }
    //  OK:=SetFlag(BrakeStatus,((2*State-1)*b_epused));
    //  SendCtrlToNext('Brake',(state*(2*BrakeCtrlPos-1)),CabNo);
    return OK;
}

// *************************************************************************************************
// Q: 20160711
// zwiększenie ciśnienia hamowania
// *************************************************************************************************
bool TMoverParameters::IncBrakePress(double &brake, double PressLimit, double dp)
{
    bool IBP;
    //  if (DynamicBrakeType<>dbrake_switch) and (DynamicBrakeType<>dbrake_none) and
    //  ((BrakePress>2.0) or (PipePress<3.7{(LowPipePress+0.5)})) then
    if ((DynamicBrakeType != dbrake_switch) && (DynamicBrakeType != dbrake_none) &&
        (BrakePress > 2.0) &&
        (TrainType != dt_EZT)) // yB radzi nie sprawdzać ciśnienia w przewodzie
    // hunter-301211: dla EN57 silnikow nie odlaczamy
    {
        DynamicBrakeFlag = true; // uruchamianie hamulca ED albo odlaczanie silnikow
        if ((DynamicBrakeType == dbrake_automatic) &&
            (abs(Im) > 60)) // nie napelniaj wiecej, jak na EP09
            dp = 0.0;
    }
    if (brake + dp < PressLimit)
    {
        brake = brake + dp;
        IBP = true;
    }
    else
    {
        IBP = false;
        brake = PressLimit;
    }
    return IBP;
}

// *************************************************************************************************
// Q: 20160711
// zmniejszenie ciśnienia hamowania
// *************************************************************************************************
bool TMoverParameters::DecBrakePress(double &brake, double PressLimit, double dp)
{
    bool DBP;

    if (brake - dp > PressLimit)
    {
        brake = brake - dp;
        DBP = true;
    }
    else
    {
        DBP = false;
        brake = PressLimit;
    }
    //  if ((DynamicBrakeType != dbrake_switch) && ((BrakePress < 0.1) && (PipePress > 0.45
    //  /*(LowPipePress+0.06)*/ )))
    if ((DynamicBrakeType != dbrake_switch) &&
        (BrakePress < 0.1)) // yB radzi nie sprawdzać ciśnienia w przewodzie
        DynamicBrakeFlag = false; // wylaczanie hamulca ED i/albo zalaczanie silnikow

    return DBP;
}

// *************************************************************************************************
// Q: 20160711
// przełączenie nastawy hamulca O/P/T
// *************************************************************************************************
bool TMoverParameters::BrakeDelaySwitch(int BDS)
{
    bool rBDS;
    //  if (BrakeCtrlPosNo > 0)
	if (BrakeHandle == MHZ_EN57)
	{
		if ((BDS != BrakeOpModeFlag) && ((BDS & BrakeOpModes) > 0))
		{
			BrakeOpModeFlag = BDS;
			rBDS = true;
		}
		else
			rBDS = false;
	}
	else if (Hamulec->SetBDF(BDS))
    {
        BrakeDelayFlag = BDS;
        rBDS = true;
        BrakeStatus &= 191;
        // kopowanie nastawy hamulca do kolejnego czlonu - do przemyślenia
        if (CabNo != 0)
            SendCtrlToNext("BrakeDelay", BrakeDelayFlag, CabNo);
    }
    else
        rBDS = false;
    return rBDS;
}

// *************************************************************************************************
// Q: 20160712
// zwiększenie przełożenia hamulca
// *************************************************************************************************
bool TMoverParameters::IncBrakeMult(void)
{
    bool IBM;

    if ((LoadFlag > 0) && (MBPM < 2) && (LoadFlag < 3))
    {
        if ((MaxBrakePress[2] > 0) && (LoadFlag == 1))
            LoadFlag = 2;
        else
            LoadFlag = 3;
        IBM = true;
        if (BrakeCylMult[2] > 0)
            BrakeCylMult[0] = BrakeCylMult[2];
    }
    else
        IBM = false;

    return IBM;
}

// *************************************************************************************************
// Q: 20160712
// zmniejszenie przełożenia hamulca
// *************************************************************************************************
bool TMoverParameters::DecBrakeMult(void)
{
    bool DBM;
    if ((LoadFlag > 1) && (MBPM < 2))
    {
        if ((MaxBrakePress[2] > 0) && (LoadFlag == 3))
            LoadFlag = 2;
        else
            LoadFlag = 1;
        DBM = true;
        if (BrakeCylMult[1] > 0)
            BrakeCylMult[0] = BrakeCylMult[1];
    }
    else
        DBM = false;

    return DBM;
}

// *************************************************************************************************
// Q: 20160712
// zaktualizowanie ciśnienia w hamulcach
// *************************************************************************************************
void TMoverParameters::UpdateBrakePressure(double dt)
{
    //const double LBDelay = 5.0; // stala czasowa hamulca
    //double Rate, Speed, dp, sm;

    dpLocalValve = 0;
    dpBrake = 0;

    BrakePress = Hamulec->GetBCP();
    //  BrakePress:=(Hamulec as TEst4).ImplsRes.pa;
    Volume = Hamulec->GetBRP();
}

// *************************************************************************************************
// Q: 20160712
// Obliczanie pracy sprężarki
// *************************************************************************************************
void TMoverParameters::CompressorCheck(double dt)
{
    // if (CompressorSpeed>0.0) then //ten warunek został sprawdzony przy wywołaniu funkcji
    if (VeselVolume > 0)
    {
        if (MaxCompressor - MinCompressor < 0.0001)
        {
            //     if (Mains && (MainCtrlPos > 1))
            if (CompressorAllow && Mains && (MainCtrlPos > 0))
            {
                if (Compressor < MaxCompressor)
                    if ((EngineType == DieselElectric) && (CompressorPower > 0))
                        CompressedVolume += dt * CompressorSpeed *
                                            (2.0 * MaxCompressor - Compressor) / MaxCompressor *
                                            (DElist[MainCtrlPos].RPM / DElist[MainCtrlPosNo].RPM);
                    else
                    {
                        CompressedVolume +=
                            dt * CompressorSpeed * (2.0 * MaxCompressor - Compressor) / MaxCompressor;
                        TotalCurrent += 0.0015 
							* Voltage; // tymczasowo tylko obciążenie sprężarki, tak z 5A na sprężarkę
                    }
                else
                {
                    CompressedVolume = CompressedVolume * 0.8;
                    SetFlag(SoundFlag, sound_relay | sound_loud);
                    // SetFlag(SoundFlag, sound_loud);
                }
            }
        }
        else
        {
            if (CompressorFlag) // jeśli sprężarka załączona
            { // sprawdzić możliwe warunki wyłączenia sprężarki
                if (CompressorPower == 5) // jeśli zasilanie z sąsiedniego członu
                { // zasilanie sprężarki w członie ra z członu silnikowego (sprzęg 1)
                    if (Couplers[1].Connected != NULL)
                        CompressorFlag =
                            (Couplers[1].Connected->CompressorAllow &&
                             Couplers[1].Connected->ConverterFlag && Couplers[1].Connected->Mains);
                    else
                        CompressorFlag = false; // bez tamtego członu nie zadziała
                }
                else if (CompressorPower == 4) // jeśli zasilanie z poprzedniego członu
                { // zasilanie sprężarki w członie ra z członu silnikowego (sprzęg 1)
                    if (Couplers[0].Connected != NULL)
                        CompressorFlag =
                            (Couplers[0].Connected->CompressorAllow &&
                             Couplers[0].Connected->ConverterFlag && Couplers[0].Connected->Mains);
                    else
                        CompressorFlag = false; // bez tamtego członu nie zadziała
                }
                else
                    CompressorFlag = (CompressorAllow) &&
                                      ((ConverterFlag) || (CompressorPower == 0)) && (Mains);
                if (Compressor >
                    MaxCompressor) // wyłącznik ciśnieniowy jest niezależny od sposobu zasilania
                    CompressorFlag = false;
            }
            else // jeśli nie załączona
                if ((Compressor < MinCompressor) &&
                    (LastSwitchingTime > CtrlDelay)) // jeśli nie załączona, a ciśnienie za małe
            { // załączenie przy małym ciśnieniu
                if (CompressorPower == 5) // jeśli zasilanie z następnego członu
                { // zasilanie sprężarki w członie ra z członu silnikowego (sprzęg 1)
                    if (Couplers[1].Connected != NULL)
                        CompressorFlag =
                            (Couplers[1].Connected->CompressorAllow &&
                             Couplers[1].Connected->ConverterFlag && Couplers[1].Connected->Mains);
                    else
                        CompressorFlag = false; // bez tamtego członu nie zadziała
                }
                else if (CompressorPower == 4) // jeśli zasilanie z poprzedniego członu
                { // zasilanie sprężarki w członie ra z członu silnikowego (sprzęg 1)
                    if (Couplers[0].Connected != NULL)
                        CompressorFlag =
                            (Couplers[0].Connected->CompressorAllow &&
                             Couplers[0].Connected->ConverterFlag && Couplers[0].Connected->Mains);
                    else
                        CompressorFlag = false; // bez tamtego członu nie zadziała
                }
                else
                    CompressorFlag = (CompressorAllow) &&
                                      ((ConverterFlag) || (CompressorPower == 0)) && (Mains);
                if (CompressorFlag) // jeśli została załączona
                    LastSwitchingTime = 0; // to trzeba ograniczyć ponowne włączenie
            }
            //          for b:=0 to 1 do //z Megapacka
            //    with Couplers[b] do
            //     if TestFlag(CouplingFlag,ctrain_scndpneumatic) then
            //      Connected.CompressorFlag:=CompressorFlag;
            if (CompressorFlag)
                if ((EngineType == DieselElectric) && (CompressorPower > 0))
                    CompressedVolume += dt * CompressorSpeed * (2.0 * MaxCompressor - Compressor) /
                                        MaxCompressor *
                                        (DElist[MainCtrlPos].RPM / DElist[MainCtrlPosNo].RPM);
                else
                {
                    CompressedVolume +=
                        dt * CompressorSpeed * (2.0 * MaxCompressor - Compressor) / MaxCompressor;
                    if ((CompressorPower == 5) && (Couplers[1].Connected != NULL))
                        Couplers[1].Connected->TotalCurrent +=
                            0.0015 * Couplers[1].Connected->Voltage; // tymczasowo tylko obciążenie
                                                                     // sprężarki, tak z 5A na
                                                                     // sprężarkę
                    else if ((CompressorPower == 4) && (Couplers[0].Connected != NULL))
                        Couplers[0].Connected->TotalCurrent +=
                            0.0015 * Couplers[0].Connected->Voltage; // tymczasowo tylko obciążenie
                                                                     // sprężarki, tak z 5A na
                                                                     // sprężarkę
                    else
                        TotalCurrent += 0.0015 *
                            Voltage; // tymczasowo tylko obciążenie sprężarki, tak z 5A na sprężarkę
                }
        }
    }
}

// *************************************************************************************************
// Q: 20160712
// aktualizacja ciśnienia w przewodzie głównym
// *************************************************************************************************
void TMoverParameters::UpdatePipePressure(double dt)
{
    const double LBDelay = 100;
    const double kL = 0.5;
    //double dV;
    //TMoverParameters *c; // T_MoverParameters
    double temp;
    //int b;

    PipePress = Pipe->P();
    //  PPP:=PipePress;

    dpMainValve = 0;

    if ((BrakeCtrlPosNo > 1) /*&& (ActiveCab != 0)*/)
    // with BrakePressureTable[BrakeCtrlPos] do
    {
        if ((EngineType != ElectricInductionMotor))
            dpLocalValve =
                LocHandle->GetPF(std::max(static_cast<double>(LocalBrakePos) / LocalBrakePosNo, LocalBrakePosA),
                                 Hamulec->GetBCP(), ScndPipePress, dt, 0);
        else
            dpLocalValve =
                LocHandle->GetPF(LocalBrakePosA, Hamulec->GetBCP(), ScndPipePress, dt, 0);
        if ((BrakeHandle == FV4a) &&
            ((PipePress < 2.75) && ((Hamulec->GetStatus() & b_rls) == 0)) &&
            (BrakeSubsystem == ss_LSt) && (TrainType != dt_EZT))
            temp = PipePress + 0.00001;
        else
            temp = ScndPipePress;
        Handle->SetReductor(BrakeCtrlPos2);

		if ((BrakeOpModeFlag != bom_PS))
			if ((BrakeOpModeFlag < bom_EP) || ((Handle->GetPos(bh_EB) - 0.5) < BrakeCtrlPosR) ||
				(BrakeHandle != MHZ_EN57))
				dpMainValve = Handle->GetPF(BrakeCtrlPosR, PipePress, temp, dt, EqvtPipePress);
			else
				dpMainValve = Handle->GetPF(0, PipePress, temp, dt, EqvtPipePress);
		
		if (dpMainValve < 0) // && (PipePressureVal > 0.01)           //50
            if (Compressor > ScndPipePress)
            {
                CompressedVolume = CompressedVolume + dpMainValve / 1500.0;
                Pipe2->Flow(dpMainValve / 3.0);
            }
            else
                Pipe2->Flow(dpMainValve);
    }

    //      if(EmergencyBrakeFlag)and(BrakeCtrlPosNo=0)then         //ulepszony hamulec bezp.
    if ((EmergencyBrakeFlag) || (TestFlag(SecuritySystem.Status, s_SHPebrake)) ||
        (TestFlag(SecuritySystem.Status, s_CAebrake)) ||
        (s_CAtestebrake == true) ||
		(TestFlag(EngDmgFlag, 32)) /* or (not Battery)*/) // ulepszony hamulec bezp.
        dpMainValve = dpMainValve + PF(0, PipePress, 0.15) * dt;
    // 0.2*Spg
    Pipe->Flow(-dpMainValve);
    Pipe->Flow(-(PipePress)*0.001 * dt);
    //      if Heating then
    //        Pipe.Flow(PF(PipePress, 0, d2A(7)) * dt);
    //      if ConverterFlag then
    //        Pipe.Flow(PF(PipePress, 0, d2A(12)) * dt);
    dpMainValve = dpMainValve / (Dim.L * Spg * 20);

    CntrlPipePress = Hamulec->GetVRP(); // ciśnienie komory wstępnej rozdzielacza

    //      if (Hamulec is typeid(TWest)) return 0;

    switch (BrakeValve) {

        case K:
        case W: {

            if( BrakeLocHandle != NoHandle ) {
                LocBrakePress = LocHandle->GetCP();

                //(Hamulec as TWest).SetLBP(LocBrakePress);
                Hamulec->SetLBP( LocBrakePress );
            }
            if( MBPM < 2 )
                //(Hamulec as TWest).PLC(MaxBrakePress[LoadFlag])
                Hamulec->PLC( MaxBrakePress[ LoadFlag ] );
            else
                //(Hamulec as TWest).PLC(TotalMass);
                Hamulec->PLC( TotalMass );
            break;
        }

        case LSt:
        case EStED: {

            LocBrakePress = LocHandle->GetCP();
            for( int b = 0; b < 2; b++ )
                if( ( ( TrainType & ( dt_ET41 | dt_ET42 ) ) != 0 ) &&
                    ( Couplers[ b ].Connected != NULL ) ) // nie podoba mi się to rozwiązanie, chyba trzeba
                    // dodać jakiś wpis do fizyki na to
                    if( ( ( Couplers[ b ].Connected->TrainType & ( dt_ET41 | dt_ET42 ) ) != 0 ) &&
                        ( ( Couplers[ b ].CouplingFlag & 36 ) == 36 ) )
                        LocBrakePress = Max0R( Couplers[ b ].Connected->LocHandle->GetCP(), LocBrakePress );

            //if ((DynamicBrakeFlag) && (EngineType == ElectricInductionMotor))
            //{
            //    //if (Vel > 10)
            //    //    LocBrakePress = 0;
            //    //else if (Vel > 5)
            //    //    LocBrakePress = (10 - Vel) / 5 * LocBrakePress;
            //}

            //(Hamulec as TLSt).SetLBP(LocBrakePress);
            Hamulec->SetLBP( LocBrakePress );
            if( ( BrakeValve == EStED ) )
                if( MBPM < 2 )
                    Hamulec->PLC( MaxBrakePress[ LoadFlag ] );
                else
                    Hamulec->PLC( TotalMass );
            break;
        }

        case CV1_L_TR:
        {
            LocBrakePress = LocHandle->GetCP();
            //(Hamulec as TCV1L_TR).SetLBP(LocBrakePress);
            Hamulec->SetLBP( LocBrakePress );
            break;
        }

        case EP2:
        {
            Hamulec->PLC( TotalMass );
            break;
        }
        case ESt3AL2:
        case NESt3:
        case ESt4:
        case ESt3:
        {
            if( MBPM < 2 )
                //(Hamulec as TNESt3).PLC(MaxBrakePress[LoadFlag])
                Hamulec->PLC( MaxBrakePress[ LoadFlag ] );
            else
                //(Hamulec as TNESt3).PLC(TotalMass);
                Hamulec->PLC( TotalMass );
            LocBrakePress = LocHandle->GetCP();
            //(Hamulec as TNESt3).SetLBP(LocBrakePress);
            Hamulec->SetLBP( LocBrakePress );
            break;
        }
        case KE:
        {
            LocBrakePress = LocHandle->GetCP();
            //(Hamulec as TKE).SetLBP(LocBrakePress);
            Hamulec->SetLBP( LocBrakePress );
            if( MBPM < 2 )
                //(Hamulec as TKE).PLC(MaxBrakePress[LoadFlag])
                Hamulec->PLC( MaxBrakePress[ LoadFlag ] );
            else
                //(Hamulec as TKE).PLC(TotalMass);
                Hamulec->PLC( TotalMass );
            break;
        }
        default:
        {
            // unsupported brake valve type, we should never land here
//            ErrorLog( "Unsupported brake valve type (" + std::to_string( BrakeValve ) + ") in " + TypeName );
//            ::PostQuitMessage( 0 );
            break;
        }
    } // switch

    if ((BrakeHandle == FVel6) && (ActiveCab != 0))
    {
        if ((Battery) && (ActiveDir != 0) &&
            (EpFuse)) // tu powinien byc jeszcze bezpiecznik EP i baterie -
            // temp = (Handle as TFVel6).GetCP
            temp = Handle->GetCP();
        else
            temp = 0;
        Hamulec->SetEPS(temp);
        SendCtrlToNext("Brake", temp,
                       CabNo); // Ra 2014-11: na tym się wysypuje, ale nie wiem, w jakich warunkach
    }

    Pipe->Act();
    PipePress = Pipe->P();
    if ((BrakeStatus & 128) == 128) // jesli hamulec wyłączony
        temp = 0; // odetnij
    else
        temp = 1; // połącz
    Pipe->Flow(temp * Hamulec->GetPF(temp * PipePress, dt, Vel) + GetDVc(dt));

    if (ASBType == 128)
        Hamulec->ASB(int(SlippingWheels));

    dpPipe = 0;

    // yB: jednokrokowe liczenie tego wszystkiego
    Pipe->Act();
    PipePress = Pipe->P();

    dpMainValve = dpMainValve / (100 * dt); // normalizacja po czasie do syczenia;

    if (PipePress < -1)
    {
        PipePress = -1;
        Pipe->CreatePress(-1);
        Pipe->Act();
    }

    if (CompressedVolume < 0)
        CompressedVolume = 0;
}

// *************************************************************************************************
// Q: 20160713
// Aktualizacja ciśnienia w przewodzie zasilającym
// *************************************************************************************************
void TMoverParameters::UpdateScndPipePressure(double dt)
{
    const double Spz = 0.5067;
    TMoverParameters *c;
    double dv1, dv2, dV;

    dv1 = 0;
    dv2 = 0;

    // sprzeg 1
    if (Couplers[0].Connected != NULL)
        if (TestFlag(Couplers[0].CouplingFlag, ctrain_scndpneumatic))
        {
            c = Couplers[0].Connected; // skrot
            dv1 = 0.5 * dt * PF(ScndPipePress, c->ScndPipePress, Spz * 0.75);
            if (dv1 * dv1 > 0.00000000000001)
                c->Physic_ReActivation();
            c->Pipe2->Flow(-dv1);
        }
    // sprzeg 2
    if (Couplers[1].Connected != NULL)
        if (TestFlag(Couplers[1].CouplingFlag, ctrain_scndpneumatic))
        {
            c = Couplers[1].Connected; // skrot
            dv2 = 0.5 * dt * PF(ScndPipePress, c->ScndPipePress, Spz * 0.75);
            if (dv2 * dv2 > 0.00000000000001)
                c->Physic_ReActivation();
            c->Pipe2->Flow(-dv2);
        }
    if ((Couplers[1].Connected != NULL) && (Couplers[0].Connected != NULL))
        if ((TestFlag(Couplers[0].CouplingFlag, ctrain_scndpneumatic)) &&
            (TestFlag(Couplers[1].CouplingFlag, ctrain_scndpneumatic)))
        {
            dV = 0.00025 * dt * PF(Couplers[0].Connected->ScndPipePress,
                                   Couplers[1].Connected->ScndPipePress, Spz * 0.25);
            Couplers[0].Connected->Pipe2->Flow(+dV);
            Couplers[1].Connected->Pipe2->Flow(-dV);
        }

    Pipe2->Flow(Hamulec->GetHPFlow(ScndPipePress, dt));

    if (((Compressor > ScndPipePress) && (CompressorSpeed > 0.0001)) || (TrainType == dt_EZT))
    {
        dV = PF(Compressor, ScndPipePress, Spz) * dt;
        CompressedVolume += dV / 1000.0;
        Pipe2->Flow(-dV);
    }

    Pipe2->Flow(dv1 + dv2);
    Pipe2->Act();
    ScndPipePress = Pipe2->P();

    if (ScndPipePress < -1)
    {
        ScndPipePress = -1;
        Pipe2->CreatePress(-1);
        Pipe2->Act();
    }
}

// *************************************************************************************************
// Q: 20160715
// oblicza i zwraca przepływ powietrza pomiędzy pojazdami
// *************************************************************************************************
double TMoverParameters::GetDVc(double dt)
{
    // T_MoverParameters *c;
    TMoverParameters *c;
    double dv1, dv2;// , dV;

    dv1 = 0;
    dv2 = 0;
    // sprzeg 1
    if (Couplers[0].Connected != NULL)
        if (TestFlag(Couplers[0].CouplingFlag, ctrain_pneumatic))
        { //*0.85
            c = Couplers[0].Connected; // skrot           //0.08           //e/D * L/D = e/D^2 * L
            dv1 = 0.5 * dt * PF(PipePress, c->PipePress, (Spg) / (1.0 + 0.015 / Spg * Dim.L));
            if (dv1 * dv1 > 0.00000000000001)
                c->Physic_ReActivation();
            c->Pipe->Flow(-dv1);
        }
    // sprzeg 2
    if (Couplers[1].Connected != NULL)
        if (TestFlag(Couplers[1].CouplingFlag, ctrain_pneumatic))
        {
            c = Couplers[1].Connected; // skrot
            dv2 = 0.5 * dt * PF(PipePress, c->PipePress, (Spg) / (1.0 + 0.015 / Spg * Dim.L));
            if (dv2 * dv2 > 0.00000000000001)
                c->Physic_ReActivation();
            c->Pipe->Flow(-dv2);
        }
    //if ((Couplers[1].Connected != NULL) && (Couplers[0].Connected != NULL))
    //    if ((TestFlag(Couplers[0].CouplingFlag, ctrain_pneumatic)) &&
    //        (TestFlag(Couplers[1].CouplingFlag, ctrain_pneumatic)))
    //    {
    //        dV = 0.05 * dt * PF(Couplers[0].Connected->PipePress, Couplers[1].Connected->PipePress,
    //                            (Spg * 0.85) / (1 + 0.03 * Dim.L)) *
    //             0; // ktoś mi powie jaki jest sens tego bloku jeśli przepływ mnożony przez zero?
    //        Couplers[0].Connected->Pipe->Flow(+dV);
    //        Couplers[1].Connected->Pipe->Flow(-dV);
    //    }
    // suma
    return dv2 + dv1;
}

// *************************************************************************************************
// Q: 20160713
// Obliczenie stałych potrzebnych do dalszych obliczeń
// *************************************************************************************************
void TMoverParameters::ComputeConstans(void)
{
    double BearingF, RollF, HideModifier;
    double Curvature; // Ra 2014-07: odwrotność promienia

    TotalCurrent = 0; // Ra 2014-04: tu zerowanie, aby EZT mogło pobierać prąd innemu członowi
    TotalMass = ComputeMass();
    TotalMassxg = TotalMass * g; // TotalMass*g
    BearingF = 2.0 * (DamageFlag && dtrain_bearing);

    HideModifier = 0; // int(Couplers[0].CouplingFlag>0)+int(Couplers[1].CouplingFlag>0);

    if (BearingType == 0)
        RollF = 0.05; // slizgowe
    else
        RollF = 0.015; // toczne
    RollF += BearingF / 200.0;

    //    if (NPoweredAxles > 0)
    //     RollF = RollF * 1.5;    //dodatkowe lozyska silnikow

    if (NPoweredAxles > 0) // drobna optymalka
    {
        RollF += 0.025;
        //       if (Ft * Ft < 1)
        //         HideModifier = HideModifier - 3;
    }
    Ff = TotalMassxg * (BearingF + RollF * V * V / 10.0) / 1000.0;
    // dorobic liczenie temperatury lozyska!
    FrictConst1 = ((TotalMassxg * RollF) / 10000.0) + (Cx * Dim.W * Dim.H);
    Curvature = abs(RunningShape.R); // zero oznacza nieskończony promień
    if (Curvature > 0.0)
        Curvature = 1.0 / Curvature;
    // opór składu na łuku (youBy): +(500*TrackW/R)*TotalMassxg*0.001 do FrictConst2s/d
    FrictConst2s = (TotalMassxg * ((500.0 * TrackW * Curvature) + 2.5 - HideModifier +
                                   2 * BearingF / dtrain_bearing)) *
                   0.001;
    FrictConst2d = (TotalMassxg * ((500.0 * TrackW * Curvature) + 2.0 - HideModifier +
                                   BearingF / dtrain_bearing)) *
                   0.001;
}

// *************************************************************************************************
// Q: 20160713
// Oblicza masę ładunku
// *************************************************************************************************
double TMoverParameters::ComputeMass(void)
{
    double M;
	LoadType = ToLower(LoadType); // po co zakładać jak można mieć na pewno
    if (Load > 0)
    { // zakładamy, że ładunek jest pisany małymi literami
        if (ToLower(LoadQuantity) == "tonns")
            M = Load * 1000;
        else if (LoadType == "passengers")
            M = Load * 80;
        else if (LoadType == "luggage")
            M = Load * 100;
        else if (LoadType == "cars")
            M = Load * 1200; // 800 kilo to miał maluch
        else if (LoadType == "containers")
            M = Load * 8000;
        else if (LoadType == "transformers")
            M = Load * 50000;
        else
            M = Load * 1000;
    }
    else
        M = 0;
    // Ra: na razie tak, ale nie wszędzie masy wirujące się wliczają
    return Mass + M + Mred;
}

// *************************************************************************************************
// Q: 20160713
// Obliczanie wypadkowej siły z wszystkich działających sił
// *************************************************************************************************
void TMoverParameters::ComputeTotalForce(double dt, double dt1, bool FullVer)
{
    int b;

    if (PhysicActivation)
    {
        //  EventFlag:=false;                                             {jesli cos sie zlego
        //  wydarzy to ustawiane na true}
        //  SoundFlag:=0;                                                 {jesli ma byc jakis dzwiek
        //  to zapalany jet odpowiedni bit}
        // to powinno byc zerowane na zewnatrz

        // juz zoptymalizowane:
        FStand = FrictionForce(RunningShape.R, RunningTrack.DamageFlag); // siła oporów ruchu
        Vel = abs(V) * 3.6; // prędkość w km/h
        nrot = v2n(); // przeliczenie prędkości liniowej na obrotową

        if( (true == TestFlag(BrakeMethod, bp_MHS))
         && (PipePress < 3.0)
         && (Vel > 45)
         && (true == TestFlag(BrakeDelayFlag, bdelay_M))) // ustawione na sztywno na 3 bar
            FStand += TrackBrakeForce; // doliczenie hamowania hamulcem szynowym
        // w charakterystykach jest wartość siły hamowania zamiast nacisku
        //    if(FullVer=true) then
        //    ABu: to dla optymalizacji, bo chyba te rzeczy wystarczy sprawdzac 1 raz na
        //    klatke?
        LastSwitchingTime += dt1;
        if (EngineType == ElectricSeriesMotor)
            LastRelayTime += dt1;
        if (Mains && /*(abs(CabNo) < 2) &&*/ (EngineType ==
                                              ElectricSeriesMotor)) // potem ulepszyc! pantogtrafy!
        { // Ra 2014-03: uwzględnienie kierunku jazdy w napięciu na silnikach, a powinien być
          // zdefiniowany nawrotnik
            if (CabNo == 0)
                Voltage = RunningTraction.TractionVoltage * ActiveDir;
            else
                Voltage = RunningTraction.TractionVoltage * DirAbsolute; // ActiveDir*CabNo;
        } // bo nie dzialalo
		else if ((EngineType == ElectricInductionMotor) ||
			(((Couplers[0].CouplingFlag & ctrain_power) == ctrain_power) ||
				((Couplers[1].CouplingFlag & ctrain_power) ==
					ctrain_power))) // potem ulepszyc! pantogtrafy!
			Voltage =
			Max0R(Max0R(RunningTraction.TractionVoltage, HVCouplers[0][1]), HVCouplers[1][1]);
		else
            Voltage = 0;
        //if (Mains && /*(abs(CabNo) < 2) &&*/ (
        //                 EngineType == ElectricInductionMotor)) // potem ulepszyc! pantogtrafy!
        //    Voltage = RunningTraction.TractionVoltage;

        if (Power > 0)
            FTrain = TractionForce(dt);
        else
            FTrain = 0;
        Fb = BrakeForce(RunningTrack);
        if( std::max( std::abs( FTrain ), Fb ) > TotalMassxg * Adhesive( RunningTrack.friction ) ) // poslizg
        {
            SlippingWheels = true;
        }
        if( true == SlippingWheels ) {
            //     TrainForce:=TrainForce-Fb;
            nrot = ComputeRotatingWheel((FTrain - Fb * Sign(V) - FStand) / NAxles -
                                            Sign(nrot * M_PI * WheelDiameter - V) *
                                                Adhesive(RunningTrack.friction) * TotalMass,
                                        dt, nrot);
            FTrain = Sign(FTrain) * TotalMassxg * Adhesive(RunningTrack.friction);
            Fb = std::min(Fb, TotalMassxg * Adhesive(RunningTrack.friction));
        }
        //  else SlippingWheels:=false;
        //  FStand:=0;
        for (b = 0; b < 2; ++b)
            if (Couplers[b].Connected != NULL) /*and (Couplers[b].CouplerType<>Bare) and
                                                  (Couplers[b].CouplerType<>Articulated)*/
            { // doliczenie sił z innych pojazdów
                Couplers[b].CForce = CouplerForce(b, dt);
                FTrain += Couplers[b].CForce;
            }
            else
                Couplers[b].CForce = 0;
        // FStand:=Fb+FrictionForce(RunningShape.R,RunningTrack.DamageFlag);
        FStand += Fb;
        FTrain +=
            TotalMassxg * RunningShape.dHtrack; // doliczenie składowej stycznej grawitacji
        //!niejawne przypisanie zmiennej!
        FTotal = FTrain - Sign(V) * FStand;
    }

    // McZapkie-031103: sprawdzanie czy warto liczyc fizyke i inne updaty
    // ABu 300105: cos tu mieszalem , dziala teraz troche lepiej, wiec zostawiam
    //     zakomentowalem PhysicActivationFlag bo cos nie dzialalo i fizyka byla liczona zawsze.
    // if (PhysicActivationFlag)
    //{
    if ((CabNo == 0) && (Vel < 0.0001) && (abs(AccS) < 0.0001) && (TrainType != dt_EZT))
    {
        if (!PhysicActivation)
        {
            if (Couplers[0].Connected != NULL)
                if ((Couplers[0].Connected->Vel > 0.0001) ||
                    (abs(Couplers[0].Connected->AccS) > 0.0001))
                    Physic_ReActivation();
            if (Couplers[1].Connected != NULL)
                if ((Couplers[1].Connected->Vel > 0.0001) ||
                    (abs(Couplers[1].Connected->AccS) > 0.0001))
                    Physic_ReActivation();
        }
        if (LastSwitchingTime > 5) // 10+Random(100) then
            PhysicActivation = false; // zeby nie brac pod uwage braku V po uruchomieniu programu
    }
    else
        PhysicActivation = true;
    //};
}

// *************************************************************************************************
// Q: 20160713
// oblicza siłę na styku koła i szyny
// *************************************************************************************************
double TMoverParameters::BrakeForce(const TTrackParam &Track)
{
    double K, Fb, NBrakeAxles, sm = 0;
    // const OerlikonForceFactor=1.5;

    if (NPoweredAxles > 0)
        NBrakeAxles = NPoweredAxles;
    else
        NBrakeAxles = NAxles;
    switch (LocalBrake)
    {
    case NoBrake:
        K = 0;
		break;
    case ManualBrake:
        K = MaxBrakeForce * ManualBrakeRatio();
		break;
    case HydraulicBrake:
        K = MaxBrakeForce * LocalBrakeRatio();
		break;
    case PneumaticBrake:
        if (Compressor < MaxBrakePress[3])
            K = MaxBrakeForce * LocalBrakeRatio() / 2.0;
        else
            K = 0;
    }

    if (MBrake == true)
    {
        K = MaxBrakeForce * ManualBrakeRatio();
    }

    // 0.03

    u = ((BrakePress * P2FTrans) - BrakeCylSpring) * BrakeCylMult[0] - BrakeSlckAdj;
    if (u * BrakeRigEff > Ntotal) // histereza na nacisku klockow
        Ntotal = u * BrakeRigEff;
    else
    {
        u = (BrakePress * P2FTrans) * BrakeCylMult[0] - BrakeSlckAdj;
        if (u * (2.0 - BrakeRigEff) < Ntotal) // histereza na nacisku klockow
            Ntotal = u * (2.0 - BrakeRigEff);
    }

    if (NBrakeAxles * NBpA > 0)
    {
        if (Ntotal > 0) // nie luz
            K += Ntotal; // w kN
        K *= static_cast<double>(BrakeCylNo) / (NBrakeAxles * static_cast<double>(NBpA)); // w kN na os
    }
    if ((BrakeSystem == Pneumatic) || (BrakeSystem == ElectroPneumatic))
    {
        u = Hamulec->GetFC(Vel, K);
        UnitBrakeForce = u * K * 1000.0; // sila na jeden klocek w N
    }
    else
        UnitBrakeForce = K * 1000.0;
    if (((double)NBpA * UnitBrakeForce > TotalMassxg * Adhesive(RunningTrack.friction) / NAxles) &&
        (abs(V) > 0.001))
    // poslizg
    {
        //     Fb = Adhesive(Track.friction) * Mass * g;
        SlippingWheels = true;
    }
    //{  else
    //   begin
    //{     SlippingWheels:=false;}
    //     if (LocalBrake=ManualBrake)or(MBrake=true)) and (BrakePress<0.3) then
    //      Fb:=UnitBrakeForce*NBpA {ham. reczny dziala na jedna os}
    //     else  //yB: to nie do konca ma sens, ponieważ ręczny w wagonie działa na jeden cylinder
    //     hamulcowy/wózek, dlatego potrzebne są oddzielnie liczone osie
    Fb = UnitBrakeForce * NBrakeAxles * std::max(1, NBpA);

    //  u:=((BrakePress*P2FTrans)-BrakeCylSpring*BrakeCylMult[BCMFlag]/BrakeCylNo-0.83*BrakeSlckAdj/(BrakeCylNo))*BrakeCylNo;
    // {  end; }
    return Fb;
}

// *************************************************************************************************
// Q: 20160713
// Obliczanie siły tarcia
// *************************************************************************************************
double TMoverParameters::FrictionForce(double R, int TDamage)
{
    double FF = 0;
    // ABu 240205: chyba juz ekstremalnie zoptymalizowana funkcja liczaca sily tarcia
    if (abs(V) > 0.01)
        FF = (FrictConst1 * V * V) + FrictConst2d;
    else
        FF = (FrictConst1 * V * V) + FrictConst2s;
    return FF;
}

// *************************************************************************************************
// Q: 20160713
// Oblicza przyczepność
// *************************************************************************************************
double TMoverParameters::Adhesive(double staticfriction)
{
    double adhesion;
/*
    // ABu: male przerobki, tylko czy to da jakikolwiek skutek w FPS?
    //     w kazdym razie zaciemni kod na pewno :)
    if (SlippingWheels == false)
    {
        if (SandDose)
            adhesion = (Max0R(staticfriction * (100.0 + Vel) / ((50.0 + Vel) * 11.0), 0.048)) *
                       (11.0 - 2.0 * Random(0.0, 1.0));
        else
            adhesion = (staticfriction * (100.0 + Vel) / ((50.0 + Vel) * 10.0)) *
                       (11.0 - 2.0 * Random(0.0, 1.0));
    }
    else
    {
        if (SandDose)
            adhesion = (0.048) * (11.0 - 2.0 * Random(0.0, 1.0));
        else
            adhesion = (staticfriction * 0.02) * (11.0 - 2.0 * Random(0.0, 1.0));
    }
    //  WriteLog(FloatToStr(adhesive));      // tutaj jest na poziomie 0.2 - 0.3
    return adhesion;
*/
    if( true == SlippingWheels ) {

        if( true == SandDose ) { adhesion = 0.48; }
        else                   { adhesion = staticfriction * 0.2; }
    }
    else {

        if( true == SandDose ) { adhesion = std::max( staticfriction * ( 100.0 + Vel ) / ( 50.0 + Vel ) * 1.1, 0.48 ); }
        else                   { adhesion = staticfriction * ( 100.0 + Vel ) / ( 50.0 + Vel ); }
    }
    adhesion *= ( 11.0 - 2 * Random() ) / 10.0;
    return adhesion;
}

// poprawka dla liczenia sil przy ustawieniu przeciwnym obiektow:
/*
double DirPatch(int Coupler1, int Coupler2)
{
  if (Coupler1 != Coupler2) return 1;
   else return -1;
}


double DirF(int CouplerN)
{
 double rDirF;
  switch (CouplerN)
  {
   case 0: return -1; break;
   case 1: return 1; break;
   default: return 0;
  }
 // if (CouplerN == 0) return -1;
//   else if (CouplerN == 0) return 1;
//    else return 0;
}
*/

// *************************************************************************************************
// Q: 20160713
// Obliczanie sił dzialających na sprzęgach
// *************************************************************************************************
double TMoverParameters::CouplerForce(int CouplerN, double dt)
{
    // wyliczenie siły na sprzęgu
    double tempdist = 0, newdist = 0, distDelta = 0, CF = 0, dV = 0, absdV = 0, Fmax = 0;
    double BetaAvg = 0;
    int CNext = 0;
    const double MaxDist = 405.0; // ustawione + 5 m, bo skanujemy do 400 m
    const double MinDist = 0.5; // ustawione +.5 m, zeby nie rozlaczac przy malych odleglosciach
    const int MaxCount = 1500;
    bool rCF = false;
    // distDelta:=0; //Ra: value never used
    CNext = Couplers[CouplerN].ConnectedNr;
    //  if (Couplers[CouplerN].CForce == 0)  //nie bylo uzgadniane wiec policz

    Couplers[CouplerN].CheckCollision = false;
    newdist = Couplers[CouplerN].CoupleDist; // odległość od sprzęgu sąsiada
    // newdist:=Distance(Loc,Connected^.Loc,Dim,Connected^.Dim);
    if (CouplerN == 0)
    {
        // ABu: bylo newdist+10*((...
        tempdist = ((Couplers[CouplerN].Connected->dMoveLen *
                     DirPatch(0, Couplers[CouplerN].ConnectedNr)) -
                    dMoveLen);
        newdist += 10.0 * tempdist;
        // tempdist:=tempdist+CoupleDist; //ABu: proby szybkiego naprawienia bledu
    }
    else
    {
        // ABu: bylo newdist+10*((...
        tempdist = ((dMoveLen - (Couplers[CouplerN].Connected->dMoveLen *
                                 DirPatch(1, Couplers[CouplerN].ConnectedNr))));
        newdist += 10.0 * tempdist;
        // tempdist:=tempdist+CoupleDist; //ABu: proby szybkiego naprawienia bledu
    }

    // blablabla
    // ABu: proby znalezienia problemu ze zle odbijajacymi sie skladami
    //***if (Couplers[CouplerN].CouplingFlag=ctrain_virtual) and (newdist>0) then
    if ((Couplers[CouplerN].CouplingFlag == ctrain_virtual) && (Couplers[CouplerN].CoupleDist > 0))
    {
        CF = 0; // kontrola zderzania sie - OK
        ScanCounter++;
        if ((newdist > MaxDist) || ((ScanCounter > MaxCount) && (newdist > MinDist)))
        //***if (tempdist>MaxDist) or ((ScanCounter>MaxCount)and(tempdist>MinDist)) then
        { // zerwij kontrolnie wirtualny sprzeg
            // Connected.Couplers[CNext].Connected:=nil; //Ra: ten podłączony niekoniecznie jest
            // wirtualny
            Couplers[CouplerN].Connected = NULL;
            ScanCounter = static_cast<int>(Random(500.0)); // Q: TODO: cy dobrze przetlumaczone?
            // WriteLog(FloatToStr(ScanCounter));
        }
    }
    else
    {
        if (Couplers[CouplerN].CouplingFlag == ctrain_virtual)
        {
            BetaAvg = Couplers[CouplerN].beta;
            Fmax = (Couplers[CouplerN].FmaxC + Couplers[CouplerN].FmaxB) * CouplerTune;
        }
        else // usrednij bo wspolny sprzeg
        {
            BetaAvg =
                (Couplers[CouplerN].beta + Couplers[CouplerN].Connected->Couplers[CNext].beta) /
                2.0;
            Fmax = (Couplers[CouplerN].FmaxC + Couplers[CouplerN].FmaxB +
                    Couplers[CouplerN].Connected->Couplers[CNext].FmaxC +
                    Couplers[CouplerN].Connected->Couplers[CNext].FmaxB) *
                   CouplerTune / 2.0;
        }
        dV = V - (double)DirPatch(CouplerN, CNext) * Couplers[CouplerN].Connected->V;
        absdV = abs(dV);
        if ((newdist < -0.001) && (Couplers[CouplerN].Dist >= -0.001) &&
            (absdV > 0.010)) // 090503: dzwieki pracy zderzakow
        {
            if (SetFlag(SoundFlag, sound_bufferclamp))
                if (absdV > 0.5)
                    SetFlag(SoundFlag, sound_loud);
        }
        else if ((newdist > 0.002) && (Couplers[CouplerN].Dist <= 0.002) &&
                 (absdV > 0.005)) // 090503: dzwieki pracy sprzegu
        {
            if (Couplers[CouplerN].CouplingFlag > 0)
                if (SetFlag(SoundFlag, sound_couplerstretch))
                    if (absdV > 0.1)
                        SetFlag(SoundFlag, sound_loud);
        }
        distDelta =
            abs(newdist) - abs(Couplers[CouplerN].Dist); // McZapkie-191103: poprawka na histereze
        Couplers[CouplerN].Dist = newdist;
        if (Couplers[CouplerN].Dist > 0)
        {
            if (distDelta > 0)
                CF = (-(Couplers[CouplerN].SpringKC +
                        Couplers[CouplerN].Connected->Couplers[CNext].SpringKC) *
                      Couplers[CouplerN].Dist / 2.0) *
                         DirF(CouplerN) -
                     Fmax * dV * BetaAvg;
            else
                CF = (-(Couplers[CouplerN].SpringKC +
                        Couplers[CouplerN].Connected->Couplers[CNext].SpringKC) *
                      Couplers[CouplerN].Dist / 2.0) *
                         DirF(CouplerN) * BetaAvg -
                     Fmax * dV * BetaAvg;
            // liczenie sily ze sprezystosci sprzegu
            if (Couplers[CouplerN].Dist >
                (Couplers[CouplerN].DmaxC +
                 Couplers[CouplerN].Connected->Couplers[CNext].DmaxC)) // zderzenie
                //***if tempdist>(DmaxC+Connected^.Couplers[CNext].DmaxC) then {zderzenie}
                Couplers[CouplerN].CheckCollision = true;
        }
        if (Couplers[CouplerN].Dist < 0)
        {
            if (distDelta > 0)
                CF = (-(Couplers[CouplerN].SpringKB +
                        Couplers[CouplerN].Connected->Couplers[CNext].SpringKB) *
                      Couplers[CouplerN].Dist / 2.0) *
                         DirF(CouplerN) -
                     Fmax * dV * BetaAvg;
            else
                CF = (-(Couplers[CouplerN].SpringKB +
                        Couplers[CouplerN].Connected->Couplers[CNext].SpringKB) *
                      Couplers[CouplerN].Dist / 2.0) *
                         DirF(CouplerN) * BetaAvg -
                     Fmax * dV * BetaAvg;
            // liczenie sily ze sprezystosci zderzaka
            if (-Couplers[CouplerN].Dist >
                (Couplers[CouplerN].DmaxB +
                 Couplers[CouplerN].Connected->Couplers[CNext].DmaxB)) // zderzenie
            //***if -tempdist>(DmaxB+Connected^.Couplers[CNext].DmaxB)/10 then  {zderzenie}
            {
                Couplers[CouplerN].CheckCollision = true;
                if ((Couplers[CouplerN].CouplerType == Automatic) &&
                    (Couplers[CouplerN].CouplingFlag ==
                     0)) // sprzeganie wagonow z samoczynnymi sprzegami}
                    // CouplingFlag:=ctrain_coupler+ctrain_pneumatic+ctrain_controll+ctrain_passenger+ctrain_scndpneumatic;
                    Couplers[CouplerN].CouplingFlag =
                        ctrain_coupler | ctrain_pneumatic | ctrain_controll; // EN57
            }
        }
    }
    if (Couplers[CouplerN].CouplingFlag != ctrain_virtual)
        // uzgadnianie prawa Newtona
        Couplers[CouplerN].Connected->Couplers[1 - CouplerN].CForce = -CF;

    return CF;
}

// *************************************************************************************************
// Q: 20160714
// oblicza sile trakcyjna lokomotywy (dla elektrowozu tez calkowity prad)
// *************************************************************************************************
double TMoverParameters::TractionForce(double dt)
{
    double PosRatio, dmoment, dtrans, tmp, tmpV;
    int i;

    Ft = 0;
    dtrans = 0;
    dmoment = 0;
    // tmpV =Abs(nrot * WheelDiameter / 2);
    // youBy
    if (EngineType == DieselElectric)
    {
        tmp = DElist[MainCtrlPos].RPM / 60.0;
        if ((Heating) && (HeatingPower > 0) && (MainCtrlPosNo > MainCtrlPos))
        {
            i = MainCtrlPosNo;
            while (DElist[i - 2].RPM / 60.0 > tmp)
                i--;
            tmp = DElist[i].RPM / 60.0;
        }

        if (enrot != tmp * int(ConverterFlag))
            if (abs(tmp * int(ConverterFlag) - enrot) < 0.001)
                enrot = tmp * int(ConverterFlag);
            else if ((enrot < DElist[0].RPM * 0.01) && (ConverterFlag))
                enrot += (tmp * int(ConverterFlag) - enrot) * dt / 5.0;
            else
                enrot += (tmp * int(ConverterFlag) - enrot) * 1.5 * dt;
    }
    else if (EngineType != DieselEngine)
        enrot = Transmision.Ratio * nrot;
    else // dla DieselEngine
    {
        if (ShuntMode) // dodatkowa przekładnia np. dla 2Ls150
            dtrans = AnPos * Transmision.Ratio * MotorParam[ScndCtrlActualPos].mIsat;
        else
            dtrans = Transmision.Ratio * MotorParam[ScndCtrlActualPos].mIsat;
        dmoment = dizel_Momentum(dizel_fill, dtrans * nrot * ActiveDir, dt); // oblicza tez
                                                                                 // enrot
    }

    eAngle += enrot * dt;
    if( eAngle > M_PI * 2.0 )
        eAngle = std::fmod( eAngle, M_PI * 2.0 );
/*
    while (eAngle > M_PI * 2.0)
        // eAngle = Pirazy2 - eAngle; <- ABu: a nie czasem tak, jak nizej?
        eAngle -= M_PI * 2.0;
*/
    // hunter-091012: przeniesione z if ActiveDir<>0 (zeby po zejsciu z kierunku dalej spadala
    // predkosc wentylatorow)
    if (EngineType == ElectricSeriesMotor)
    {
        switch (RVentType) // wentylatory rozruchowe}
        {
        case 1:
        {
            if ((ActiveDir != 0) && (RList[MainCtrlActualPos].R > RVentCutOff))
                RventRot += (RVentnmax - RventRot) * RVentSpeed * dt;
            else
                RventRot *= (1.0 - RVentSpeed * dt);
            break;
        }
        case 2:
        {
            if ((abs(Itot) > RVentMinI) && (RList[MainCtrlActualPos].R > RVentCutOff))
                RventRot +=
                    (RVentnmax * abs(Itot) / (ImaxLo * RList[MainCtrlActualPos].Bn) - RventRot) *
                    RVentSpeed * dt;
            else if ((DynamicBrakeType == dbrake_automatic) && (DynamicBrakeFlag))
                RventRot += (RVentnmax * Im / ImaxLo - RventRot) * RVentSpeed * dt;
            else
            {
                RventRot *= (1.0 - RVentSpeed * dt);
                if (RventRot < 0.1)
                    RventRot = 0;
            }
            break;
        }
        }
    }

    if (ActiveDir != 0)
        switch (EngineType)
        {
        case Dumb:
        {
            PosRatio = (MainCtrlPos + ScndCtrlPos) / (MainCtrlPosNo + ScndCtrlPosNo + 0.01);
            if (Mains && (ActiveDir != 0) && (CabNo != 0))
            {
                if (Vel > 0.1)
                {
                    Ft = Min0R(1000.0 * Power / abs(V), Ftmax) * PosRatio;
                }
                else
                    Ft = Ftmax * PosRatio;
                Ft = Ft * DirAbsolute; // ActiveDir*CabNo;
            }
            else
                Ft = 0;
            EnginePower = 1000.0 * Power * PosRatio;
            break;
        } // Dumb

        case WheelsDriven:
        {
            if (EnginePowerSource.SourceType == InternalSource)
                if (EnginePowerSource.PowerType == BioPower)
                    Ft = Sign(sin(eAngle)) * PulseForce * Transmision.Ratio;
            PulseForceTimer = PulseForceTimer + dt;
            if (PulseForceTimer > CtrlDelay)
            {
                PulseForce = 0;
                if (PulseForceCount > 0)
                    PulseForceCount--;
            }
            EnginePower = Ft * (1.0 + Vel);
            break;
        } // WheelsDriven

        case ElectricSeriesMotor:
        {
            //        enrot:=Transmision.Ratio*nrot;
            // yB: szereg dwoch sekcji w ET42
            if ((TrainType == dt_ET42) && (Imax == ImaxHi))
                Voltage = Voltage / 2.0;
            Mm = Momentum(current(enrot, Voltage)); // oblicza tez prad p/slinik

            if (TrainType == dt_ET42)
            {
                if (Imax == ImaxHi)
                    Voltage = Voltage * 2;
                if ((DynamicBrakeFlag) && (abs(Im) > 300)) // przeiesione do mover.cpp
                    FuseOff();
            }
            if ((DynamicBrakeType == dbrake_automatic) && (DynamicBrakeFlag))
            {
                if (((Vadd + abs(Im)) > 760) || (Hamulec->GetEDBCP() < 0.25))
                {
                    Vadd -= 500.0 * dt;
                    if (Vadd < 1)
                        Vadd = 0;
                }
                else if ((DynamicBrakeFlag) && ((Vadd + abs(Im)) < 740))
                {
                    Vadd += 70.0 * dt;
                    Vadd = Min0R(Max0R(Vadd, 60), 400);
                }
                if (Vadd > 0)
                    Mm = MomentumF(Im, Vadd, 0);
            }

            if ((TrainType == dt_ET22) && (DelayCtrlFlag)) // szarpanie przy zmianie układu w byku
                Mm = Mm * RList[MainCtrlActualPos].Bn /
                     (RList[MainCtrlActualPos].Bn +
                      1); // zrobione w momencie, żeby nie dawac elektryki w przeliczaniu sił

			if (abs(Im) > Imax)
				Vhyp += dt; //*(abs(Im) / Imax - 0.9) * 10; // zwieksz czas oddzialywania na PN
            else
                Vhyp = 0;
            if (Vhyp > CtrlDelay / 2) // jesli czas oddzialywania przekroczony
                FuseOff(); // wywalanie bezpiecznika z powodu przetezenia silnikow

			if ((Mains)) // nie wchodzić w funkcję bez potrzeby
				if ((abs(Voltage) < EnginePowerSource.CollectorParameters.MinV) ||
					(abs(Voltage) * EnginePowerSource.CollectorParameters.OVP >
						EnginePowerSource.CollectorParameters.MaxV))
					if (MainSwitch(false))
						EventFlag = true; // wywalanie szybkiego z powodu niewłaściwego napięcia

            if (((DynamicBrakeType == dbrake_automatic) || (DynamicBrakeType == dbrake_switch)) &&
                (DynamicBrakeFlag))
                Itot = Im * 2; // 2x2 silniki w EP09
            else if ((TrainType == dt_EZT) && (Imin == IminLo) &&
                     (ScndS)) // yBARC - boczniki na szeregu poprawnie
                Itot = Im;
            else
                Itot = Im * RList[MainCtrlActualPos].Bn; // prad silnika * ilosc galezi
            Mw = Mm * Transmision.Ratio;
            Fw = Mw * 2.0 / WheelDiameter;
            Ft = Fw * NPoweredAxles; // sila trakcyjna
            break;
        }

        case DieselEngine:
        {
            EnginePower = dmoment * enrot;
            if (MainCtrlPos > 1)
                dmoment -=
                    dizel_Mstand * (0.2 * enrot / dizel_nmax); // dodatkowe opory z powodu sprezarki}
            Mm = dizel_engage * dmoment;
            Mw = Mm * dtrans; // dmoment i dtrans policzone przy okazji enginerotation
            Fw = Mw * 2.0 / WheelDiameter;
            Ft = Fw * NPoweredAxles; // sila trakcyjna
            Ft = Ft * DirAbsolute; // ActiveDir*CabNo;
            break;
        }

        case DieselElectric: // youBy
        {
            //       tmpV:=V*CabNo*ActiveDir;
            tmpV = nrot * Pirazy2 * 0.5 * WheelDiameter * DirAbsolute; //*CabNo*ActiveDir;
            // jazda manewrowa
            if (ShuntMode)
            {
                Voltage = (SST[MainCtrlPos].Umax * AnPos) + (SST[MainCtrlPos].Umin * (1.0 - AnPos));
                tmp = (SST[MainCtrlPos].Pmax * AnPos) + (SST[MainCtrlPos].Pmin * (1.0 - AnPos));
                Ft = tmp * 1000.0 / (abs(tmpV) + 1.6);
                PosRatio = 1;
            }
            else // jazda ciapongowa
            {

                auto power = Power;
                if( true == Heating ) { power -= HeatingPower; }
                if( power < 0.0 ) { power = 0.0; }
                tmp = std::min( DElist[ MainCtrlPos ].GenPower, power );// Power - HeatingPower * double( Heating ));

                PosRatio = DElist[MainCtrlPos].GenPower / DElist[MainCtrlPosNo].GenPower;
                // stosunek mocy teraz do mocy max
                if ((MainCtrlPos > 0) && (ConverterFlag))
                    if (tmpV <
                        (Vhyp * power /
                         DElist[MainCtrlPosNo].GenPower)) // czy na czesci prostej, czy na hiperboli
                        Ft = (Ftmax -
                              ((Ftmax - 1000.0 * DElist[MainCtrlPosNo].GenPower / (Vhyp + Vadd)) *
                               (tmpV / Vhyp) / PowerCorRatio)) *
                             PosRatio; // posratio - bo sila jakos tam sie rozklada

                    // Ft:=(Ftmax - (Ftmax - (1000.0 * DEList[MainCtrlPosNo].genpower /
                    //(Vhyp+Vadd) / PowerCorRatio)) * (tmpV/Vhyp)) * PosRatio //wersja z Megapacka
                    else // na hiperboli                             //1.107 -
                        // wspolczynnik sredniej nadwyzki Ft w symku nad charakterystyka
                        Ft = 1000.0 * tmp / (tmpV + Vadd) /
                             PowerCorRatio; // tu jest zawarty stosunek mocy
                else
                    Ft = 0; // jak nastawnik na zero, to sila tez zero

                PosRatio = tmp / DElist[MainCtrlPosNo].GenPower;
            }

            if (FuseFlag)
                Ft = 0;
            else
                Ft = Ft * DirAbsolute; // ActiveDir * CabNo; //zwrot sily i jej wartosc
            Fw = Ft / NPoweredAxles; // sila na obwodzie kola
            Mw = Fw * WheelDiameter / 2.0; // moment na osi kola
            Mm = Mw / Transmision.Ratio; // moment silnika trakcyjnego

            // with MotorParam[ScndCtrlPos] do
            if (abs(Mm) > MotorParam[ScndCtrlPos].fi)
                Im = NPoweredAxles *
                     abs(abs(Mm) / MotorParam[ScndCtrlPos].mfi + MotorParam[ScndCtrlPos].mIsat);
            else
                Im = NPoweredAxles * sqrt(abs(Mm * MotorParam[ScndCtrlPos].Isat));

            if (ShuntMode)
            {
                EnginePower = Voltage * Im / 1000.0;
                if (EnginePower > tmp)
                {
                    EnginePower = tmp * 1000.0;
                    Voltage = EnginePower / Im;
                }
                if (EnginePower < tmp)
                    Ft = Ft * EnginePower / tmp;
            }
            else
            {
                if (abs(Im) > DElist[MainCtrlPos].Imax)
                { // nie ma nadmiarowego, tylko Imax i zwarcie na pradnicy
                    Ft = Ft / Im * DElist[MainCtrlPos].Imax;
                    Im = DElist[MainCtrlPos].Imax;
                }

                if (Im > 0) // jak pod obciazeniem
                    if (Flat) // ograniczenie napiecia w pradnicy - plaszczak u gory
                        Voltage = 1000.0 * tmp / abs(Im);
                    else // charakterystyka pradnicy obcowzbudnej (elipsa) - twierdzenie Pitagorasa

                    {
                        Voltage = sqrt(abs(sqr(DElist[MainCtrlPos].Umax) -
                                           sqr(DElist[MainCtrlPos].Umax * Im /
                                               DElist[MainCtrlPos].Imax))) *
                                      (MainCtrlPos - 1) +
                                  (1.0 - Im / DElist[MainCtrlPos].Imax) * DElist[MainCtrlPos].Umax *
                                      (MainCtrlPosNo - MainCtrlPos);
                        Voltage = Voltage / (MainCtrlPosNo - 1);
                        Voltage = Min0R(Voltage, (1000.0 * tmp / abs(Im)));
                        if (Voltage < (Im * 0.05))
                            Voltage = Im * 0.05;
                    }
                if ((Voltage > DElist[MainCtrlPos].Umax) ||
                    (Im == 0)) // gdy wychodzi za duze napiecie
                    Voltage = DElist[MainCtrlPos].Umax *
                              int(ConverterFlag); // albo przy biegu jalowym (jest cos takiego?)

                EnginePower = Voltage * Im / 1000.0;

                if ((tmpV > 2) && (EnginePower < tmp))
                    Ft = Ft * EnginePower / tmp;
            }

            if ((Imax > 1) && (Im > Imax))
                FuseOff();
            if (FuseFlag)
                Voltage = 0;

            // przekazniki bocznikowania, kazdy inny dla kazdej pozycji
            if ((MainCtrlPos == 0) || (ShuntMode))
                ScndCtrlPos = 0;

            else if (AutoRelayFlag)
                switch (RelayType)
                {
                case 0:
                {
                    if ((Im <= (MPTRelay[ScndCtrlPos].Iup * PosRatio)) &&
                        (ScndCtrlPos < ScndCtrlPosNo))
                        ++ScndCtrlPos;
                    if ((Im >= (MPTRelay[ScndCtrlPos].Idown * PosRatio)) && (ScndCtrlPos > 0))
                        --ScndCtrlPos;
                    break;
                }
                case 1:
                {
                    if ((MPTRelay[ScndCtrlPos].Iup < Vel) && (ScndCtrlPos < ScndCtrlPosNo))
                        ++ScndCtrlPos;
                    if ((MPTRelay[ScndCtrlPos].Idown > Vel) && (ScndCtrlPos > 0))
                        --ScndCtrlPos;
                    break;
                }
                case 2:
                {
                    if ((MPTRelay[ScndCtrlPos].Iup < Vel) && (ScndCtrlPos < ScndCtrlPosNo) &&
                        (EnginePower < (tmp * 0.99)))
                        ++ScndCtrlPos;
                    if ((MPTRelay[ScndCtrlPos].Idown < Im) && (ScndCtrlPos > 0))
                        --ScndCtrlPos;
                    break;
                }
                case 41:
                {
                    if ((MainCtrlPos == MainCtrlPosNo) &&
                        (tmpV * 3.6 > MPTRelay[ScndCtrlPos].Iup) && (ScndCtrlPos < ScndCtrlPosNo))
                    {
                        ++ScndCtrlPos;
                        enrot = enrot * 0.73;
                    }
                    if ((Im > MPTRelay[ScndCtrlPos].Idown) && (ScndCtrlPos > 0))
                        --ScndCtrlPos;
                    break;
                }
                case 45:
                {
                    if ((MainCtrlPos > 11) && (ScndCtrlPos < ScndCtrlPosNo))
                        if ((ScndCtrlPos == 0))
                            if ((MPTRelay[ScndCtrlPos].Iup > Im))
                                ++ScndCtrlPos;
                            else if ((MPTRelay[ScndCtrlPos].Iup < Vel))
                                ++ScndCtrlPos;

                    // malenie
                    if ((ScndCtrlPos > 0) && (MainCtrlPos < 12))
                        if ((ScndCtrlPos == ScndCtrlPosNo))
                            if ((MPTRelay[ScndCtrlPos].Idown < Im))
                                --ScndCtrlPos;
                            else if ((MPTRelay[ScndCtrlPos].Idown > Vel))
                                --ScndCtrlPos;
                    if ((MainCtrlPos < 11) && (ScndCtrlPos > 2))
                        ScndCtrlPos = 2;
                    if ((MainCtrlPos < 9) && (ScndCtrlPos > 0))
                        ScndCtrlPos = 0;
                }
                case 46:
                {
                    // wzrastanie
                    if ((MainCtrlPos > 9) && (ScndCtrlPos < ScndCtrlPosNo))
                        if ((ScndCtrlPos) % 2 == 0)
                            if ((MPTRelay[ScndCtrlPos].Iup > Im))
                                ++ScndCtrlPos;
                            else if ((MPTRelay[ScndCtrlPos - 1].Iup > Im) &&
                                     (MPTRelay[ScndCtrlPos].Iup < Vel))
                                ++ScndCtrlPos;

                    // malenie
                    if ((MainCtrlPos < 10) && (ScndCtrlPos > 0))
                        if ((ScndCtrlPos) % 2 == 0)
                            if ((MPTRelay[ScndCtrlPos].Idown < Im))
                                --ScndCtrlPos;
                            else if ((MPTRelay[ScndCtrlPos + 1].Idown < Im) &&
                                     (MPTRelay[ScndCtrlPos].Idown > Vel))
                                --ScndCtrlPos;
                    if ((MainCtrlPos < 9) && (ScndCtrlPos > 2))
                        ScndCtrlPos = 2;
                    if ((MainCtrlPos < 6) && (ScndCtrlPos > 0))
                        ScndCtrlPos = 0;
                }
                } // switch RelayType
			break;
        } // DieselElectric

        case ElectricInductionMotor:
        {
            if ((Mains)) // nie wchodzić w funkcję bez potrzeby
                if ((abs(Voltage) < EnginePowerSource.CollectorParameters.MinV) ||
                    (abs(Voltage) > EnginePowerSource.CollectorParameters.MaxV + 200))
                {
                    MainSwitch(false);
                }
            tmpV = abs(nrot) * (PI * WheelDiameter) *
                   3.6; //*DirAbsolute*eimc[eimc_s_p]; - do przemyslenia dzialanie pp
            if ((Mains))
            {

                dtrans = Hamulec->GetEDBCP();
                if (((DoorLeftOpened) || (DoorRightOpened)))
                    DynamicBrakeFlag = true;
                else if (((dtrans < 0.25) && (LocHandle->GetCP() < 0.25) && (AnPos < 0.01)) ||
                         ((dtrans < 0.25) && (ShuntModeAllow) && (LocalBrakePos == 0)))
                    DynamicBrakeFlag = false;
                else if ((((BrakePress > 0.25) && (dtrans > 0.25) || (LocHandle->GetCP() > 0.25))) ||
                         (AnPos > 0.02))
                    DynamicBrakeFlag = true;
                dtrans = Hamulec->GetEDBCP() * eimc[eimc_p_abed]; // stala napedu
                if ((DynamicBrakeFlag))
                {
                    if (eimv[eimv_Fmax] * Sign(V) * DirAbsolute < -1)
                    {
                        PosRatio = -Sign(V) * DirAbsolute * eimv[eimv_Fr] /
                                   (eimc[eimc_p_Fh] *
                                    Max0R(dtrans / MaxBrakePress[0], AnPos) /*dizel_fill*/);
                    }
                    else
                        PosRatio = 0;
                    PosRatio = Round(20.0 * PosRatio) / 20.0;
                    if (PosRatio < 19.5 / 20.0)
                        PosRatio *= 0.9;
                    //           if PosRatio<0 then
                    //             PosRatio:=2+PosRatio-2;
                    Hamulec->SetED(Max0R(0.0, std::min(PosRatio, 1.0)));
                    //           (Hamulec as TLSt).SetLBP(LocBrakePress*(1-PosRatio));
                    PosRatio = -std::max(std::min(dtrans * 1.0 / MaxBrakePress[0], 1.0), AnPos) *
                               std::max(0.0, std::min(1.0, (Vel - eimc[eimc_p_Vh0]) /
                                                     (eimc[eimc_p_Vh1] - eimc[eimc_p_Vh0])));
                    eimv[eimv_Fzad] = -std::max(LocalBrakeRatio(), dtrans / MaxBrakePress[0]);
                    tmp = 5;
                }
                else
                {
                    PosRatio = static_cast<double>( MainCtrlPos ) / static_cast<double>( MainCtrlPosNo );
                    eimv[eimv_Fzad] = PosRatio;
                    if ((Flat) && (eimc[eimc_p_F0] * eimv[eimv_Fful] > 0))
                        PosRatio = Min0R(PosRatio * eimc[eimc_p_F0] / eimv[eimv_Fful], 1);
                    if (ScndCtrlActualPos > 0)
                        if (Vmax < 250)
                            PosRatio = Min0R(PosRatio, Max0R(-1, 0.5 * (ScndCtrlActualPos - Vel)));
                        else
                            PosRatio =
                                Min0R(PosRatio, Max0R(-1, 0.5 * (ScndCtrlActualPos * 2 - Vel)));
                    // PosRatio = 1.0 * (PosRatio * 0 + 1) * PosRatio; // 1 * 1 * PosRatio = PosRatio
                    Hamulec->SetED(0);
                    //           (Hamulec as TLSt).SetLBP(LocBrakePress);
                    if ((PosRatio > dizel_fill))
                        tmp = 1;
                    else
                        tmp = 4; // szybkie malenie, powolne wzrastanie
                }
                //         if SlippingWheels then begin PosRatio:=0; tmp:=10; SandDoseOn;
                //         end;//przeciwposlizg

                //         if(Flat)then //PRZECIWPOŚLIZG
                dmoment = eimv[eimv_Fful];
                //         else
                //           dmoment:=eimc[eimc_p_F0]*0.99;
                if ((abs((PosRatio + 9.66 * dizel_fill) * dmoment * 100) >
                     0.95 * Adhesive(RunningTrack.friction) * TotalMassxg))
                {
                    PosRatio = 0;
                    tmp = 4;
                    SandDoseOn();
                } // przeciwposlizg
                if ((abs((PosRatio + 9.80 * dizel_fill) * dmoment * 100) >
                     0.95 * Adhesive(RunningTrack.friction) * TotalMassxg))
                {
                    PosRatio = 0;
                    tmp = 9;
                    SandDoseOn();
                } // przeciwposlizg
                if ((SlippingWheels))
                {
                    // PosRatio = -PosRatio * 0; // serio -0 ???
					PosRatio = 0;
                    tmp = 9;
                    SandDoseOn();
                } // przeciwposlizg

                dizel_fill += Max0R(Min0R(PosRatio - dizel_fill, 0.1), -0.1) * 2 *
                                 (tmp /*2{+4*byte(PosRatio<dizel_fill)*/) *
                                 dt; // wartość zadana/procent czegoś

                if ((DynamicBrakeFlag))
                    tmp = eimc[eimc_f_Uzh];
                else
                    tmp = eimc[eimc_f_Uzmax];

                eimv[eimv_Uzsmax] = Min0R(Voltage - eimc[eimc_f_DU], tmp);
                eimv[eimv_fkr] = eimv[eimv_Uzsmax] / eimc[eimc_f_cfu];
                if ((dizel_fill < 0))
                    eimv[eimv_Pmax] = eimc[eimc_p_Ph];
                else
                    eimv[eimv_Pmax] =
                        Min0R(eimc[eimc_p_Pmax],
                              0.001 * Voltage * (eimc[eimc_p_Imax] - eimc[eimc_f_I0]) * Pirazy2 *
                                  eimc[eimc_s_cim] / eimc[eimc_s_p] / eimc[eimc_s_cfu]);
                eimv[eimv_FMAXMAX] =
                    0.001 * sqr(Min0R(eimv[eimv_fkr] / Max0R(abs(enrot) * eimc[eimc_s_p] +
                                                                 eimc[eimc_s_dfmax] * eimv[eimv_ks],
                                                             eimc[eimc_s_dfmax]),
                                      1) *
                                eimc[eimc_f_cfu] / eimc[eimc_s_cfu]) *
                    (eimc[eimc_s_dfmax] * eimc[eimc_s_dfic] * eimc[eimc_s_cim]) *
                    Transmision.Ratio * NPoweredAxles * 2.0 / WheelDiameter;
                if ((dizel_fill < 0))
                {
                    eimv[eimv_Fful] = std::min(eimc[eimc_p_Ph] * 3.6 / (Vel != 0.0 ? Vel : 0.001),
                                               std::min(eimc[eimc_p_Fh], eimv[eimv_FMAXMAX]));
                    eimv[eimv_Fmax] =
                        -Sign(V) * (DirAbsolute)*std::min(
                                       eimc[eimc_p_Ph] * 3.6 / (Vel != 0.0 ? Vel : 0.001),
                                       std::min(-eimc[eimc_p_Fh] * dizel_fill, eimv[eimv_FMAXMAX]));
                    //*Min0R(1,(Vel-eimc[eimc_p_Vh0])/(eimc[eimc_p_Vh1]-eimc[eimc_p_Vh0]))
                }
                else
                {
                    eimv[eimv_Fful] = Min0R(Min0R(3.6 * eimv[eimv_Pmax] / Max0R(Vel, 1),
                                                  eimc[eimc_p_F0] - Vel * eimc[eimc_p_a1]),
                                            eimv[eimv_FMAXMAX]);
                    //           if(not Flat)then
                    eimv[eimv_Fmax] = eimv[eimv_Fful] * dizel_fill;
                    //           else
                    //             eimv[eimv_Fmax]:=Min0R(eimc[eimc_p_F0]*dizel_fill,eimv[eimv_Fful]);
                }

                eimv[eimv_ks] = eimv[eimv_Fmax] / eimv[eimv_FMAXMAX];
                eimv[eimv_df] = eimv[eimv_ks] * eimc[eimc_s_dfmax];
                eimv[eimv_fp] = DirAbsolute * enrot * eimc[eimc_s_p] +
                                eimv[eimv_df]; // do przemyslenia dzialanie pp z tmpV
                //         eimv[eimv_U]:=Max0R(eimv[eimv_Uzsmax],Min0R(eimc[eimc_f_cfu]*eimv[eimv_fp],eimv[eimv_Uzsmax]));
                //         eimv[eimv_pole]:=eimv[eimv_U]/(eimv[eimv_fp]*eimc[eimc_s_cfu]);
                if ((abs(eimv[eimv_fp]) <= eimv[eimv_fkr]))
                    eimv[eimv_pole] = eimc[eimc_f_cfu] / eimc[eimc_s_cfu];
                else
                    eimv[eimv_pole] =
                        eimv[eimv_Uzsmax] / eimc[eimc_s_cfu] / abs(eimv[eimv_fp]);
                eimv[eimv_U] = eimv[eimv_pole] * eimv[eimv_fp] * eimc[eimc_s_cfu];
                eimv[eimv_Ic] = (eimv[eimv_fp] - DirAbsolute * enrot * eimc[eimc_s_p]) *
                                eimc[eimc_s_dfic] * eimv[eimv_pole];
                eimv[eimv_If] = eimv[eimv_Ic] * eimc[eimc_s_icif];
                eimv[eimv_M] = eimv[eimv_pole] * eimv[eimv_Ic] * eimc[eimc_s_cim];
                eimv[eimv_Ipoj] = (eimv[eimv_Ic] * NPoweredAxles * eimv[eimv_U]) /
                                      (Voltage - eimc[eimc_f_DU]) +
                                  eimc[eimc_f_I0];
                eimv[eimv_Pm] =
                    ActiveDir * eimv[eimv_M] * NPoweredAxles * enrot * Pirazy2 / 1000;
                eimv[eimv_Pe] = eimv[eimv_Ipoj] * Voltage / 1000;
                eimv[eimv_eta] = eimv[eimv_Pm] / eimv[eimv_Pe];

                Im = eimv[eimv_If];
                if ((eimv[eimv_Ipoj] >= 0))
                    Vadd *= (1.0 - 2.0 * dt);
                else if ((Voltage < EnginePowerSource.CollectorParameters.MaxV))
                    Vadd *= (1.0 - dt);
                else
                    Vadd = Max0R(
                        Vadd * (1.0 - 0.2 * dt),
                        0.007 * (Voltage - (EnginePowerSource.CollectorParameters.MaxV - 100)));
                Itot = eimv[eimv_Ipoj] * (0.01 + Min0R(0.99, 0.99 - Vadd));

                EnginePower = abs(eimv[eimv_Ic] * eimv[eimv_U] * NPoweredAxles) / 1000;
                tmpV = eimv[eimv_fp];
                if (((abs(eimv[eimv_If]) > 1) || (abs(tmpV) > 0.1)) && (RlistSize > 0))
                {
                    i = 0;
                    while ((i < RlistSize - 1) && (DElist[i + 1].RPM < abs(tmpV)))
                        i++;
                    RventRot = (abs(tmpV) - DElist[i].RPM) /
                                   (DElist[i + 1].RPM - DElist[i].RPM) *
                                   (DElist[i + 1].GenPower - DElist[i].GenPower) +
                               DElist[i].GenPower;
                }
                else
                    RventRot = 0;

                Mm = eimv[eimv_M] * DirAbsolute;
                Mw = Mm * Transmision.Ratio;
                Fw = Mw * 2.0 / WheelDiameter;
                Ft = Fw * NPoweredAxles;
                eimv[eimv_Fr] = DirAbsolute * Ft / 1000;
                //       RventRot;
            }
            else
            {
                Im = 0;
                Mm = 0;
                Mw = 0;
                Fw = 0;
                Ft = 0;
                Itot = 0;
                dizel_fill = 0;
                EnginePower = 0;
                {
                    for (i = 0; i < 21; i++)
                        eimv[i] = 0;
                }
                Hamulec->SetED(0);
                RventRot = 0.0; //(Hamulec as TLSt).SetLBP(LocBrakePress);
            }
			break;
        } // ElectricInductionMotor
        case None:
        {
            break;
        }
        } // case EngineType
    return Ft;
}

// *************************************************************************************************
// Q: 20160713
//Obliczenie predkości obrotowej kół???
// *************************************************************************************************
double TMoverParameters::ComputeRotatingWheel(double WForce, double dt, double n)
{
    double newn = 0, eps = 0;
    if ((n == 0) && (WForce * Sign(V) < 0))
        newn = 0;
    else
    {
        eps = WForce * WheelDiameter / (2.0 * AxleInertialMoment);
        newn = n + eps * dt;
        if ((newn * n <= 0) && (eps * n < 0))
            newn = 0;
    }
    return newn;
}

// *************************************************************************************************
// Q: 20160713
// Sprawdzenie bezpiecznika nadmiarowego
// *************************************************************************************************
bool TMoverParameters::FuseFlagCheck(void)
{
    bool FFC;

    FFC = false;
    if (Power > 0.01)
        FFC = FuseFlag;
    else // pobor pradu jezeli niema mocy
        for (int b = 0; b < 2; b++)
            if (TestFlag(Couplers[b].CouplingFlag, ctrain_controll))
                if (Couplers[b].Connected->Power > 0.01)
                    FFC = Couplers[b].Connected->FuseFlagCheck();

    return FFC;
}

// *************************************************************************************************
// Q: 20160713
// Załączenie bezpiecznika nadmiarowego
// *************************************************************************************************
bool TMoverParameters::FuseOn(void)
{
    bool FO = false;
    if ((MainCtrlPos == 0) && (ScndCtrlPos == 0) && (TrainType != dt_ET40) &&
        ((Mains) || (TrainType != dt_EZT)) && (!TestFlag(EngDmgFlag, 1)))
    { // w ET40 jest blokada nastawnika, ale czy działa dobrze?
        SendCtrlToNext("FuseSwitch", 1, CabNo);
        if (((EngineType == ElectricSeriesMotor) || ((EngineType == DieselElectric))) && FuseFlag)
        {
            FuseFlag = false; // wlaczenie ponowne obwodu
            FO = true;
            SetFlag(SoundFlag, sound_relay | sound_loud);
        }
    }
    return FO;
}

// *************************************************************************************************
// Q: 20160713
// Wyłączenie bezpiecznika nadmiarowego
// *************************************************************************************************
void TMoverParameters::FuseOff(void)
{
    if (!FuseFlag)
    {
        FuseFlag = true;
        EventFlag = true;
        SetFlag(SoundFlag, sound_relay | sound_loud);
    }
}

// *************************************************************************************************
// Q: 20160713
// Przeliczenie prędkości liniowej na obrotową
// *************************************************************************************************
double TMoverParameters::v2n(void)
{
    // przelicza predkosc liniowa na obrotowa
    const double dmgn = 0.5;
    double n, deltan = 0;

    n = V / (M_PI * WheelDiameter); // predkosc obrotowa wynikajaca z liniowej [obr/s]
    deltan = n - nrot; //"pochodna" prędkości obrotowej
    if (SlippingWheels)
        if (std::abs(deltan) < 0.01)
            SlippingWheels = false; // wygaszenie poslizgu
    if (SlippingWheels) // nie ma zwiazku z predkoscia liniowa V
    { // McZapkie-221103: uszkodzenia kol podczas poslizgu
        if (deltan > dmgn)
            if (FuzzyLogic(deltan, dmgn, p_slippdmg))
                if (SetFlag(DamageFlag, dtrain_wheelwear)) // podkucie
                    EventFlag = true;
        if (deltan < -dmgn)
            if (FuzzyLogic(-deltan, dmgn, p_slippdmg))
                if (SetFlag(DamageFlag, dtrain_thinwheel)) // wycieranie sie obreczy
                    EventFlag = true;
        n = nrot; // predkosc obrotowa nie zalezy od predkosci liniowej
    }
    return n;
}

// *************************************************************************************************
// Q: 20160714
// Oblicza moment siły wytwarzany przez silnik
// *************************************************************************************************
double TMoverParameters::Momentum(double I)
{
    // liczy moment sily wytwarzany przez silnik elektryczny}
    int SP;

    SP = ScndCtrlActualPos;
    if (ScndInMain)
        if (!(RList[MainCtrlActualPos].ScndAct == 255))
            SP = RList[MainCtrlActualPos].ScndAct;

    //     Momentum:=mfi*I*(1-1.0/(Abs(I)/mIsat+1));
    return (MotorParam[SP].mfi * I *
            (abs(I) / (abs(I) + MotorParam[SP].mIsat) - MotorParam[SP].mfi0));
}

// *************************************************************************************************
// Q: 20160714
// Oblicza moment siły do sterowania wzbudzeniem
// *************************************************************************************************
double TMoverParameters::MomentumF(double I, double Iw, int SCP)
{
    // umozliwia dokladne sterowanie wzbudzeniem

    return (MotorParam[SCP].mfi * I *
            Max0R(abs(Iw) / (abs(Iw) + MotorParam[SCP].mIsat) - MotorParam[SCP].mfi0, 0));
}

// *************************************************************************************************
// Q: 20160713
// Odłączenie uszkodzonych silników
// *************************************************************************************************
bool TMoverParameters::CutOffEngine(void)
{
    bool COE = false; // Ra: wartość domyślna, sprawdzić to trzeba
    if ((NPoweredAxles > 0) && (CabNo == 0) && (EngineType == ElectricSeriesMotor))
    {
        if (SetFlag(DamageFlag, -dtrain_engine))
        {
            NPoweredAxles = NPoweredAxles / 2; // bylo div czyli mod?
            COE = true;
        }
    }
    return COE;
}

// *************************************************************************************************
// Q: 20160713
// Przełączenie wysoki / niski prąd rozruchu
// *************************************************************************************************
bool TMoverParameters::MaxCurrentSwitch(bool State)
{
    bool MCS = false;
    if (EngineType == ElectricSeriesMotor)
        if (ImaxHi > ImaxLo)
        {
            if (State && (Imax == ImaxLo) && (RList[MainCtrlPos].Bn < 2) &&
                !((TrainType == dt_ET42) && (MainCtrlPos > 0)))
            {
                Imax = ImaxHi;
                MCS = true;
                if (CabNo != 0)
                    SendCtrlToNext("MaxCurrentSwitch", 1, CabNo);
            }
            if (!State)
                if (Imax == ImaxHi)
                    if (!((TrainType == dt_ET42) && (MainCtrlPos > 0)))
                    {
                        Imax = ImaxLo;
                        MCS = true;
                        if (CabNo != 0)
                            SendCtrlToNext("MaxCurrentSwitch", 0, CabNo);
                    }
        }
    return MCS;
}

// *************************************************************************************************
// Q: 20160713
// Przełączenie wysoki / niski prąd rozruchu automatycznego
// *************************************************************************************************
bool TMoverParameters::MinCurrentSwitch(bool State)
{
    bool MCS = false;
    if (((EngineType == ElectricSeriesMotor) && (IminHi > IminLo)) || (TrainType == dt_EZT))
    {
        if (State && (Imin == IminLo))
        {
            Imin = IminHi;
            MCS = true;
            if (CabNo != 0)
                SendCtrlToNext("MinCurrentSwitch", 1, CabNo);
        }
        if ((!State) && (Imin == IminHi))
        {
            Imin = IminLo;
            MCS = true;
            if (CabNo != 0)
                SendCtrlToNext("MinCurrentSwitch", 0, CabNo);
        }
    }
    return MCS;
}

// *************************************************************************************************
// Q: 20160713
// Sprawdzenie wskaźnika jazdy na oporach
// *************************************************************************************************
bool TMoverParameters::ResistorsFlagCheck(void)
{
    bool RFC = false;

    if (Power > 0.01)
        RFC = ResistorsFlag;
	else // pobor pradu jezeli niema mocy
	{
		for (int b = 0; b < 2; b++)
			if (TestFlag(Couplers[b].CouplingFlag, ctrain_controll))
				if (Couplers[b].Connected->Power > 0.01)
					RFC = Couplers[b].Connected->ResistorsFlagCheck();
	}
    return RFC;
}

// *************************************************************************************************
// Q: 20160713
// Włączenie / wyłączenie automatycznego rozruchu
// *************************************************************************************************
bool TMoverParameters::AutoRelaySwitch(bool State)
{
    bool ARS;
    if ((AutoRelayType == 2) && (AutoRelayFlag != State))
    {
        AutoRelayFlag = State;
        ARS = true;
        SendCtrlToNext("AutoRelaySwitch", int(State), CabNo);
    }
    else
        ARS = false;

    return ARS;
}

// *************************************************************************************************
// Q: 20160724
// Sprawdzenie warunków pracy automatycznego rozruchu
// *************************************************************************************************

bool TMoverParameters::AutoRelayCheck(void)
{
    bool OK = false; // b:int;
    bool ARFASI = false;
    bool ARFASI2 = false; // sprawdzenie wszystkich warunkow (AutoRelayFlag, AutoSwitch, Im<Imin)
    bool ARC = false;

    // Ra 2014-06: dla SN61 nie działa prawidłowo
    // rozlaczanie stycznikow liniowych
    if ((!Mains) || (FuseFlag) || (MainCtrlPos == 0) ||
        ((BrakePress > 2.1) && (TrainType != dt_EZT)) ||
        (ActiveDir == 0)) // hunter-111211: wylacznik cisnieniowy
    {
        StLinFlag = false; // yBARC - rozlaczenie stycznikow liniowych
        OK = false;
        if (!DynamicBrakeFlag)
        {
            Im = 0;
            Itot = 0;
            ResistorsFlag = false;
        }
    }

    ARFASI2 = (!AutoRelayFlag) || ((MotorParam[ScndCtrlActualPos].AutoSwitch) &&
                                    (abs(Im) < Imin)); // wszystkie warunki w jednym
    ARFASI = (!AutoRelayFlag) || ((RList[MainCtrlActualPos].AutoSwitch) && (abs(Im) < Imin)) ||
              ((!RList[MainCtrlActualPos].AutoSwitch) &&
               (RList[MainCtrlActualPos].Relay < MainCtrlPos)); // wszystkie warunki w jednym
    // brak PSR                   na tej pozycji działa PSR i prąd poniżej progu
    // na tej pozycji nie działa PSR i pozycja walu ponizej
    //                         chodzi w tym wszystkim o to, żeby można było zatrzymać rozruch na
    //                         jakiejś pozycji wpisując Autoswitch=0 i wymuszać
    //                         przejście dalej przez danie nastawnika na dalszą pozycję - tak to do
    //                         tej pory działało i na tym się opiera fizyka ET22-2k
    {
        if (StLinFlag)
        {
            if ((RList[MainCtrlActualPos].R == 0) &&
                ((ScndCtrlActualPos > 0) || (ScndCtrlPos > 0)) &&
                (!(CoupledCtrl) || (RList[MainCtrlActualPos].Relay == MainCtrlPos)))
            { // zmieniaj scndctrlactualpos
                // scnd bez samoczynnego rozruchu
                if (ScndCtrlActualPos < ScndCtrlPos)
                {
                    if ((LastRelayTime > CtrlDelay) && (ARFASI2))
                    {
                        ScndCtrlActualPos++;
                        OK = true;
                    }
                }
                else if (ScndCtrlActualPos > ScndCtrlPos)
                {
                    if ((LastRelayTime > CtrlDownDelay) && (TrainType != dt_EZT))
                    {
                        ScndCtrlActualPos--;
                        OK = true;
                    }
                }
                else
                    OK = false;
            }
            else
            { // zmieniaj mainctrlactualpos
                if ((ActiveDir < 0) && (TrainType != dt_PseudoDiesel))
                    if (RList[MainCtrlActualPos + 1].Bn > 1)
                    {
                        return false; // nie poprawiamy przy konwersji
                        // return ARC;// bbylo exit; //Ra: to powoduje, że EN57 nie wyłącza się przy
                        // IminLo
                    }
                // main bez samoczynnego rozruchu

                if ((RList[MainCtrlActualPos].Relay < MainCtrlPos) ||
                    (RList[MainCtrlActualPos + 1].Relay == MainCtrlPos) ||
                    ((TrainType == dt_ET22) && (DelayCtrlFlag)))
                {
                    if ((RList[MainCtrlPos].R == 0) && (MainCtrlPos > 0) &&
                        (!(MainCtrlPos == MainCtrlPosNo)) && (FastSerialCircuit == 1))
                    {
                        MainCtrlActualPos++;
                        //                 MainCtrlActualPos:=MainCtrlPos; //hunter-111012:
                        //                 szybkie wchodzenie na bezoporowa (303E)
                        OK = true;
                        SetFlag(SoundFlag, sound_manyrelay | sound_loud);
                    }
                    else if ((LastRelayTime > CtrlDelay) && (ARFASI))
                    {
                        // WriteLog("LRT = " + FloatToStr(LastRelayTime) + ", " +
                        // FloatToStr(CtrlDelay));
                        if ((TrainType == dt_ET22) && (MainCtrlPos > 1) &&
                            ((RList[MainCtrlActualPos].Bn < RList[MainCtrlActualPos + 1].Bn) ||
                             (DelayCtrlFlag))) // et22 z walem grupowym
                            if (!DelayCtrlFlag) // najpierw przejscie
                            {
                                MainCtrlActualPos++;
                                DelayCtrlFlag = true; // tryb przejscia
                                OK = true;
                            }
                            else if (LastRelayTime > 4 * CtrlDelay) // przejscie
                            {

                                DelayCtrlFlag = false;
                                OK = true;
                            }
                            else
                                ;
                        else // nie ET22 z wałem grupowym
                        {
                            MainCtrlActualPos++;
                            OK = true;
                        }
                        //---------
                        // hunter-111211: poprawki
                        if (MainCtrlActualPos > 0)
                            if ((RList[MainCtrlActualPos].R == 0) &&
                                (!(MainCtrlActualPos == MainCtrlPosNo))) // wejscie na bezoporowa
                            {
                                SetFlag(SoundFlag, sound_manyrelay | sound_loud);
                            }
                            else if ((RList[MainCtrlActualPos].R > 0) &&
                                     (RList[MainCtrlActualPos - 1].R ==
                                      0)) // wejscie na drugi uklad
                            {
                                SetFlag(SoundFlag, sound_manyrelay);
                            }
                    }
                }
                else if (RList[MainCtrlActualPos].Relay > MainCtrlPos)
                {
                    if ((RList[MainCtrlPos].R == 0) && (MainCtrlPos > 0) &&
                        (!(MainCtrlPos == MainCtrlPosNo)) && (FastSerialCircuit == 1))
                    {
                        MainCtrlActualPos--;
                        //                 MainCtrlActualPos:=MainCtrlPos; //hunter-111012:
                        //                 szybkie wchodzenie na bezoporowa (303E)
                        OK = true;
                        SetFlag(SoundFlag, sound_manyrelay);
                    }
                    else if (LastRelayTime > CtrlDownDelay)
                    {
                        if (TrainType != dt_EZT) // tutaj powinien być tryb sterowania wałem
                        {
                            MainCtrlActualPos--;
                            OK = true;
                        }
                        if (MainCtrlActualPos > 0) // hunter-111211: poprawki
                            if (RList[MainCtrlActualPos].R ==
                                0) // dzwieki schodzenia z bezoporowej}
                            {
                                SetFlag(SoundFlag, sound_manyrelay);
                            }
                    }
                }
                else if ((RList[MainCtrlActualPos].R > 0) && (ScndCtrlActualPos > 0))
                {
                    if (LastRelayTime > CtrlDownDelay)
                    {
                        ScndCtrlActualPos--; // boczniki nie dzialaja na poz. oporowych
                        OK = true;
                    }
                }
                else
                    OK = false;
            }
        }
        else // not StLinFlag
        {
            OK = false;
            // ybARC - tutaj sa wszystkie warunki, jakie musza byc spelnione, zeby mozna byla
            // zalaczyc styczniki liniowe
            if (((MainCtrlPos == 1) || ((TrainType == dt_EZT) && (MainCtrlPos > 0))) &&
                (!FuseFlag) && (Mains) && ((BrakePress < 1.0) || (TrainType == dt_EZT)) &&
                (MainCtrlActualPos == 0) && (ActiveDir != 0))
            { //^^ TODO: sprawdzic BUG, prawdopodobnie w CreateBrakeSys()
                DelayCtrlFlag = true;
                if (LastRelayTime >= InitialCtrlDelay)
                {
                    StLinFlag = true; // ybARC - zalaczenie stycznikow liniowych
                    MainCtrlActualPos = 1;
                    DelayCtrlFlag = false;
                    SetFlag(SoundFlag, sound_relay | sound_loud);
                    OK = true;
                }
            }
            else
                DelayCtrlFlag = false;

            if ((!StLinFlag) && ((MainCtrlActualPos > 0) || (ScndCtrlActualPos > 0)))
                if ((TrainType == dt_EZT) && (CoupledCtrl)) // EN57 wal jednokierunkowy calosciowy
                {
                    if (MainCtrlActualPos == 1)
                    {
                        MainCtrlActualPos = 0;
                        OK = true;
                    }
                    else if (LastRelayTime > CtrlDownDelay)
                    {
                        if (MainCtrlActualPos < RlistSize)
                            MainCtrlActualPos++; // dojdz do konca
                        else if (ScndCtrlActualPos < ScndCtrlPosNo)
                            ScndCtrlActualPos++; // potem boki
                        else
                        { // i sie przewroc na koniec
                            MainCtrlActualPos = 0;
                            ScndCtrlActualPos = 0;
                        }
                        OK = true;
                    }
                }
                else if (CoupledCtrl) // wal kulakowy dwukierunkowy
                {
                    if (LastRelayTime > CtrlDownDelay)
                    {
                        if (ScndCtrlActualPos > 0)
                            ScndCtrlActualPos--;
                        else
                            MainCtrlActualPos--;
                        OK = true;
                    }
                }
                else
                {
                    MainCtrlActualPos = 0;
                    ScndCtrlActualPos = 0;
                    OK = true;
                }
        }
        if (OK)
            LastRelayTime = 0;

        return OK;
    }
}

// *************************************************************************************************
// Q: 20160713
// Podnosi / opuszcza przedni pantograf
// *************************************************************************************************
bool TMoverParameters::PantFront(bool State)
{
    double pf1 = 0;
    bool PF = false;

    if ((Battery ==
         true) /* and ((TrainType<>dt_ET40)or ((TrainType=dt_ET40) and (EnginePowerSource.CollectorsNo>1)))*/)
    {
        PF = true;
        if (State == true)
            pf1 = 1;
        else
            pf1 = 0;
        if (PantFrontUp != State)
        {
            PantFrontUp = State;
            if (State == true)
            {
                PantFrontStart = 0;
                SendCtrlToNext("PantFront", 1, CabNo);
            }
            else
            {
                PF = false;
                PantFrontStart = 1;
                SendCtrlToNext("PantFront", 0, CabNo);
                //{Ra: nie ma potrzeby opuszczać obydwu na raz, jak mozemy każdy osobno
                //      if (TrainType == dt_EZT) && (ActiveCab == 1)
                //       {
                //        PantRearUp = false;
                //        PantRearStart = 1;
                //        SendCtrlToNext("PantRear", 0, CabNo);
                //       }
                //}
            }
        }
        else
            SendCtrlToNext("PantFront", pf1, CabNo);
    }
    return PF;
}

// *************************************************************************************************
// Q: 20160713
// Podnoszenie / opuszczanie pantografu tylnego
// *************************************************************************************************
bool TMoverParameters::PantRear(bool State)
{
    double pf1;
	bool PR = false;

    if (Battery == true)
    {
        PR = true;
        if (State == true)
            pf1 = 1;
        else
            pf1 = 0;
        if (PantRearUp != State)
        {
            PantRearUp = State;
            if (State == true)
            {
                PantRearStart = 0;
                SendCtrlToNext("PantRear", 1, CabNo);
            }
            else
            {
                PR = false;
                PantRearStart = 1;
                SendCtrlToNext("PantRear", 0, CabNo);
            }
        }
        else
            SendCtrlToNext("PantRear", pf1, CabNo);
    }
    return PR;
}

// *************************************************************************************************
// Q: 20160715
// Zmienia parametr do którego dąży sprzęgło
// *************************************************************************************************
bool TMoverParameters::dizel_EngageSwitch(double state)
{
    if ((EngineType == DieselEngine) && (state <= 1) && (state >= 0) &&
        (state != dizel_engagestate))
    {
        dizel_engagestate = state;
        return true;
    }
    else
        return false;
}

// *************************************************************************************************
// Q: 20160715
// Zmienia parametr do którego dąży sprzęgło
// *************************************************************************************************
bool TMoverParameters::dizel_EngageChange(double dt)
{
    const double engagedownspeed = 0.9;
    const double engageupspeed = 0.5;
    double engagespeed = 0; // OK:boolean;
    bool DEC;

    DEC = false;
    if (dizel_engage - dizel_engagestate > 0)
        engagespeed = engagedownspeed;
    else
        engagespeed = engageupspeed;
    if (dt > 0.2)
        dt = 0.1;
    if (abs(dizel_engage - dizel_engagestate) < 0.11)
    {
        if (dizel_engage != dizel_engagestate)
        {
            DEC = true;
            dizel_engage = dizel_engagestate;
        }
        // else OK:=false; //już jest false
    }
    else
    {
        dizel_engage = dizel_engage + engagespeed * dt * (dizel_engagestate - dizel_engage);
        // OK:=false;
    }
    // dizel_EngageChange:=OK;
    return DEC;
}

// *************************************************************************************************
// Q: 20160715
// Automatyczna zmiana biegów gdy prędkość przekroczy widełki
// *************************************************************************************************
bool TMoverParameters::dizel_AutoGearCheck(void)
{
    bool OK;

    OK = false;
    if (MotorParam[ScndCtrlActualPos].AutoSwitch && Mains)
    {
        if (RList[MainCtrlPos].Mn == 0)
        {
            if (dizel_engagestate > 0)
                dizel_EngageSwitch(0);
            if ((MainCtrlPos == 0) && (ScndCtrlActualPos > 0))
                dizel_automaticgearstatus = -1;
        }
        else
        {
            if (MotorParam[ScndCtrlActualPos].AutoSwitch &&
                (dizel_automaticgearstatus == 0)) // sprawdz czy zmienic biegi
            {
                if ((Vel > MotorParam[ScndCtrlActualPos].mfi) &&
                    (ScndCtrlActualPos < ScndCtrlPosNo))
                {
                    dizel_automaticgearstatus = 1;
                    OK = true;
                }
                else if ((Vel < MotorParam[ScndCtrlActualPos].fi) && (ScndCtrlActualPos > 0))
                {
                    dizel_automaticgearstatus = -1;
                    OK = true;
                }
            }
        }
        if ((dizel_engage < 0.1) && (dizel_automaticgearstatus != 0))
        {
            if (dizel_automaticgearstatus == 1)
                ScndCtrlActualPos++;
            else
                ScndCtrlActualPos--;
            dizel_automaticgearstatus = 0;
            dizel_EngageSwitch(1.0);
            OK = true;
        }
    }

    if (Mains)
    {
        if (dizel_automaticgearstatus == 0) // ustaw cisnienie w silowniku sprzegla}
            switch (RList[MainCtrlPos].Mn)
            {
            case 1:
                dizel_EngageSwitch(0.5);
				break;
            case 2:
                dizel_EngageSwitch(1.0);
				break;
            default:
                dizel_EngageSwitch(0.0);
            }
        else
            dizel_EngageSwitch(0.0);
        if (!(MotorParam[ScndCtrlActualPos].mIsat > 0))
            dizel_EngageSwitch(0.0); // wylacz sprzeglo na pozycjach neutralnych
        if (!AutoRelayFlag)
            ScndCtrlActualPos = ScndCtrlPos;
    }
    return OK;
}

// *************************************************************************************************
// Q: 20160715
// Aktualizacja stanu silnika
// *************************************************************************************************
bool TMoverParameters::dizel_Update(double dt)
{
    const double fillspeed = 2;
    bool DU;

    // dizel_Update:=false;
    if (dizel_enginestart && (LastSwitchingTime >= InitialCtrlDelay))
    {
        dizel_enginestart = false;
        LastSwitchingTime = 0;
        enrot = dizel_nmin / 2.0; // TODO: dac zaleznie od temperatury i baterii
    }
    /*OK=*/dizel_EngageChange(dt);
    //  if AutoRelayFlag then Poprawka na SM03
    DU = dizel_AutoGearCheck();
    // dizel_fill:=(dizel_fill+dizel_fillcheck(MainCtrlPos))/2;
    dizel_fill = dizel_fill + fillspeed * dt * (dizel_fillcheck(MainCtrlPos) - dizel_fill);
    // dizel_Update:=OK;

    return DU;
}

// *************************************************************************************************
// Q: 20160715
// oblicza napelnienie, uzwglednia regulator obrotow
// *************************************************************************************************
double TMoverParameters::dizel_fillcheck(int mcp)
{ 
    double realfill, nreg;

    realfill = 0;
    nreg = 0;
    if (Mains && (MainCtrlPosNo > 0))
    {
        if (dizel_enginestart &&
            (LastSwitchingTime >= 0.9 * InitialCtrlDelay)) // wzbogacenie przy rozruchu
            realfill = 1;
        else
            realfill = RList[mcp].R; // napelnienie zalezne od MainCtrlPos
        if (dizel_nmax_cutoff > 0)
        {
            switch (RList[MainCtrlPos].Mn)
            {
            case 0:
            case 1:
                nreg = dizel_nmin;
				break;
            case 2:
                if (dizel_automaticgearstatus == 0)
                    nreg = dizel_nmax;
                else
                    nreg = dizel_nmin;
				break;
            default:
                realfill = 0; // sluczaj
            }
            if (enrot > nreg)
                realfill = realfill * (3.9 - 3.0 * abs(enrot) / nreg);
            if (enrot > dizel_nmax_cutoff)
                realfill = realfill * (9.8 - 9.0 * abs(enrot) / dizel_nmax_cutoff);
            if (enrot < dizel_nmin)
                realfill = realfill * (1.0 + (dizel_nmin - abs(enrot)) / dizel_nmin);
        }
    }
    if (realfill < 0)
        realfill = 0;
    if (realfill > 1)
        realfill = 1;
    return realfill;
}

// *************************************************************************************************
// Q: 20160715
// Oblicza moment siły wytwarzany przez silnik spalinowy
// *************************************************************************************************
double TMoverParameters::dizel_Momentum(double dizel_fill, double n, double dt)
{ // liczy moment sily wytwarzany przez silnik spalinowy}
    double Moment = 0, enMoment = 0, eps = 0, newn = 0, friction = 0;

    // friction =dizel_engagefriction*(11-2*random)/10;
    friction = dizel_engagefriction;
    if (enrot > 0)
    { // sqr TODO: sqr c++
        Moment = dizel_Mmax * dizel_fill -
                 (dizel_Mmax - dizel_Mnmax * dizel_fill) *
                     sqr(enrot / (dizel_nmax - dizel_nMmax * dizel_fill)) -
                 dizel_Mstand; // Q: zamieniam SQR() na sqr()
        //    Moment:=Moment*(1+sin(eAngle*4))-dizel_Mstand*(1+cos(eAngle*4));}
    }
    else
        Moment = -dizel_Mstand;
    if (enrot < dizel_nmin / 10.0)
        if (eAngle < PI / 2.0)
            Moment -=  dizel_Mstand; // wstrzymywanie przy malych obrotach
    //!! abs
    if (abs(abs(n) - enrot) < 0.1)
    {
        if ((Moment) > (dizel_engageMaxForce * dizel_engage * dizel_engageDia * friction *
                        2)) // zerwanie przyczepnosci sprzegla
            enrot +=  dt * Moment / dizel_AIM;
        else
        {
            dizel_engagedeltaomega = 0;
            enrot = abs(n); // jest przyczepnosc tarcz
        }
    }
    else
    {
        if ((enrot == 0) && (Moment < 0))
            newn = 0;
        else
        {
            //!! abs
            dizel_engagedeltaomega = enrot - n; // sliganie tarcz
            enMoment = Moment -
                       Sign(dizel_engagedeltaomega) * dizel_engageMaxForce * dizel_engage *
                           dizel_engageDia * friction;
            Moment = Sign(dizel_engagedeltaomega) * dizel_engageMaxForce * dizel_engage *
                     dizel_engageDia * friction;
            dizel_engagedeltaomega = abs(enrot - abs(n));
            eps = enMoment / dizel_AIM;
            newn = enrot + eps * dt;
            if ((newn * enrot <= 0) && (eps * enrot < 0))
                newn = 0;
        }
        enrot = newn;
    }
    if ((enrot == 0) && (!dizel_enginestart))
        Mains = false;

    return Moment;
}

// *************************************************************************************************
// Q: 20160713
// Test zakończenia załadunku / rozładunku
// *************************************************************************************************
bool TMoverParameters::LoadingDone(double LSpeed, std::string LoadInit)
{
    // test zakończenia załadunku/rozładunku
    long LoadChange = 0;
    bool LD = false;

    // ClearPendingExceptions; // zabezpieczenie dla Trunc()
    // LoadingDone:=false; //nie zakończone
    if (!LoadInit.empty()) // nazwa ładunku niepusta
    {
        if (Load > MaxLoad)
            LoadChange = abs(long(LSpeed * LastLoadChangeTime / 2.0)); // przeładowanie?
        else
            LoadChange = abs(long(LSpeed * LastLoadChangeTime));
        if (LSpeed < 0) // gdy rozładunek
        {
            LoadStatus = 2; // trwa rozładunek (włączenie naliczania czasu)
            if (LoadChange != 0) // jeśli coś przeładowano
            {
                LastLoadChangeTime = 0; // naliczony czas został zużyty
                Load -= LoadChange; // zmniejszenie ilości ładunku
                CommandIn.Value1 =
                    CommandIn.Value1 - LoadChange; // zmniejszenie ilości do rozładowania
                if (Load < 0)
                    Load = 0; //ładunek nie może być ujemny
                if ((Load == 0) || (CommandIn.Value1 < 0)) // pusto lub rozładowano żądaną ilość
                    LoadStatus = 4; // skończony rozładunek
                if (Load == 0)
                    LoadType.clear(); // jak nic nie ma, to nie ma też nazwy
            }
        }
        else if (LSpeed > 0) // gdy załadunek
        {
            LoadStatus = 1; // trwa załadunek (włączenie naliczania czasu)
            if (LoadChange != 0) // jeśli coś przeładowano
            {
                LastLoadChangeTime = 0; // naliczony czas został zużyty
                LoadType = LoadInit; // nazwa
                Load += LoadChange; // zwiększenie ładunku
                CommandIn.Value1 = CommandIn.Value1 - LoadChange;
                if ((Load >= MaxLoad * (1.0 + OverLoadFactor)) || (CommandIn.Value1 < 0))
                    LoadStatus = 4; // skończony załadunek
            }
        }
        else
            LoadStatus = 4; // zerowa prędkość zmiany, to koniec
    }
    return (LoadStatus >= 4);
}

// *************************************************************************************************
// Q: 20160713
// Zwraca informacje o działającej blokadzie drzwi
// *************************************************************************************************
bool TMoverParameters::DoorBlockedFlag(void)
{
    // if (DoorBlocked=true) and (Vel<5.0) then
    bool DBF = false;
    if ((DoorBlocked == true) && (Vel >= 5.0))
        DBF = true;

    return DBF;
}

// *************************************************************************************************
// Q: 20160713
// Otwiera / zamyka lewe drzwi
// *************************************************************************************************
bool TMoverParameters::DoorLeft(bool State)
{
    bool DL = false;
    if ((DoorLeftOpened != State) && (DoorBlockedFlag() == false) && (Battery == true))
    {
        DL = true;
        DoorLeftOpened = State;
        if (State == true)
        {
            if (CabNo > 0)
                SendCtrlToNext("DoorOpen", 1, CabNo); // 1=lewe, 2=prawe
            else
                SendCtrlToNext("DoorOpen", 2, CabNo); // zamiana
            CompressedVolume -= 0.003;
        }
        else
        {
            if (CabNo > 0)
                SendCtrlToNext("DoorClose", 1, CabNo);
            else
                SendCtrlToNext("DoorClose", 2, CabNo);
        }
    }
    else
        DL = false;
    return DL;
}

// *************************************************************************************************
// Q: 20160713
// Otwiera / zamyka prawe drzwi
// *************************************************************************************************
bool TMoverParameters::DoorRight(bool State)
{
    bool DR = false;
    if ((DoorRightOpened != State) && (DoorBlockedFlag() == false) && (Battery == true))
    {
        DR = true;
        DoorRightOpened = State;
        if (State == true)
        {
            if (CabNo > 0)
                SendCtrlToNext("DoorOpen", 2, CabNo); // 1=lewe, 2=prawe
            else
                SendCtrlToNext("DoorOpen", 1, CabNo); // zamiana
            CompressedVolume -= 0.003;
        }
        else
        {
            if (CabNo > 0)
                SendCtrlToNext("DoorClose", 2, CabNo);
            else
                SendCtrlToNext("DoorClose", 1, CabNo);
        }
    }
    else
        DR = false;

    return DR;
}

// *************************************************************************************************
// Q: 20160713
// Przesuwa pojazd o podaną wartość w bok względem toru (dla samochodów)
// *************************************************************************************************
bool TMoverParameters::ChangeOffsetH(double DeltaOffset)
{
    bool COH = false;
    if (TestFlag(CategoryFlag, 2) && TestFlag(RunningTrack.CategoryFlag, 2))
    {
        OffsetTrackH = OffsetTrackH + DeltaOffset;
        //     if (abs(OffsetTrackH) > (RunningTrack.Width / 1.95 - TrackW / 2.0))
        if (abs(OffsetTrackH) >
            (0.5 * (RunningTrack.Width - Dim.W) - 0.05)) // Ra: może pół pojazdu od brzegu?
            COH = false; // kola na granicy drogi
        else
            COH = true;
    }
    else
        COH = false;

    return COH;
}

// *************************************************************************************************
// Q: 20160713
// Testuje zmienną (narazie tylko 0) i na podstawie uszkodzenia zwraca informację tekstową
// *************************************************************************************************
std::string TMoverParameters::EngineDescription(int what)
{
    std::string outstr;

    outstr = "";
    switch (what)
    {
    case 0:
    {
        if (DamageFlag == 255)
            outstr = "Totally destroyed!";
        else
        {
            if (TestFlag(DamageFlag, dtrain_thinwheel))
                if (Power > 0.1)
                    outstr = "Thin wheel,";
                else
                    outstr = "Load shifted,";
            if (TestFlag(DamageFlag, dtrain_wheelwear))
                outstr = "Wheel wear,";
            if (TestFlag(DamageFlag, dtrain_bearing))
                outstr = "Bearing damaged,";
            if (TestFlag(DamageFlag, dtrain_coupling))
                outstr = "Coupler broken,";
            if (TestFlag(DamageFlag, dtrain_loaddamage))
                if (Power > 0.1)
                    outstr = "Ventilator damaged,";
                else
                    outstr = "Load damaged,";

            if (TestFlag(DamageFlag, dtrain_loaddestroyed))
                if (Power > 0.1)
                    outstr = "Engine damaged,";
                else
                    outstr = "Load destroyed!,";
            if (TestFlag(DamageFlag, dtrain_axle))
                outstr = "Axle broken,";
            if (TestFlag(DamageFlag, dtrain_out))
                outstr = "DERAILED!";
            if (outstr == "")
                outstr = "OK!";
        }
        break;
    }
    default:
        outstr = "Invalid qualifier";
        break;
    }
    return outstr;
}

// *************************************************************************************************
// Q: 20160709
// Funkcja zwracajaca napiecie dla calego skladu, przydatna dla EZT
// *************************************************************************************************
double TMoverParameters::GetTrainsetVoltage(void)
{//ABu: funkcja zwracajaca napiecie dla calego skladu, przydatna dla EZT
	return Max0R(HVCouplers[1][1], HVCouplers[0][1]);
}

// *************************************************************************************************
// Kasowanie zmiennych pracy fizyki
// *************************************************************************************************
bool TMoverParameters::Physic_ReActivation(void) // DO PRZETLUMACZENIA NA KONCU
{
//    bool pr;
    if (PhysicActivation)
        return false;
    else
    {
        PhysicActivation = true;
        LastSwitchingTime = 0;
        return true;
    }
}

// *************************************************************************************************
// FUNKCJE PARSERA WCZYTYWANIA PLIKU FIZYKI POJAZDU
// *************************************************************************************************
std::string p0, p1, p2, p3, p4, p5, p6, p7;
std::string vS;
int vI;
double vD;
bool startBPT;
bool startMPT, startMPT0;
bool startRLIST;
bool startDLIST, startFFLIST, startWWLIST;
bool startLIGHTSLIST;
int LISTLINE;
std::vector<std::string> x;

// *************************************************************************************************
// Q: 20160717
// *************************************************************************************************
size_t Pos(std::string str_find, std::string in)
{
    size_t pos = in.find(str_find);
    return (pos != std::string::npos ? pos+1 : 0);
}
/*
// *************************************************************************************************
// Q: 20160717
// *************************************************************************************************
bool issection(std::string const &name)
{
    sectionname = name;
    if (xline.compare(0, name.size(), name) == 0)
    {
        lastsectionname = name;
        return true;
    }
    else
        return false;
}
*/
bool issection( std::string const &Name, std::string const &Input ) {

    return ( Input.compare( 0, Name.size(), Name ) == 0 );
}

int MARKERROR(int code, std::string type, std::string msg)
{
    WriteLog(msg);
    return code;
}

int s2NPW(std::string s)
{ // wylicza ilosc osi napednych z opisu ukladu osi
	const char A = 64;
	int NPW = 0;
	for (std::size_t k = 0; k < s.size(); ++k)
	{
		if (s[k] >= (char)65 && s[k] <= (char)90)
			NPW += s[k] - A;
	}
	return NPW;
}

int s2NNW(std::string s)
{ // wylicza ilosc osi nienapedzanych z opisu ukladu osi
	const char Zero = 48;
	int NNW = 0;
	for (std::size_t k = 0; k < s.size(); ++k)
	{
		if (s[k] >= (char)49 && s[k] <= (char)57)
			NNW += s[k] - Zero;
	}
	return NNW;
}

// *************************************************************************************************
// Q: 20160717
// *************************************************************************************************
// parsowanie Motor Param Table
bool TMoverParameters::readMPT0( std::string const &line ) {

    cParser parser( line );
    if( false == parser.getTokens( 7, false ) ) {
        WriteLog( "Read MPT0: arguments missing in line " + std::to_string( LISTLINE ) );
        return false;
    }
    int idx = 0; // numer pozycji
    parser >> idx;
    parser
        >> MotorParam[ idx ].mfi
        >> MotorParam[ idx ].mIsat
        >> MotorParam[ idx ].mfi0
        >> MotorParam[ idx ].fi
        >> MotorParam[ idx ].Isat
        >> MotorParam[ idx ].fi0;
    if( true == parser.getTokens( 1, false ) ) {
        int autoswitch;
        parser >> autoswitch;
        MotorParam[ idx ].AutoSwitch = ( autoswitch == 1 );
    }
    else {
        MotorParam[ idx ].AutoSwitch = false;
    }
    return true;
}

bool TMoverParameters::readMPT( std::string const &line ) {

    ++LISTLINE;

    switch( EngineType ) {

        case ElectricSeriesMotor: { return readMPTElectricSeries( line ); }
        case DieselElectric:      { return readMPTDieselElectric( line ); }
        case DieselEngine:        { return readMPTDieselEngine( line ); }
        default:                  { return false; }
    }
}

bool TMoverParameters::readMPTElectricSeries(std::string const &line) {

    cParser parser( line );
    if( false == parser.getTokens( 5, false ) ) {
        WriteLog( "Read MPT: arguments missing in line " + std::to_string( LISTLINE ) );
			return false;
		}
    int idx = 0; // numer pozycji
    parser >> idx;
    parser
        >> MotorParam[ idx ].mfi
        >> MotorParam[ idx ].mIsat
        >> MotorParam[ idx ].fi
        >> MotorParam[ idx ].Isat;
    if( true == parser.getTokens( 1, false ) ) {
        int autoswitch;
        parser >> autoswitch;
        MotorParam[ idx ].AutoSwitch = (autoswitch == 1); }
    else{
        MotorParam[ idx ].AutoSwitch = false;
    }
    return true;
}

bool TMoverParameters::readMPTDieselElectric( std::string const &line ) {

    cParser parser( line );
    if( false == parser.getTokens( 7, false ) ) {
        WriteLog( "Read MPT: arguments missing in line " + std::to_string( LISTLINE ) );
        return false;
    }
    int idx = 0; // numer pozycji
    parser >> idx;
    parser
        >> MotorParam[ idx ].mfi
        >> MotorParam[ idx ].mIsat
        >> MotorParam[ idx ].fi
        >> MotorParam[ idx ].Isat
        >> MPTRelay[ idx ].Iup
        >> MPTRelay[ idx ].Idown;

    return true;
}

bool TMoverParameters::readMPTDieselEngine( std::string const &line ) {

    cParser parser( line );
    if( false == parser.getTokens( 4, false ) ) {
        WriteLog( "Read MPT: arguments missing in line " + std::to_string( LISTLINE ) );
        return false;
    }
    int idx = 0; // numer pozycji
    parser >> idx;
    parser
        >> MotorParam[ idx ].mIsat
        >> MotorParam[ idx ].fi
        >> MotorParam[ idx ].mfi;
    if( true == parser.getTokens( 1, false ) ) {
        int autoswitch;
        parser >> autoswitch;
        MotorParam[ idx ].AutoSwitch = ( autoswitch == 1 );
    }
    else {
        MotorParam[ idx ].AutoSwitch = false;
    }
    return true;
}

bool TMoverParameters::readBPT( std::string const &line ) {

    cParser parser( line );
    if( false == parser.getTokens( 5, false ) ) {

		WriteLog( "Read BPT: arguments missing in line " + std::to_string( LISTLINE + 1 ) );
            return false;
    }
    ++LISTLINE;
    std::string braketype; int idx = 0;
    parser >> idx;
    parser
        >> BrakePressureTable[ idx ].PipePressureVal
        >> BrakePressureTable[ idx ].BrakePressureVal
        >> BrakePressureTable[ idx ].FlowSpeedVal
        >> braketype;
              if( braketype == "Pneumatic" )        { BrakePressureTable[ idx ].BrakeType = Pneumatic; } 
         else if( braketype == "ElectroPneumatic" ) { BrakePressureTable[ idx ].BrakeType = ElectroPneumatic; }
         else                                       { BrakePressureTable[ idx ].BrakeType = Individual; }

    return true;
}

bool TMoverParameters::readRList( std::string const &Input ) {

    cParser parser( Input );
    if( false == parser.getTokens( 5, false ) ) {

        WriteLog( "Read RList: arguments missing in line " + std::to_string( LISTLINE + 1 ) );
        return false;
    }
    auto idx = LISTLINE++;
    if( idx >= sizeof( RList ) ) {
        WriteLog( "Read RList: number of entries exceeded capacity of the data table" );
        return false;
    }
    parser
        >> RList[ idx ].Relay
        >> RList[ idx ].R
        >> RList[ idx ].Bn
        >> RList[ idx ].Mn
        >> RList[ idx ].AutoSwitch;

    if( true == parser.getTokens( 1, false ) ) { parser >> RList[ idx ].ScndAct; }
    else                                       {           RList[ idx ].ScndAct = 0; }

    return true;
}

bool TMoverParameters::readDList( std::string const &line ) {

    cParser parser( line );
    parser.getTokens( 3, false );
    auto idx = LISTLINE++;
    if( idx >= sizeof( RList ) ) {
        WriteLog( "Read DList: number of entries exceeded capacity of the data table" );
        return false;
    }
    parser
        >> RList[ idx ].Relay
        >> RList[ idx ].R
        >> RList[ idx ].Mn;

    return true;
}

bool TMoverParameters::readFFList( std::string const &line ) {

    cParser parser( line );
    if( false == parser.getTokens( 2, false ) ) {
    WriteLog( "Read FList: arguments missing in line " + std::to_string( LISTLINE + 1 ) );
    return false;
    }
    int idx = LISTLINE++;
    if( idx >= sizeof( DElist ) ) {
        WriteLog( "Read FList: number of entries exceeded capacity of the data table" );
        return false;
    }
    parser
        >> DElist[ idx ].RPM
        >> DElist[ idx ].GenPower;

    return true;
}

// parsowanie WWList
bool TMoverParameters::readWWList( std::string const &line ) {

    cParser parser( line );
    if( false == parser.getTokens( 4, false ) ) {
        WriteLog( "Read WWList: arguments missing in line " + std::to_string( LISTLINE + 1 ) );
        return false;
    }
    int idx = LISTLINE++;
    if( idx >= sizeof( DElist ) ) {
        WriteLog( "Read WWList: number of entries exceeded capacity of the data table" );
        return false;
    }
    parser
        >> DElist[ idx ].RPM
        >> DElist[ idx ].GenPower
        >> DElist[ idx ].Umax
        >> DElist[ idx ].Imax;

    if( true == parser.getTokens( 3, false ) ) {
        // optional parameters for shunt mode
        parser
            >> SST[ idx ].Umin
            >> SST[ idx ].Umax
            >> SST[ idx ].Pmax;

        SST[ idx ].Pmin = std::sqrt( std::pow( SST[ idx ].Umin, 2 ) / 47.6 );
        SST[ idx ].Pmax = std::min( SST[ idx ].Pmax, std::pow( SST[ idx ].Umax, 2 ) / 47.6 );
    }

    return true;
}

bool TMoverParameters::readLightsList( std::string const &Input ) {

    cParser parser( Input );
    if( false == parser.getTokens( 2, false ) ) {
        WriteLog( "Read LightsList: arguments missing in line " + std::to_string( LISTLINE + 1 ) );
        return false;
    }
    int idx = LISTLINE++;
    if( idx > 16 ) {
        WriteLog( "Read LightsList: number of entries exceeded capacity of the data table" );
        return false;
    }
    parser
        >> Lights[ 0 ][ idx ]
        >> Lights[ 1 ][ idx ];

    return true;
}

// *************************************************************************************************
// Q: 20160719
// *************************************************************************************************
void TMoverParameters::BrakeValveDecode( std::string const &Valve ) {

    std::map<std::string, TBrakeValve> valvetypes{
        { "W", W },
        { "W_Lu_L", W_Lu_L },
        { "W_Lu_XR", W_Lu_XR },
        { "W_Lu_VI", W_Lu_VI },
        { "K", K },
        { "Kg", Kg },
        { "Kp", Kp },
        { "Kss", Kss },
        { "Kkg", Kkg },
        { "Kkp", Kkp },
        { "Kks", Kks },
        { "Hikp1", Hikp1 },
        { "Hikss", Hikss },
        { "Hikg1", Hikg1 },
        { "KE", KE },
        { "SW", SW },
        { "EStED", EStED },
        { "NESt3", NESt3 },
        { "ESt3", ESt3 },
        { "LSt", LSt },
        { "ESt4", ESt4 },
        { "ESt3AL2", ESt3AL2 },
        { "EP1", EP1 },
        { "EP2", EP2 },
        { "M483", M483 },
        { "CV1_L_TR", CV1_L_TR },
        { "CV1", CV1 },
        { "CV1_R", CV1_R }
    };
    auto lookup = valvetypes.find( Valve );
    BrakeValve =
        lookup != valvetypes.end() ?
        lookup->second :
        Other;

    if( ( BrakeValve == Other )
        && ( Valve.find( "ESt" ) != std::string::npos ) ) {

        BrakeValve = ESt3;
    }
}

// *************************************************************************************************
// Q: 20160719
// *************************************************************************************************
void TMoverParameters::BrakeSubsystemDecode()
{
    BrakeSubsystem = ss_None;
    switch (BrakeValve)
    {
	case W:
	case W_Lu_L:
	case W_Lu_VI:
	case W_Lu_XR:
        BrakeSubsystem = ss_W;
        break;
	case ESt3:
	case ESt3AL2:
	case ESt4:
	case EP2:
	case EP1:
        BrakeSubsystem = ss_ESt;
        break;
    case KE:
        BrakeSubsystem = ss_KE;
        break;
	case CV1:
	case CV1_L_TR:
        BrakeSubsystem = ss_Dako;
        break;
	case LSt:
	case EStED:
        BrakeSubsystem = ss_LSt;
        break;
    }
}

// *************************************************************************************************
// Q: 20160717
// Funkcja pelniaca role pierwotnej LoadChkFile wywolywana w dynobj.cpp w double
// TDynamicObject::Init()
// Po niej wykonywana jest CreateBrakeSys(), ktora jest odpowiednikiem CheckLocomotiveParameters()
// *************************************************************************************************
bool TMoverParameters::LoadFIZ(std::string chkpath)
{
    const int param_ok = 1;
    const int wheels_ok = 2;
    const int dimensions_ok = 4;

    ConversionError = 666;
    LISTLINE = 0;
    startBPT = false;
    startMPT = false;
    startMPT0 = false;
    startRLIST = false;
    startDLIST = false;
    startFFLIST = false;
    startWWLIST = false;
    startLIGHTSLIST = false;
    std::string file = chkpath + TypeName + ".fiz";

    WriteLog("LOAD FIZ FROM " + file);

    std::ifstream in(file);
	if (!in.is_open())
	{
		WriteLog("E8 - FIZ FILE NOT EXIST.");
		return false;
	}

    ConversionError = 0;

    // Zbieranie danych zawartych w pliku FIZ
    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    std::unordered_map<std::string, std::string> fizlines;
    std::string inputline;
    while (std::getline(in, inputline))
    {
        bool comment = ( ( inputline.find('#') != std::string::npos )
			          || ( inputline.compare( 0, 2, "//" ) == 0 ) );
        if( true == comment ) {
            // skip commented lines
            continue;
        }

		if( inputline.length() == 0 ) {
			startBPT = false;
			continue;
		}
        // checking if table parsing should be switched off goes first...
		if( issection( "END-MPT", inputline ) ) {
			startBPT = false;
			startMPT = false;
            startMPT0 = false;
			continue;
		}
        if( issection( "END-RL", inputline ) ) {
			startBPT = false;
			startRLIST = false;
			continue;
		}
        if( issection( "END-DL", inputline ) ) {
            startBPT = false;
            startDLIST = false;
            continue;
        }
        if( issection( "endff", inputline ) ) {
            startBPT = false;
            startFFLIST = false;
            continue;
        }
        if( issection( "END-WWL", inputline ) ) {
            startBPT = false;
            startWWLIST = false;
            continue;
        }
        if( issection( "endL", inputline ) ) {
            startBPT = false;
            startLIGHTSLIST = false;
            continue;
        }
        // ...then all recognized sections...
        if( issection( "Param.", inputline ) )
        {
			startBPT = false;
            fizlines.emplace( "Param", inputline );
            LoadFIZ_Param( inputline );
			continue;
        }

        if( issection( "Load:", inputline ) )
        {
			startBPT = false;
            fizlines.emplace( "Load", inputline );
            LoadFIZ_Load( inputline );
            continue;
        }

        if( issection( "Dimensions:", inputline ) )
        {
			startBPT = false;
            fizlines.emplace( "Dimensions", inputline );
            LoadFIZ_Dimensions( inputline );
            continue;
        }

        if( issection( "Wheels:", inputline ) )
        {
			startBPT = false;
            fizlines.emplace( "Wheels", inputline );
            LoadFIZ_Wheels( inputline );
            continue;
        }

        if( issection( "Brake:", inputline ) )
        {
			startBPT = false;
            fizlines.emplace( "Brake", inputline );
            LoadFIZ_Brake( inputline );
            continue;
        }

        if( issection( "Doors:", inputline ) ) {

            startBPT = false;
            fizlines.emplace( "Doors", inputline );
            LoadFIZ_Doors( inputline );
            continue;
        }

        if( issection( "BuffCoupl.", inputline ) ) {

            startBPT = false;
            fizlines.emplace( "BuffCoupl", inputline );
            LoadFIZ_BuffCoupl( inputline, 0 );
            continue;
        }

        else if( issection( "BuffCoupl1.", inputline ) ) {

            startBPT = false;
            fizlines.emplace( "BuffCoupl1", inputline );
            LoadFIZ_BuffCoupl( inputline, 1 );
            continue;
        }
            
        else if( issection( "BuffCoupl2.", inputline ) ) {

            startBPT = false;
            fizlines.emplace( "BuffCoupl2", inputline );
            LoadFIZ_BuffCoupl( inputline, 2 );
            continue;
        }

        if( issection( "TurboPos:", inputline ) ) {

            startBPT = false;
            fizlines.emplace( "TurboPos", inputline );
            LoadFIZ_TurboPos( inputline );
            continue;
        }

        if( issection( "Cntrl.", inputline ) ) {

            startBPT = true; LISTLINE = 0;
            fizlines.emplace( "Cntrl", inputline );
            LoadFIZ_Cntrl( inputline );
            continue;
        }

        if( issection( "Light:", inputline ) ) {

            startBPT = false;
            fizlines.emplace( "Light", inputline );
            LoadFIZ_Light( inputline );
            continue;
        }

        if( issection( "Security:", inputline ) )
        {
			startBPT = false;
            fizlines.emplace( "Security", inputline );
            LoadFIZ_Security( inputline );
            continue;
        }

        if( issection( "Clima:", inputline ) ) {

            startBPT = false;
            fizlines.emplace( "Clima", inputline );
            LoadFIZ_Clima( inputline );
            continue;
        }

        if( issection( "Power:", inputline ) )
        {
			startBPT = false;
            fizlines.emplace( "Power", inputline );
            LoadFIZ_Power( inputline );
            continue;
        }

        if( issection( "Engine:", inputline ) )
        {
			startBPT = false;
            fizlines.emplace( "Engine", inputline );
            LoadFIZ_Engine( inputline );
			continue;
        }

        if( issection( "Switches:", inputline ) ) {
            startBPT = false;
            fizlines.emplace( "Switches", inputline );
            LoadFIZ_Switches( inputline );
            continue;
        }

        if( issection( "MotorParamTable:", inputline ) ) {
            startBPT = false;
            startMPT = true; LISTLINE = 0;
            fizlines.emplace( "MotorParamTable", inputline );
            LoadFIZ_MotorParamTable( inputline );
            continue;
        }

        if( issection( "MotorParamTable0:", inputline ) ) {
            startBPT = false;
            startMPT0 = true; LISTLINE = 0;
            continue;
        }

        if( issection( "Circuit:", inputline ) )
        {
			startBPT = false;
            fizlines.emplace( "Circuit", inputline );
            LoadFIZ_Circuit( inputline );
            continue;
        }

        if( issection( "RList:", inputline ) )
        {
			startBPT = false;
            fizlines.emplace( "RList", inputline );
            startRLIST = true; LISTLINE = 0;
            LoadFIZ_RList( inputline );
            continue;
        }

        if( issection( "DList:", inputline ) )
        {
			startBPT = false;
            fizlines.emplace( "DList", inputline );
            startDLIST = true; LISTLINE = 0;
            LoadFIZ_DList( inputline );
            continue;
        }

        if( issection( "ffList:", inputline ) ) {
			startBPT = false;
            startFFLIST = true; LISTLINE = 0;
            LoadFIZ_FFList( inputline );
            continue;
        }

        if( issection( "WWList:", inputline ) )
        {
			startBPT = false;
            startWWLIST = true; LISTLINE = 0;
			continue;
        }

        if( issection( "LightsList:", inputline ) ) {
            startBPT = false;
            fizlines.emplace( "LightsList", inputline );
            startLIGHTSLIST = true; LISTLINE = 0;
            LoadFIZ_LightsList( inputline );
            continue;
        }

        // ...and finally, table parsers.
        // NOTE: once table parsing is enabled it lasts until switched off, when another section is recognized
        if( true == startBPT ) {
            readBPT( inputline );
            continue;
        }
        if( true == startMPT ) {
            readMPT( inputline );
            continue;
        }
        if( true == startMPT0 ) {
            readMPT0( inputline );
            continue;
        }
        if( true == startRLIST ) {
            readRList( inputline );
            continue;
        }
        if( true == startDLIST ) {
            readDList( inputline );
            continue;
        }
        if( true == startFFLIST ) {
            readFFList( inputline );
            continue;
		}
        if( true == startWWLIST ) {
            readWWList( inputline );
			continue;
		}
        if( true == startLIGHTSLIST ) {
            readLightsList( inputline );
            continue;
        }
    } // while line
    in.close();

    // Operacje na zebranych parametrach - przypisywanie do wlasciwych zmiennych i ustawianie
    // zaleznosci

    bool result;
    if (ConversionError == 0)
        result = true;
    else
        result = false;

    WriteLog("CERROR: " + to_string(ConversionError) + ", SUCCES: " + to_string(result));
    return result;
}

void TMoverParameters::LoadFIZ_Param( std::string const &line ) {

    extract_value( Mass, "M", line, "0" );
    extract_value( Mred, "Mred", line, "0" );
    extract_value( Vmax, "Vmax", line, "0" );
    extract_value( Power, "PWR", line, "0" );
    extract_value( SandCapacity, "SandCap", line, "0" );
    extract_value( HeatingPower, "HeatingP", line, "0" );
    extract_value( LightPower, "LightP", line, "0" );

    {
        std::map<std::string, int> categories{
            { "train", 1 },
            { "road", 2 },
            { "unimog", 3 },
            { "ship", 4 },
            { "airplane,", 8 }
        };
        std::string category; extract_value( category, "Category", line, "none" );
        auto lookup = categories.find( category );
        CategoryFlag = (
            lookup != categories.end() ?
            lookup->second :
            0 );
        if( CategoryFlag == 0 ) {
            ErrorLog( "Unknown vehicle category: \"" + category + "\"." );
        }
    }

    {
        std::map<std::string, int> types{
            { "pseudodiesel", dt_PseudoDiesel },
            { "ezt", dt_EZT },
            { "sn61", dt_SN61 },
            { "et22", dt_ET22 },
            { "et40", dt_ET40 },
            { "et41", dt_ET41 },
            { "et42", dt_ET42 },
            { "ep05", dt_EP05 },
            { "181", dt_181 },
            { "182", dt_181 } // na razie tak
        };
        std::string type; extract_value( type, "Type", line, "none" );
        auto lookup = types.find( ToLower( type ) );
        TrainType = (
            lookup != types.end() ?
            lookup->second :
            dt_Default );
    }

    if( TrainType == dt_EZT ) {

        IminLo = 1;
        IminHi = 2;
        Imin = 1;
    }
}

void TMoverParameters::LoadFIZ_Load( std::string const &line ) {

    extract_value( LoadAccepted, "LoadAccepted", line, "" );
    if( true == LoadAccepted.empty() ) {
        return;
    }
    LoadAccepted = ToLower( LoadAccepted );

    extract_value( MaxLoad, "MaxLoad", line, "" );
    extract_value( LoadQuantity, "LoadQ", line, "" );
    extract_value( OverLoadFactor, "OverLoadFactor", line, "" );
    extract_value( LoadSpeed, "LoadSpeed", line, "" );
    extract_value( UnLoadSpeed, "UnLoadSpeed", line, "" );
}

void TMoverParameters::LoadFIZ_Dimensions( std::string const &line ) {

    extract_value( Dim.L, "L", line, "" );
    extract_value( Dim.H, "H", line, "" );
    extract_value( Dim.W, "W", line, "" );
    extract_value( Cx, "Cx", line, "0.3" );
    if( Dim.H <= 2.0 ) {
        //gdyby nie było parametru, lepsze to niż zero
        Floor = Dim.H;
    }
    else {
        //zgodność wsteczna
        Floor = 0.0;
    }
    extract_value( Floor, "Floor", line, "" );
}

void TMoverParameters::LoadFIZ_Wheels( std::string const &line ) {

    extract_value( WheelDiameter, "D", line, "" );
    WheelDiameterL = WheelDiameter; //gdyby nie było parametru, lepsze to niż zero
    extract_value( WheelDiameterL, "Dl", line, "" );
    WheelDiameterT = WheelDiameter; //gdyby nie było parametru, lepsze to niż zero
    extract_value( WheelDiameterT, "Dt", line, "" );

    extract_value( TrackW, "Tw", line, "" );
    extract_value( AxleInertialMoment, "AIM", line, "" );
    if( AxleInertialMoment <= 0.0 ) { AxleInertialMoment = 1.0; }

    extract_value( AxleArangement, "Axle", line, "" );
    NPoweredAxles = s2NPW( AxleArangement );
    NAxles = NPoweredAxles + s2NNW( AxleArangement );

    BearingType =
        ( extract_value( "BearingType", line ) == "Roll" ) ?
        1 :
        0;

    extract_value( ADist, "Ad", line, "" );
    extract_value( BDist, "Bd", line, "" );
}

void TMoverParameters::LoadFIZ_Brake( std::string const &line ) {

    extract_value( BrakeValveParams, "BrakeValve", line, "" );
    BrakeValveDecode( BrakeValveParams );
    BrakeSubsystemDecode();

    extract_value( NBpA, "NBpA", line, "" );
    extract_value( MaxBrakeForce, "MBF", line, "" );
    extract_value( BrakeValveSize, "Size", line, "" );
    extract_value( TrackBrakeForce, "TBF", line, "" ); TrackBrakeForce *= 1000.0;

    extract_value( MaxBrakePress[ 3 ], "MaxBP", line, "" );
    if( MaxBrakePress[ 3 ] > 0.0 ) {

        extract_value( BrakeCylNo, "BCN", line, "" );
        if( BrakeCylNo > 0 ) {

            extract_value( MaxBrakePress[ 0 ], "MaxLBP", line, "" );
            if( MaxBrakePress[ 0 ] < 0.01 ) { MaxBrakePress[ 0 ] = MaxBrakePress[ 3 ]; }
            extract_value( MaxBrakePress[ 1 ], "TareMaxBP", line, "" );
            extract_value( MaxBrakePress[ 2 ], "MedMaxBP", line, "" );
            extract_value( MaxBrakePress[ 4 ], "MaxASBP", line, "" );
            if( MaxBrakePress[ 4 ] < 0.01 ) { MaxBrakePress[ 4 ] = 0.0; }

            extract_value( BrakeCylRadius, "BCR", line, "" );
            extract_value( BrakeCylDist, "BCD", line, "" );
            extract_value( BrakeCylSpring, "BCS", line, "" );
            extract_value( BrakeSlckAdj, "BSA", line, "" );
            extract_value( BrakeRigEff, "BRE", line, "1" );

            extract_value( BrakeCylMult[ 0 ], "BCM", line, "" );
            extract_value( BrakeCylMult[ 1 ], "BCMlo", line, "" );
            extract_value( BrakeCylMult[ 2 ], "BCMHi", line, "" );

            P2FTrans = 100 * M_PI * std::pow( BrakeCylRadius, 2 ); // w kN/bar

            if( ( BrakeCylMult[ 1 ] > 0.0 ) || ( MaxBrakePress[ 1 ] > 0.0 ) ) { LoadFlag = 1; }
            else { LoadFlag = 0; }

            BrakeVolume = M_PI * std::pow( BrakeCylRadius, 2 ) * BrakeCylDist * BrakeCylNo;
            extract_value( BrakeVVolume, "BVV", line, "" );

            {
                std::map<std::string, int> brakemethods{
                    { "P10-Bg", bp_P10Bg },
                    { "P10-Bgu", bp_P10Bgu },
                    { "FR513", bp_FR513 },
                    { "FR510", bp_FR510 },
                    { "Cosid", bp_Cosid },
                    { "P10yBg", bp_P10yBg },
                    { "P10yBgu", bp_P10yBgu },
                    { "Disk1", bp_D1 },
                    { "Disk1+Mg", bp_D1 + bp_MHS },
                    { "Disk2", bp_D2 }
                };
                auto lookup = brakemethods.find( extract_value( "BM", line ) );
                BrakeMethod =
                    lookup != brakemethods.end() ?
                    lookup->second :
                    0;
            }

            extract_value( RapidMult, "RM", line, "1" );
        }
    }
    else {
        // maxbrakepress[3] == 0 or less
        P2FTrans = 0;
    }

    CntrlPipePress = 5 + 0.001 * ( Random( 10 ) - Random( 10 ) ); //Ra 2014-07: trochę niedokładności
    extract_value( CntrlPipePress, "HiPP", line, "" );
    HighPipePress = CntrlPipePress;
    LowPipePress = std::min( HighPipePress, 3.5 );
    extract_value( LowPipePress, "LoPP", line, "" );
    DeltaPipePress = HighPipePress - LowPipePress;

    extract_value( VeselVolume, "Vv", line, "" );
    if( VeselVolume == 0.0 ) { VeselVolume = 0.01; }

    extract_value( MinCompressor, "MinCP", line, "" );
    extract_value( MaxCompressor, "MaxCP", line, "" );
    extract_value( CompressorSpeed, "CompressorSpeed", line, "" );
    {
        std::map<std::string, int> compressorpowers{
            { "Converter", 2 },
            { "Engine", 3 },
            { "Coupler1", 4 },//włączana w silnikowym EZT z przodu
            { "Coupler2", 5 },//włączana w silnikowym EZT z tyłu
            { "Main", 0 }
        };
        auto lookup = compressorpowers.find( extract_value( "CompressorPower", line ) );
        CompressorPower =
            lookup != compressorpowers.end() ?
            lookup->second :
            1;
    }
}

void TMoverParameters::LoadFIZ_Doors( std::string const &line ) {

    DoorOpenCtrl = 0;
    std::string openctrl; extract_value( openctrl, "OpenCtrl", line, "" );
    if( openctrl == "DriverCtrl" ) { DoorOpenCtrl = 1; }

    DoorCloseCtrl = 0;
    std::string closectrl; extract_value( closectrl, "CloseCtrl", line, "" );
    if( closectrl == "DriverCtrl" ) { DoorCloseCtrl = 1; }
    else if( closectrl == "AutomaticCtrl" ) { DoorCloseCtrl = 2; }

    if( DoorCloseCtrl == 2 ) { extract_value( DoorStayOpen, "DoorStayOpen", line, "" ); }

    extract_value( DoorOpenSpeed, "OpenSpeed", line, "" );
    extract_value( DoorCloseSpeed, "CloseSpeed", line, "" );
    extract_value( DoorMaxShiftL, "DoorMaxShiftL", line, "" );
    extract_value( DoorMaxShiftR, "DoorMaxShiftR", line, "" );

    DoorOpenMethod = 2; //obrót, default
    std::string openmethod; extract_value( openmethod, "DoorOpenMethod", line, "" );
    if( openmethod == "Shift" ) { DoorOpenMethod = 1; } //przesuw
    else if( openmethod == "Fold" ) { DoorOpenMethod = 3; } //3 submodele się obracają
    else if( openmethod == "Plug" ) { DoorOpenMethod = 4; } //odskokowo-przesuwne

    std::string closurewarning; extract_value( closurewarning, "DoorClosureWarning", line, "" );
    DoorClosureWarning = ( closurewarning == "Yes" );

    std::string doorblocked; extract_value( doorblocked, "DoorBlocked", line, "" );
    DoorBlocked = ( doorblocked == "Yes" );

    extract_value( DoorMaxPlugShift, "DoorMaxShiftPlug", line, "" );
    extract_value( PlatformSpeed, "PlatformSpeed", line, "" );
    extract_value( PlatformMaxShift, "PlatformMaxSpeed", line, "" );

    PlatformOpenMethod = 2; // obrót, default
    std::string platformopenmethod; extract_value( platformopenmethod, "PlatformOpenMethod", line, "" );
    if( platformopenmethod == "Shift" ) { PlatformOpenMethod = 1; } // przesuw
}

void TMoverParameters::LoadFIZ_BuffCoupl( std::string const &line, int const Index ) {

    TCoupling *coupler;
    if( Index == 2 ) { coupler = &Couplers[ 1 ]; }
    else             { coupler = &Couplers[ 0 ]; }

    std::map<std::string, TCouplerType> couplertypes {
        { "Automatic", Automatic },
        { "Screw", Screw },
        { "Chain", Chain },
        { "Bare", Bare },
        { "Articulated", Articulated },
    };
    auto lookup = couplertypes.find( extract_value( "CType", line ) );
    coupler->CouplerType = (
        lookup != couplertypes.end() ?
        lookup->second :
        NoCoupler );

    extract_value( coupler->SpringKC, "kC", line, "" );
    extract_value( coupler->DmaxC, "DmaxC", line, "" );
    extract_value( coupler->FmaxC, "FmaxC", line, "" );
    extract_value( coupler->SpringKB, "kB", line, "" );
    extract_value( coupler->DmaxB, "DmaxB", line, "" );
    extract_value( coupler->FmaxB, "FmaxB", line, "" );
    extract_value( coupler->beta, "beta", line, "" );
    extract_value( coupler->AllowedFlag, "AllowedFlag", line, "" );

    if( coupler->AllowedFlag < 0 ) {

        coupler->AllowedFlag = ( ( -coupler->AllowedFlag ) | ctrain_depot );
    }

    if( ( coupler->CouplerType != NoCoupler )
     && ( coupler->CouplerType != Bare )
     && ( coupler->CouplerType != Articulated ) ) {

        coupler->SpringKC *= 1000;
        coupler->FmaxC *= 1000;
        coupler->SpringKB *= 1000;
        coupler->FmaxB *= 1000;
    }
    else if( coupler->CouplerType == Bare ) {

        coupler->SpringKC = 50.0 * Mass + Ftmax / 0.05;
        coupler->DmaxC = 0.05;
        coupler->FmaxC = 100.0 * Mass + 2 * Ftmax;
        coupler->SpringKB = 60.0 * Mass + Ftmax / 0.05;
        coupler->DmaxB = 0.05;
        coupler->FmaxB = 50.0 * Mass + 2.0 * Ftmax;
        coupler->beta = 0.3;
    }
    else if( coupler->CouplerType == Articulated ) {

        coupler->SpringKC = 60.0 * Mass + 1000;
        coupler->DmaxC = 0.05;
        coupler->FmaxC = 20000000.0 + 2.0 * Ftmax;
        coupler->SpringKB = 70.0 * Mass + 1000;
        coupler->DmaxB = 0.05;
        coupler->FmaxB = 4000000.0 + 2.0 * Ftmax;
        coupler->beta = 0.55;
    }

    if( Index == 0 ) {
        // 0 indicates single entry for both couplers
        Couplers[ 1 ] = Couplers[ 0 ];
    }
}

void TMoverParameters::LoadFIZ_TurboPos( std::string const &Input ) {

    extract_value( TurboTest, "TurboPos", Input, "" );
}

void TMoverParameters::LoadFIZ_Cntrl( std::string const &line ) {

    {
        std::map<std::string, TBrakeSystem> brakesystems{
            { "Pneumatic", Pneumatic },
            { "ElectroPneumatic", ElectroPneumatic }
        };
        auto lookup = brakesystems.find( extract_value( "BrakeSystem", line ) );
        BrakeSystem =
            lookup != brakesystems.end() ?
            lookup->second :
            Individual;
    }
    if( BrakeSystem != Individual ) {

        extract_value( BrakeCtrlPosNo, "BCPN", line, "" );
        for( int idx = 0; idx < 4; ++idx ) {

            extract_value( BrakeDelay[ idx ], "BDelay" + std::to_string( idx + 1 ), line, "" );
        }
        // brakedelays, brakedelayflag
        {
            std::map<std::string, int> brakedelays{
                { "GPR", bdelay_G + bdelay_P + bdelay_R },
                { "PR", bdelay_P + bdelay_R },
                { "GP", bdelay_G + bdelay_P },
                { "R", bdelay_R },
                { "P", bdelay_P },
                { "G", bdelay_G },
                { "GPR+Mg", bdelay_G + bdelay_P + bdelay_R + bdelay_M },
                { "PR+Mg", bdelay_P + bdelay_R + bdelay_M }
            };
            std::map<std::string, int> brakedelayflags{
                { "R", bdelay_R },
                { "P", bdelay_P },
                { "G", bdelay_G }
            };
            std::string brakedelay;
            extract_value( brakedelay, "BrakeDelays", line, "" );
            auto lookup = brakedelays.find( brakedelay );
            BrakeDelays =
                lookup != brakedelays.end() ?
                lookup->second :
                Individual;
            lookup = brakedelayflags.find( brakedelay );
            BrakeDelayFlag =
                lookup != brakedelayflags.end() ?
                lookup->second :
                0;
        }
        // brakeopmode
        {
            std::map<std::string, int> brakeopmodes{
                { "PN", bom_PS + bom_PN },
                { "PNEPMED", bom_PS + bom_PN + bom_EP + bom_MED }
            };
            auto lookup = brakeopmodes.find( extract_value( "BrakeOpModes", line ) );
            BrakeOpModes =
                lookup != brakeopmodes.end() ?
                lookup->second :
                0;
        }
        // brakehandle
        {
            std::map<std::string, TBrakeHandle> brakehandles{
                { "FV4a", FV4a },
                { "test", testH },
                { "D2", D2 },
                { "MHZ_EN57", MHZ_EN57 },
                { "M394", M394 },
                { "Knorr", Knorr },
                { "Westinghouse", West },
                { "FVel6", FVel6 },
                { "St113", St113 }
            };
            auto lookup = brakehandles.find( extract_value( "BrakeHandle", line ) );
            BrakeHandle =
                lookup != brakehandles.end() ?
                lookup->second :
                NoHandle;
        }
        // brakelochandle
        {
            std::map<std::string, TBrakeHandle> locbrakehandles{
                { "FD1", FD1 },
                { "Knorr", Knorr },
                { "Westinghouse", West }
            };
            auto lookup = locbrakehandles.find( extract_value( "LocBrakeHandle", line ) );
            BrakeLocHandle =
                lookup != locbrakehandles.end() ?
                lookup->second :
                NoHandle;
        }

        // mbpm
        extract_value( MBPM, "MaxBPMass", line, "" );
//        MBPM *= 1000;

        // asbtype
        std::string asb;
        extract_value( asb, "ASB", line, "" );
        if( BrakeCtrlPosNo > 0 ) {

            if( asb == "Manual" ) { ASBType = 1; }
            else if( asb == "Automatic" ) { ASBType = 2; }
        }
        else {

            if( asb == "Yes" ) { ASBType = 128; }
        }
    } // brakesystem != individual

    // localbrake
    {
        std::map<std::string, TLocalBrake> localbrakes{
            { "ManualBrake", ManualBrake },
            { "PneumaticBrake", PneumaticBrake },
            { "HydraulicBrake", HydraulicBrake }
        };
        auto lookup = localbrakes.find( extract_value( "LocalBrake", line ) );
        LocalBrake =
            lookup != localbrakes.end() ?
            lookup->second :
            NoBrake;
    }
    // mbrake
    MBrake = ( extract_value( "ManualBrake", line ) == "Yes" );
    // dynamicbrake
    {
        std::map<std::string, int> dynamicbrakes{
            { "Passive", dbrake_passive },
            { "Switch", dbrake_switch },
            { "Reversal", dbrake_reversal },
            { "Automatic", dbrake_automatic }
        };
        auto lookup = dynamicbrakes.find( extract_value( "DynamicBrake", line ) );
        DynamicBrakeType =
            lookup != dynamicbrakes.end() ?
            lookup->second :
            dbrake_none;
    }

    extract_value( MainCtrlPosNo, "MCPN", line, "" );
    extract_value( ScndCtrlPosNo, "SCPN", line, "" );
    extract_value( ScndInMain, "SCIM", line, "" );

    std::string autorelay;
    extract_value( autorelay, "AutoRelay", line, "" );
    if( autorelay == "Optional" ) { AutoRelayType = 2; }
    else if( autorelay == "Yes" ) { AutoRelayType = 1; }
    else { AutoRelayType = 0; }

    CoupledCtrl = ( extract_value( "CoupledCtrl", line ) == "Yes" );

    ScndS = ( extract_value( "ScndS", line ) == "Yes" ); // brak pozycji rownoleglej przy niskiej nastawie PSR

    extract_value( InitialCtrlDelay, "IniCDelay", line, "" );
    extract_value( CtrlDelay, "SCDelay", line, "" );
    CtrlDownDelay = CtrlDelay; //hunter-101012: jesli nie ma SCDDelay;
    extract_value( CtrlDownDelay, "SCDDelay", line, "" );

    //hunter-111012: dla siodemek 303E
    FastSerialCircuit =
        ( extract_value( "FSCircuit", line ) == "Yes" ) ?
        1 :
        0;

    extract_value( StopBrakeDecc, "SBD", line, "" );
}

void TMoverParameters::LoadFIZ_Light( std::string const &line ) {

    LightPowerSource.SourceType = LoadFIZ_SourceDecode( extract_value( "Light", line ) );
    LoadFIZ_PowerParamsDecode( LightPowerSource, "L", line );

    AlterLightPowerSource.SourceType = LoadFIZ_SourceDecode( extract_value( "AlterLight", line ) );
    LoadFIZ_PowerParamsDecode( AlterLightPowerSource, "AlterL", line );

    extract_value( NominalVoltage, "Volt", line, "" );
    extract_value( BatteryVoltage, "LMaxVoltage", line, "" );
    NominalBatteryVoltage = BatteryVoltage;
}

void TMoverParameters::LoadFIZ_Security( std::string const &line ) {

    std::string awaresystem = extract_value( "AwareSystem", line );
    if( awaresystem.find( "Active" ) != std::string::npos ) {
        SetFlag( SecuritySystem.SystemType, 1 );
    }
    if( awaresystem.find( "CabSignal" ) != std::string::npos ) {
        SetFlag( SecuritySystem.SystemType, 2 );
    }

    extract_value( SecuritySystem.AwareDelay, "AwareDelay", line, "" );
    SecuritySystem.AwareMinSpeed = 0.1 * Vmax; //domyślnie 10% Vmax
    extract_value( SecuritySystem.AwareMinSpeed, "AwareMinSpeed", line, "" );
    extract_value( SecuritySystem.SoundSignalDelay, "SoundSignalDelay", line, "" );
    extract_value( SecuritySystem.EmergencyBrakeDelay, "EmergencyBrakeDelay", line, "" );
    SecuritySystem.RadioStop = ( extract_value( "RadioStop", line ).find( "Yes" ) != std::string::npos );
}

void TMoverParameters::LoadFIZ_Clima( std::string const &line ) {

    HeatingPowerSource.SourceType = LoadFIZ_SourceDecode( extract_value( "Heating", line ) );
    LoadFIZ_PowerParamsDecode( HeatingPowerSource, "H", line );
    AlterHeatPowerSource.SourceType = LoadFIZ_SourceDecode( extract_value( "AlterHeating", line ) );
    LoadFIZ_PowerParamsDecode( AlterHeatPowerSource, "AlterH", line );
}

void TMoverParameters::LoadFIZ_Power( std::string const &Line ) {

    EnginePowerSource.SourceType = LoadFIZ_SourceDecode( extract_value( "EnginePower", Line ) );
    LoadFIZ_PowerParamsDecode( EnginePowerSource, "", Line );

    if( ( EnginePowerSource.SourceType == Generator )
     && ( EnginePowerSource.GeneratorEngine == WheelsDriven ) ) {
        // perpetuum mobile?
        ConversionError = 666;
    }
    if( Power == 0.0 ) {
        //jeśli nie ma mocy, np. rozrządcze EZT
        EnginePowerSource.SourceType = NotDefined;
    }

    SystemPowerSource.SourceType = LoadFIZ_SourceDecode( extract_value( "SystemPower", Line ) );
    LoadFIZ_PowerParamsDecode( SystemPowerSource, "", Line );
}

void TMoverParameters::LoadFIZ_Engine( std::string const &Input ) {

    EngineType = LoadFIZ_EngineDecode( extract_value( "EngineType", Input ) );

    std::string transmission = extract_value( "Trans", Input );
    if( false == transmission.empty() ) {
        // transmission type. moved here because more than one engine type has this entry
        auto ratios = Split( transmission, ':' ); // e.g. 18:79

        if( ratios.size() != 2 ) {
            ErrorLog( "Wrong transmition definition: " + transmission );
        }

        Transmision.NToothM = std::atoi( ratios[0].c_str() ); // ToothM to pierwszy czyli 18
        Transmision.NToothW = std::atoi( ratios[1].c_str() ); // ToothW to drugi parametr czyli 79

        if( Transmision.NToothM > 0 )
            Transmision.Ratio = static_cast<double>( Transmision.NToothW ) / Transmision.NToothM;
        else
            Transmision.Ratio = 1.0;
    }

    switch( EngineType ) {

        case ElectricSeriesMotor: {

            extract_value( NominalVoltage, "Volt", Input, "" );
            extract_value( WindingRes, "WindingRes", Input, "" );
            if( WindingRes == 0.0 ) {

                WindingRes = 0.01;
            }
            extract_value( nmax, "nmax", Input, "" );
            nmax /= 60.0;
            break;
        }
        case WheelsDriven:
        case Dumb: {

            extract_value( Ftmax, "Ftmax", Input, "" );
            break;
        }
        case DieselEngine: {

            extract_value( dizel_nmin, "nmin", Input, "" );
            dizel_nmin /= 60.0;
            extract_value( dizel_nmax, "nmax", Input, "" );
            dizel_nmax /= 60.0; 
            nmax = dizel_nmax; // not sure if this is needed, but just in case
            extract_value( dizel_nmax_cutoff, "nmax_cutoff", Input, "0.0" );
            dizel_nmax_cutoff /= 60.0;
            extract_value( dizel_AIM, "AIM", Input, "1.0" );

            if( true == extract_value( AnPos, "ShuntMode", Input, "" ) ) {
                //dodatkowa przekładnia dla SM03 (2Ls150)
                ShuntModeAllow = true;
                ShuntMode = false;
                if( AnPos < 1.0 ) {
                    //"rozruch wysoki" ma dawać większą siłę
                    AnPos = 1.0 / AnPos; //im większa liczba, tym wolniej jedzie
                }
            }
            break;
        }
        case DieselElectric: { //youBy

            extract_value( Ftmax, "Ftmax", Input, "" );
            Flat = ( extract_value( "Flat", Input ) == "1" );
            extract_value( Vhyp, "Vhyp", Input, "" );
            Vhyp /= 3.6;
            extract_value( Vadd, "Vadd", Input, "" );
            Vadd /= 3.6;
            extract_value( PowerCorRatio, "Cr", Input, "" );
            extract_value( RelayType, "RelayType", Input, "" );
            if( extract_value( "ShuntMode", Input ) == "1" ) {

                ShuntModeAllow = true;
                ShuntMode = false;
                AnPos = 0.0;
                ImaxHi = 2;
                ImaxLo = 1;
            }
            break;
        }
        case ElectricInductionMotor: {

            RVentnmax = 1.0;
            extract_value( NominalVoltage, "Volt", Input, "" );

            extract_value( eimc[ eimc_s_dfic ], "dfic", Input, "" );
            extract_value( eimc[ eimc_s_dfmax ], "dfmax", Input, "" );
            extract_value( eimc[ eimc_s_p ], "p", Input, "" );
            extract_value( eimc[ eimc_s_cfu ], "cfu", Input, "" );
            extract_value( eimc[ eimc_s_cim ], "cim", Input, "" );
            extract_value( eimc[ eimc_s_icif ], "icif", Input, "" );
            extract_value( eimc[ eimc_f_Uzmax ], "Uzmax", Input, "" );
            extract_value( eimc[ eimc_f_Uzh ], "Uzh", Input, "" );
            extract_value( eimc[ eimc_f_DU ], "DU", Input, "" );
            extract_value( eimc[ eimc_f_I0 ], "I0", Input, "" );
            extract_value( eimc[ eimc_f_cfu ], "fcfu", Input, "" );
            extract_value( eimc[ eimc_p_F0 ], "F0", Input, "" );
            extract_value( eimc[ eimc_p_a1 ], "a1", Input, "" );
            extract_value( eimc[ eimc_p_Pmax ], "Pmax", Input, "" );
            extract_value( eimc[ eimc_p_Fh ], "Fh", Input, "" );
            extract_value( eimc[ eimc_p_Ph ], "Ph", Input, "" );
            extract_value( eimc[ eimc_p_Vh0 ], "Vh0", Input, "" );
            extract_value( eimc[ eimc_p_Vh1 ], "Vh1", Input, "" );
            extract_value( eimc[ eimc_p_Imax ], "Imax", Input, "" );
            extract_value( eimc[ eimc_p_abed ], "abed", Input, "" );
            extract_value( eimc[ eimc_p_eped ], "edep", Input, "" );

            Flat = ( extract_value( "Flat", Input ) == "1" );

            break;
        }
        default: {
            // nothing here
        }
    }
}

void TMoverParameters::LoadFIZ_Switches( std::string const &Input ) {

    extract_value( PantSwitchType, "Pantograph", Input, "" );
    extract_value( ConvSwitchType, "Converter", Input, "" );
}

void TMoverParameters::LoadFIZ_MotorParamTable( std::string const &Input ) {

    switch( EngineType ) {

        case DieselEngine: {
            
            extract_value( dizel_minVelfullengage, "minVelfullengage", Input, "" );
            extract_value( dizel_engageDia, "engageDia", Input, "" );
            extract_value( dizel_engageMaxForce, "engageMaxForce", Input, "" );
            extract_value( dizel_engagefriction, "engagefriction", Input, "" );
            break;
        }
        default: {
            // nothing here
        }
    }
}

void TMoverParameters::LoadFIZ_Circuit( std::string const &Input ) {

    extract_value( CircuitRes, "CircuitRes", Input, "" );
    extract_value( IminLo, "IminLo", Input, "" );
    extract_value( IminHi, "IminHi", Input, "" );
    extract_value( ImaxLo, "ImaxLo", Input, "" );
    extract_value( ImaxHi, "ImaxHi", Input, "" );
    Imin = IminLo;
    Imax = ImaxLo;
}

void TMoverParameters::LoadFIZ_RList( std::string const &Input ) {

    auto const venttype = extract_value( "RVent", Input );
    if( venttype == "Automatic" ) {
    
        RVentType = 2;
    }
    else {

        RVentType =
            venttype == "Yes" ?
            1 :
            0;
    }

    if( RVentType > 0 ) {

        extract_value( RVentnmax, "RVentnmax", Input, "" );
        RVentnmax /= 60.0;
        extract_value( RVentCutOff, "RVentCutOff", Input, "" );
    }
}

void TMoverParameters::LoadFIZ_DList( std::string const &Input ) {

    extract_value( dizel_Mmax, "Mmax", Input, "" );
    extract_value( dizel_nMmax, "nMmax", Input, "" );
    extract_value( dizel_Mnmax, "Mnmax", Input, "" );
    extract_value( dizel_nmax, "nmax", Input, "" );
    extract_value( dizel_nominalfill, "nominalfill", Input, "" );
    extract_value( dizel_Mstand, "Mstand", Input, "" );
}

void TMoverParameters::LoadFIZ_FFList( std::string const &Input ) {

    extract_value( RlistSize, "Size", Input, "" );
}

void TMoverParameters::LoadFIZ_LightsList( std::string const &Input ) {

    extract_value( LightsPosNo, "Size", Input, "" );
    extract_value( LightsWrap, "Wrap", Input, "" );
    extract_value( LightsDefPos, "Default", Input, "" );
}

void TMoverParameters::LoadFIZ_PowerParamsDecode( TPowerParameters &Powerparameters, std::string const Prefix, std::string const &Line ) {

    switch( Powerparameters.SourceType ) {

        case NotDefined:
        case InternalSource: {
            
            Powerparameters.PowerType = LoadFIZ_PowerDecode( extract_value( Prefix + "PowerType", Line ) );
            break;
        }
        case Transducer: {
            
            extract_value( Powerparameters.InputVoltage, Prefix + "TransducerInputV", Line, "" );
            break;
        }
        case Generator: {

            Powerparameters.GeneratorEngine = LoadFIZ_EngineDecode( extract_value( Prefix + "GeneratorEngine", Line ) );
            break;
        }
        case Accumulator: {

            extract_value( Powerparameters.RAccumulator.MaxCapacity, Prefix + "Cap", Line, "" );
            Powerparameters.RAccumulator.RechargeSource = LoadFIZ_SourceDecode( extract_value( Prefix + "RS", Line ) );
            break;
        }
        case CurrentCollector: {

            auto &collectorparameters = Powerparameters.CollectorParameters;

            extract_value( collectorparameters.CollectorsNo, "CollectorsNo", Line, "" );
            extract_value( collectorparameters.MinH, "MinH", Line, "" );
            extract_value( collectorparameters.MaxH, "MaxH", Line, "" );
            extract_value( collectorparameters.CSW, "CSW", Line, "" ); //szerokość części roboczej
            extract_value( collectorparameters.MaxV, "MaxVoltage", Line, "" );
            collectorparameters.OVP = //przekaźnik nadnapięciowy
                extract_value( "OverVoltProt", Line ) == "Yes" ?
                1 :
                0;
            //napięcie rozłączające WS
            collectorparameters.MinV = 0.5 * collectorparameters.MaxV; //gdyby parametr nie podany
            extract_value( collectorparameters.MinV, "MinV", Line, "" );
            //napięcie wymagane do załączenia WS
            collectorparameters.InsetV = 0.6 * collectorparameters.MaxV; //gdyby parametr nie podany
            extract_value( collectorparameters.InsetV, "InsetV", Line, "" );
            //ciśnienie rozłączające WS
            extract_value( collectorparameters.MinPress, "MinPress", Line, "2.0" ); //domyślnie 2 bary do załączenia WS
            //maksymalne ciśnienie za reduktorem
            collectorparameters.MaxPress = 5.0 + 0.001 * ( Random( 50 ) - Random( 50 ) );
            extract_value( collectorparameters.MaxPress, "MaxPress", Line, "" );
            break;
        }
        case PowerCable: {

            Powerparameters.RPowerCable.PowerTrans = LoadFIZ_PowerDecode( extract_value( Prefix + "PowerTrans", Line ) );
            if( Powerparameters.RPowerCable.PowerTrans == SteamPower ) {

                extract_value( Powerparameters.RPowerCable.SteamPressure, Prefix + "SteamPress", Line, "" );
            }
            break;
        }
        case Heater: {
            //jeszcze nie skonczone!
            break;
        }
        default:
            ; // nothing here
    }

    if( ( Powerparameters.SourceType != Heater )
     && ( Powerparameters.SourceType != InternalSource ) ) {


        extract_value( Powerparameters.MaxVoltage, Prefix + "MaxVoltage", Line, "" );
        extract_value( Powerparameters.MaxCurrent, Prefix + "MaxCurrent", Line, "" );
        extract_value( Powerparameters.IntR, Prefix + "IntR", Line, "" );
    }
}

TPowerType TMoverParameters::LoadFIZ_PowerDecode( std::string const &Power ) {

    std::map<std::string, TPowerType> powertypes{
        { "BioPower", BioPower },
        { "MechPower", MechPower },
        { "ElectricPower", ElectricPower },
        { "SteamPower", SteamPower }
    };
    auto lookup = powertypes.find( Power );
    return
        lookup != powertypes.end() ?
        lookup->second :
        NoPower;
}

TPowerSource TMoverParameters::LoadFIZ_SourceDecode( std::string const &Source ) {

    std::map<std::string, TPowerSource> powersources{
        { "Transducer", Transducer },
        { "Generator", Generator },
        { "Accu", Accumulator },
        { "CurrentCollector", CurrentCollector },
        { "PowerCable", PowerCable },
        { "Heater", Heater },
        { "Internal", InternalSource }
    };
    auto lookup = powersources.find( Source );
    return
        lookup != powersources.end() ?
        lookup->second :
        NotDefined;
}

TEngineTypes TMoverParameters::LoadFIZ_EngineDecode( std::string const &Engine ) {

    std::map<std::string, TEngineTypes> enginetypes{
        { "ElectricSeriesMotor", ElectricSeriesMotor },
        { "DieselEngine", DieselEngine },
        { "SteamEngine", SteamEngine },
        { "WheelsDriven", WheelsDriven },
        { "Dumb", Dumb },
        { "DieselElectric", DieselElectric },
        { "DumbDE", DieselElectric },
        { "ElectricInductionMotor", ElectricInductionMotor }
    };
    auto lookup = enginetypes.find( Engine );
    return
        lookup != enginetypes.end() ?
        lookup->second :
        None;
}

// *************************************************************************************************
// Q: 20160717
// *************************************************************************************************

bool TMoverParameters::CheckLocomotiveParameters(bool ReadyFlag, int Dir)
{
	WriteLog("check locomotive parameters...");
	int b;
	bool OK = true;

	AutoRelayFlag = (AutoRelayType == 1);

	Sand = SandCapacity;

	// WriteLog("aa = " + AxleArangement + " " + std::string( Pos("o", AxleArangement)) );

    if( ( AxleArangement.find( "o" ) != std::string::npos ) && ( EngineType == ElectricSeriesMotor ) ) {
        // test poprawnosci ilosci osi indywidualnie napedzanych
        OK = ( ( RList[ 1 ].Bn * RList[ 1 ].Mn ) == NPoweredAxles );
        // WriteLogSS("aa ok", BoolToYN(OK));
    }

    if( ( LoadType.empty() == false ) && ( LoadAccepted.find( LoadType ) == std::string::npos ) ) {
        // remove load if the type isn't supported
        Load = 0.0;
    }

	if (BrakeSystem == Individual)
		if (BrakeSubsystem != ss_None)
			OK = false; //!

    if( ( BrakeVVolume == 0 ) && ( MaxBrakePress[ 3 ] > 0 ) && ( BrakeSystem != Individual ) ) {
        BrakeVVolume =
            MaxBrakePress[ 3 ] /
            ( 5.0 - MaxBrakePress[ 3 ] ) * ( BrakeCylRadius * BrakeCylRadius * BrakeCylDist * BrakeCylNo * M_PI ) * 1000;
    }
    if( BrakeVVolume == 0.0 ) {
        BrakeVVolume = 0.01;
    }
	// WriteLog("BVV = "  + FloatToStr(BrakeVVolume));


    switch( BrakeValve ) {
        case W:
        case K:
        {
            WriteLog( "XBT W, K" );
            Hamulec = std::make_shared<TWest>( MaxBrakePress[ 3 ], BrakeCylRadius, BrakeCylDist, BrakeVVolume, BrakeCylNo, BrakeDelays, BrakeMethod, NAxles, NBpA );
            if( MBPM < 2 ) // jesli przystawka wazaca
                Hamulec->SetLP( 0, MaxBrakePress[ 3 ], 0 );
            else
                Hamulec->SetLP( Mass, MBPM, MaxBrakePress[ 1 ] );
            break;
        }
        case KE:
        {
            WriteLog( "XBT WKE" );
            Hamulec = std::make_shared<TKE>( MaxBrakePress[ 3 ], BrakeCylRadius, BrakeCylDist, BrakeVVolume, BrakeCylNo, BrakeDelays, BrakeMethod, NAxles, NBpA );
            Hamulec->SetRM( RapidMult );
            if( MBPM < 2 ) // jesli przystawka wazaca
                Hamulec->SetLP( 0, MaxBrakePress[ 3 ], 0 );
            else
                Hamulec->SetLP( Mass, MBPM, MaxBrakePress[ 1 ] );
            break;
        }
        case NESt3:
        case ESt3:
        case ESt3AL2:
        case ESt4:
        {
            WriteLog( "XBT NESt3, ESt3, ESt3AL2, ESt4" );
            Hamulec = std::make_shared<TNESt3>( MaxBrakePress[ 3 ], BrakeCylRadius, BrakeCylDist, BrakeVVolume, BrakeCylNo, BrakeDelays, BrakeMethod, NAxles, NBpA );
            static_cast<TNESt3 *>( Hamulec.get() )->SetSize( BrakeValveSize, BrakeValveParams );
            if( MBPM < 2 ) // jesli przystawka wazaca
                Hamulec->SetLP( 0, MaxBrakePress[ 3 ], 0 );
            else
                Hamulec->SetLP( Mass, MBPM, MaxBrakePress[ 1 ] );
            break;
        }
        case LSt:
        {
            WriteLog( "XBT LSt" );
            Hamulec = std::make_shared<TLSt>( MaxBrakePress[ 3 ], BrakeCylRadius, BrakeCylDist, BrakeVVolume, BrakeCylNo, BrakeDelays, BrakeMethod, NAxles, NBpA );
            Hamulec->SetRM( RapidMult );
            break;
        }
        case EStED:
        {
            WriteLog( "XBT EStED" );
            Hamulec = std::make_shared<TEStED>( MaxBrakePress[ 3 ], BrakeCylRadius, BrakeCylDist, BrakeVVolume, BrakeCylNo, BrakeDelays, BrakeMethod, NAxles, NBpA );
            Hamulec->SetRM( RapidMult );
            if( MBPM < 2 ) {
                //jesli przystawka wazaca
                Hamulec->SetLP( 0, MaxBrakePress[ 3 ], 0 );
            }
            else {
                Hamulec->SetLP( Mass, MBPM, MaxBrakePress[ 1 ] );
            }
            break;
        }
        case EP2:
        {
            WriteLog( "XBT EP2" );
            Hamulec = std::make_shared<TEStEP2>( MaxBrakePress[ 3 ], BrakeCylRadius, BrakeCylDist, BrakeVVolume, BrakeCylNo, BrakeDelays, BrakeMethod, NAxles, NBpA );
            Hamulec->SetLP( Mass, MBPM, MaxBrakePress[ 1 ] );
            break;
        }
        case CV1:
        {
            WriteLog( "XBT CV1" );
            Hamulec = std::make_shared<TCV1>( MaxBrakePress[ 3 ], BrakeCylRadius, BrakeCylDist, BrakeVVolume, BrakeCylNo, BrakeDelays, BrakeMethod, NAxles, NBpA );
            break;
        }
        case CV1_L_TR:
        {
            WriteLog( "XBT CV1_L_T" );
            Hamulec = std::make_shared<TCV1L_TR>( MaxBrakePress[ 3 ], BrakeCylRadius, BrakeCylDist, BrakeVVolume, BrakeCylNo, BrakeDelays, BrakeMethod, NAxles, NBpA );
            break;
        }
        default:
            Hamulec = std::make_shared<TBrake>( MaxBrakePress[ 3 ], BrakeCylRadius, BrakeCylDist, BrakeVVolume, BrakeCylNo, BrakeDelays, BrakeMethod, NAxles, NBpA );
    }

    Hamulec->SetASBP( MaxBrakePress[ 4 ] );

    switch( BrakeHandle ) {
        case FV4a:
            Handle = std::make_shared<TFV4aM>();
            break;
        case MHZ_EN57:
            Handle = std::make_shared<TMHZ_EN57>();
            break;
        case FVel6:
            Handle = std::make_shared<TFVel6>();
            break;
        case testH:
            Handle = std::make_shared<Ttest>();
            break;
        case M394:
            Handle = std::make_shared<TM394>();
            break;
        case Knorr:
            Handle = std::make_shared<TH14K1>();
            break;
        case St113:
            Handle = std::make_shared<TSt113>();
            break;
        default:
            Handle = std::make_shared<TDriverHandle>();
    }

    switch( BrakeLocHandle ) {
        case FD1:
        {
            LocHandle = std::make_shared<TFD1>();
            LocHandle->Init( MaxBrakePress[ 0 ] );
            if( TrainType == dt_EZT ) {

                dynamic_cast<TFD1*>(LocHandle.get())->SetSpeed( 3.5 );
            }
            break;
        }
        case Knorr:
        {
            LocHandle = std::make_shared<TH1405>();
            LocHandle->Init( MaxBrakePress[ 0 ] );
            break;
        }
        default:
            LocHandle = std::make_shared<TDriverHandle>();
    }

	if ( ( true == TestFlag( BrakeDelays, bdelay_G ) )
      && ( ( false == TestFlag(BrakeDelays, bdelay_R ) )
        || ( Power > 1.0 ) ) ) // ustalanie srednicy przewodu glownego (lokomotywa lub napędowy
		Spg = 0.792;
	else
		Spg = 0.507;
    // WriteLog("SPG = " + FloatToStr(Spg));

    Pipe = std::make_shared<TReservoir>();
    Pipe2 = std::make_shared<TReservoir>(); // zabezpieczenie, bo sie PG wywala... :(
    Pipe->CreateCap( ( std::max( Dim.L, 14.0 ) + 0.5 ) * Spg * 1 ); // dlugosc x przekroj x odejscia i takie tam
    Pipe2->CreateCap( ( std::max( Dim.L, 14.0 ) + 0.5 ) * Spg * 1 );

    if( LightsPosNo > 0 )
        LightsPos = LightsDefPos;

    // checking ready flag
    // to dac potem do init
    if( ReadyFlag ) // gotowy do drogi
    {
        WriteLog( "Ready to depart" );
        CompressedVolume = VeselVolume * MinCompressor * ( 9.8 ) / 10.0;
        ScndPipePress = CompressedVolume / VeselVolume;
        PipePress = CntrlPipePress;
        BrakePress = 0.0;
        LocalBrakePos = 0;
        if( CabNo == 0 )
            BrakeCtrlPos = static_cast<int>( Handle->GetPos( bh_NP ) );
        else
            BrakeCtrlPos = static_cast<int>( Handle->GetPos( bh_RP ) );
        MainSwitch( false );
        PantFront( true );
        PantRear( true );
        MainSwitch( true );
        ActiveDir = 0; // Dir; //nastawnik kierunkowy - musi być ustawiane osobno!
        DirAbsolute = ActiveDir * CabNo; // kierunek jazdy względem sprzęgów
        LimPipePress = CntrlPipePress;
    }
    else { // zahamowany}
        WriteLog( "Braked" );
        Volume = BrakeVVolume * MaxBrakePress[ 3 ];
        CompressedVolume = VeselVolume * MinCompressor * 0.55;
        ScndPipePress = 5.1;
        PipePress = LowPipePress;
        PipeBrakePress = MaxBrakePress[ 3 ] * 0.5;
        BrakePress = MaxBrakePress[ 3 ] * 0.5;
        LocalBrakePos = 0;
        BrakeCtrlPos = static_cast<int>( Handle->GetPos( bh_NP ) );
        LimPipePress = LowPipePress;
    }

    ActFlowSpeed = 0.0;
    BrakeCtrlPosR = BrakeCtrlPos;

    if( BrakeLocHandle == Knorr )
        LocalBrakePos = 5;

    Pipe->CreatePress( PipePress );
    Pipe2->CreatePress( ScndPipePress );
    Pipe->Act();
    Pipe2->Act();

    EqvtPipePress = PipePress;

    Handle->Init( PipePress );

    ComputeConstans();

    if( LoadFlag > 0 ) {

        if( Load < MaxLoad * 0.45 ) {
            IncBrakeMult();
            IncBrakeMult();
            DecBrakeMult(); // TODO: przeinesiono do mover.cpp
            if( Load < MaxLoad * 0.35 )
                DecBrakeMult();
        }
        else {
            IncBrakeMult(); // TODO: przeinesiono do mover.cpp
            if( Load >= MaxLoad * 0.55 )
                IncBrakeMult();
        }
    }

	// taki mini automat - powinno byc ladnie dobrze :)
	BrakeDelayFlag = bdelay_P;
	if ((TestFlag(BrakeDelays, bdelay_G)) && !(TestFlag(BrakeDelays, bdelay_R)))
		BrakeDelayFlag = bdelay_G;
	if ((TestFlag(BrakeDelays, bdelay_R)) && !(TestFlag(BrakeDelays, bdelay_G)))
		BrakeDelayFlag = bdelay_R;

	if (BrakeOpModes & bom_PS)
		BrakeOpModeFlag = bom_PS;
	else
		BrakeOpModeFlag = bom_PN;

	// yB: jesli pojazdy nie maja zadeklarowanych czasow, to wsadz z przepisow +-16,(6)%
    int DefBrakeTable[8] = { 15, 4, 25, 25, 13, 3, 12, 2 };

    for( b = 1; b < 4; b++ )
	{
		if (BrakeDelay[b] == 0)
			BrakeDelay[b] = DefBrakeTable[b];
		BrakeDelay[b] = BrakeDelay[b] * (2.5 + Random(0.0, 0.2)) / 3.0;
	}

	if (TrainType == dt_ET22)
		CompressorPower = 0;

	Hamulec->Init(PipePress, HighPipePress, LowPipePress, BrakePress, BrakeDelayFlag);

	ScndPipePress = Compressor;

	// WriteLogSS("OK=", BoolTo10(OK));
	// WriteLog("");

	return OK;
}

// *************************************************************************************************
// Q: 20160714
// Wstawia komendę z parametrem, od sprzęgu i w lokalizacji do pojazdu
// *************************************************************************************************
void TMoverParameters::PutCommand(std::string NewCommand, double NewValue1, double NewValue2,
                                  const TLocation &NewLocation)
{
    CommandLast = NewCommand; // zapamiętanie komendy

    CommandIn.Command = NewCommand;
    CommandIn.Value1 = NewValue1;
    CommandIn.Value2 = NewValue2;
    CommandIn.Location = NewLocation;
    // czy uruchomic tu RunInternalCommand? nie wiem
}

// *************************************************************************************************
// Q: 20160714
// Pobiera komendę z parametru funkcji oraz wartość zmiennej jako return
// *************************************************************************************************
double TMoverParameters::GetExternalCommand(std::string &Command)
{
    Command = CommandOut;
    return ValueOut;
}

// *************************************************************************************************
// Q: 20160714
// GF: 20161117
// rozsyłanie komend do całego składu
// *************************************************************************************************
bool TMoverParameters::SendCtrlBroadcast(std::string CtrlCommand, double ctrlvalue)
{
    int b;
    bool OK;

    OK = ((CtrlCommand != CommandIn.Command) && (ctrlvalue != CommandIn.Value1));
    if (OK)
        for (b = 0; b < 2; b++)
            if (TestFlag(Couplers[b].CouplingFlag, ctrain_controll))
                if (Couplers[b].Connected->SetInternalCommand(CtrlCommand, ctrlvalue, DirF(b)))
                    OK = (Couplers[b].Connected->RunInternalCommand() || OK);

    return OK;
}

// *************************************************************************************************
// Q: 20160714
// Ustawienie komendy wraz z parametrami
// *************************************************************************************************
bool TMoverParameters::SetInternalCommand(std::string NewCommand, double NewValue1,
                                          double NewValue2)
{
    bool SIC;
    if ((CommandIn.Command == NewCommand) && (CommandIn.Value1 == NewValue1) &&
        (CommandIn.Value2 == NewValue2))
        SIC = false;
    else
    {
        CommandIn.Command = NewCommand;
        CommandIn.Value1 = NewValue1;
        CommandIn.Value2 = NewValue2;
        SIC = true;
        LastLoadChangeTime = 0; // zerowanie czasu (roz)ładowania
    }

    return SIC;
}

// *************************************************************************************************
// Q: 20160714
// wysyłanie komendy w kierunku dir (1=przód, -1=tył) do kolejnego pojazdu (jednego)
// *************************************************************************************************
bool TMoverParameters::SendCtrlToNext( std::string CtrlCommand, double ctrlvalue, double dir ) {
    bool OK;
    int d; // numer sprzęgu w kierunku którego wysyłamy

    // Ra: był problem z propagacją, jeśli w składzie jest pojazd wstawiony odwrotnie
    // Ra: problem jest również, jeśli AI będzie na końcu składu
    OK = ( dir != 0 ); // and Mains;
    d = ( 1 + static_cast<int>(Sign( dir )) ) / 2; // dir=-1=>d=0, dir=1=>d=1 - wysyłanie tylko w tył
    if( OK ) {
        // musi być wybrana niezerowa kabina
        if( ( Couplers[ d ].Connected != nullptr )
         && ( TestFlag( Couplers[ d ].CouplingFlag, ctrain_controll ) ) ) {
            if( Couplers[ d ].ConnectedNr != d ) {
                // jeśli ten nastpęny jest zgodny z aktualnym
                if( Couplers[ d ].Connected->SetInternalCommand( CtrlCommand, ctrlvalue, dir ) )
                    OK = ( Couplers[ d ].Connected->RunInternalCommand() && OK ); // tu jest rekurencja
            }
            else {
                // jeśli następny jest ustawiony przeciwnie, zmieniamy kierunek
                if( Couplers[ d ].Connected->SetInternalCommand( CtrlCommand, ctrlvalue, -dir ) )
                    OK = ( Couplers[ d ].Connected->RunInternalCommand() && OK ); // tu jest rekurencja
            }
        }
    }
    return OK;
}

// *************************************************************************************************
// Q: 20160723
// *************************************************************************************************
// wysłanie komendy otrzymanej z kierunku CValue2 (względem sprzęgów: 1=przod,-1=tył)
// Ra: Jest tu problem z rekurencją. Trzeba by oddzielić wykonywanie komend od mechanizmu
// ich propagacji w składzie. Osobnym problemem może być propagacja tylko w jedną stronę.
// Jeśli jakiś człon jest wstawiony odwrotnie, to również odwrotnie musi wykonywać
// komendy związane z kierunkami (PantFront, PantRear, DoorLeft, DoorRight).
// Komenda musi być zdefiniowana tutaj, a jeśli się wywołuje funkcję, to ona nie może
// sama przesyłać do kolejnych pojazdów. Należy też się zastanowić, czy dla uzyskania
// jakiejś zmiany (np. IncMainCtrl) lepiej wywołać funkcję, czy od razu wysłać komendę.
bool TMoverParameters::RunCommand(std::string Command, double CValue1, double CValue2)
{
    bool OK;
    std::string testload;
    OK = false;

	if (Command == "MainCtrl")
	{
		if (MainCtrlPosNo >= floor(CValue1))
			MainCtrlPos = static_cast<int>(floor(CValue1));
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	else if (Command == "ScndCtrl")
	{
		if ((EngineType == ElectricInductionMotor))
			if ((ScndCtrlPos == 0) && (floor(CValue1) > 0))
				if ((Vmax < 250))
					ScndCtrlActualPos = Round(Vel + 0.5);
				else
					ScndCtrlActualPos = Round(Vel / 2 + 0.5);
			else if ((floor(CValue1) == 0))
				ScndCtrlActualPos = 0;
		if (ScndCtrlPosNo >= floor(CValue1))
			ScndCtrlPos = static_cast<int>(floor(CValue1));
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	/*  else if command='BrakeCtrl' then
	begin
	if BrakeCtrlPosNo>=Trunc(CValue1) then
	begin
	BrakeCtrlPos:=Trunc(CValue1);
	OK:=SendCtrlToNext(command,CValue1,CValue2);
	end;
	end */
	else if (Command == "Brake") // youBy - jak sie EP hamuje, to trza sygnal wyslac...
	{
		Hamulec->SetEPS(CValue1);
		// fBrakeCtrlPos:=BrakeCtrlPos; //to powinnno być w jednym miejscu, aktualnie w C++!!!
		BrakePressureActual = BrakePressureTable[BrakeCtrlPos];
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	} // youby - odluzniacz hamulcow, przyda sie
	else if (Command == "BrakeReleaser")
	{
		OK = BrakeReleaser(Round(CValue1)); // samo się przesyła dalej
											// OK:=SendCtrlToNext(command,CValue1,CValue2); //to robiło kaskadę 2^n
	}
	else if (Command == "MainSwitch")
	{
		if (CValue1 == 1)
		{
			Mains = true;
			if ((EngineType == DieselEngine) && Mains)
				dizel_enginestart = true;
		}
		else
			Mains = false;
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	else if (Command == "Direction")
	{
		ActiveDir = static_cast<int>(floor(CValue1));
		DirAbsolute = ActiveDir * CabNo;
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	else if (Command == "CabActivisation")
	{
		//  OK:=Power>0.01;
		switch (static_cast<int>(CValue1 * CValue2))
		{ // CValue2 ma zmieniany znak przy niezgodności sprzęgów
            case 1: {
                CabNo = 1;
                break;
            }
            case -1: {
                CabNo = -1;
                break;
            }
            default:{
                CabNo = 0; // gdy CValue1==0
                break;
            }
		}
		DirAbsolute = ActiveDir * CabNo;
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	else if (Command == "AutoRelaySwitch")
	{
		if ((CValue1 == 1) && (AutoRelayType == 2))
			AutoRelayFlag = true;
		else
			AutoRelayFlag = false;
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	else if (Command == "FuseSwitch")
	{
		if (((EngineType == ElectricSeriesMotor) || (EngineType == DieselElectric)) && FuseFlag &&
			(CValue1 == 1) && (MainCtrlActualPos == 0) && (ScndCtrlActualPos == 0) && Mains)
			/*      if (EngineType=ElectricSeriesMotor) and (CValue1=1) and
			(MainCtrlActualPos=0) and (ScndCtrlActualPos=0) and Mains then*/
			FuseFlag = false; /*wlaczenie ponowne obwodu*/
							  // if ((EngineType=ElectricSeriesMotor)or(EngineType=DieselElectric)) and not FuseFlag and
							  // (CValue1=0) and Mains then
							  //   FuseFlag:=true;
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	else if (Command == "ConverterSwitch") /*NBMX*/
	{
		if ((CValue1 == 1))
			ConverterAllow = true;
		else if ((CValue1 == 0))
			ConverterAllow = false;
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	else if (Command == "BatterySwitch") /*NBMX*/
    {
        if ((CValue1 == 1))
            Battery = true;
        else if ((CValue1 == 0))
            Battery = false;
        if ((Battery) && (ActiveCab != 0) /*or (TrainType=dt_EZT)*/)
            SecuritySystem.Status = SecuritySystem.Status || s_waiting; // aktywacja czuwaka
        else
            SecuritySystem.Status = 0; // wyłączenie czuwaka
        OK = SendCtrlToNext(Command, CValue1, CValue2);
    }
    //   else if command='EpFuseSwitch' then         {NBMX}
        //   begin
        //     if (CValue1=1) then EpFuse:=true
        //     else if (CValue1=0) then EpFuse:=false;
        //     OK:=SendCtrlToNext(command,CValue1,CValue2);
        //   end
    else if (Command == "CompressorSwitch") /*NBMX*/
	{
		if ((CValue1 == 1))
			CompressorAllow = true;
		else if ((CValue1 == 0))
			CompressorAllow = false;
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	else if (Command == "DoorOpen") /*NBMX*/
	{ // Ra: uwzględnić trzeba jeszcze zgodność sprzęgów
		if ((CValue2 > 0))
		{ // normalne ustawienie pojazdu
			if ((CValue1 == 1) || (CValue1 == 3))
				DoorLeftOpened = true;
			if ((CValue1 == 2) || (CValue1 == 3))
				DoorRightOpened = true;
		}
		else
		{ // odwrotne ustawienie pojazdu
			if ((CValue1 == 2) || (CValue1 == 3))
				DoorLeftOpened = true;
			if ((CValue1 == 1) || (CValue1 == 3))
				DoorRightOpened = true;
		}
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	else if (Command == "DoorClose") /*NBMX*/
	{ // Ra: uwzględnić trzeba jeszcze zgodność sprzęgów
		if ((CValue2 > 0))
		{ // normalne ustawienie pojazdu
			if ((CValue1 == 1) || (CValue1 == 3))
				DoorLeftOpened = false;
			if ((CValue1 == 2) || (CValue1 == 3))
				DoorRightOpened = false;
		}
		else
		{ // odwrotne ustawienie pojazdu
			if ((CValue1 == 2) || (CValue1 == 3))
				DoorLeftOpened = false;
			if ((CValue1 == 1) || (CValue1 == 3))
				DoorRightOpened = false;
		}
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	else if (Command == "PantFront") /*Winger 160204*/
	{ // Ra: uwzględnić trzeba jeszcze zgodność sprzęgów
	  // Czemu EZT ma być traktowane inaczej? Ukrotnienie ma, a człon może być odwrócony
		if ((TrainType == dt_EZT))
		{ //'ezt'
			if ((CValue1 == 1))
			{
				PantFrontUp = true;
				PantFrontStart = 0;
			}
			else if ((CValue1 == 0))
			{
				PantFrontUp = false;
				PantFrontStart = 1;
			}
		}
		else
		{ // nie 'ezt' - odwrotne ustawienie pantografów: ^-.-^ zamiast ^-.^-
			if ((CValue1 == 1))
				if ((TestFlag(Couplers[1].CouplingFlag, ctrain_controll) && (CValue2 == 1)) ||
					(TestFlag(Couplers[0].CouplingFlag, ctrain_controll) && (CValue2 == -1)))
				{
					PantFrontUp = true;
					PantFrontStart = 0;
				}
				else
				{
					PantRearUp = true;
					PantRearStart = 0;
				}
			else if ((CValue1 == 0))
				if ((TestFlag(Couplers[1].CouplingFlag, ctrain_controll) && (CValue2 == 1)) ||
					(TestFlag(Couplers[0].CouplingFlag, ctrain_controll) && (CValue2 == -1)))
				{
					PantFrontUp = false;
					PantFrontStart = 1;
				}
				else
				{
					PantRearUp = false;
					PantRearStart = 1;
				}
		}
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	else if (Command == "PantRear") /*Winger 160204, ABu 310105 i 030305*/
	{ // Ra: uwzględnić trzeba jeszcze zgodność sprzęgów
		if ((TrainType == dt_EZT))
		{ /*'ezt'*/
			if ((CValue1 == 1))
			{
				PantRearUp = true;
				PantRearStart = 0;
			}
			else if ((CValue1 == 0))
			{
				PantRearUp = false;
				PantRearStart = 1;
			}
		}
		else
		{ /*nie 'ezt'*/
			if ((CValue1 == 1))
				/*if ostatni polaczony sprz. sterowania*/
				if ((TestFlag(Couplers[1].CouplingFlag, ctrain_controll) && (CValue2 == 1)) ||
					(TestFlag(Couplers[0].CouplingFlag, ctrain_controll) && (CValue2 == -1)))
				{
					PantRearUp = true;
					PantRearStart = 0;
				}
				else
				{
					PantFrontUp = true;
					PantFrontStart = 0;
				}
			else if ((CValue1 == 0))
				if ((TestFlag(Couplers[1].CouplingFlag, ctrain_controll) && (CValue2 == 1)) ||
					(TestFlag(Couplers[0].CouplingFlag, ctrain_controll) && (CValue2 == -1)))
				{
					PantRearUp = false;
					PantRearStart = 1;
				}
				else
				{
					PantFrontUp = false;
					PantFrontStart = 1;
				}
		}
		OK = SendCtrlToNext(Command, CValue1, CValue2);
	}
	else if (Command == "MaxCurrentSwitch")
	{
		OK = MaxCurrentSwitch(CValue1 == 1);
	}
	else if (Command == "MinCurrentSwitch")
	{
		OK = MinCurrentSwitch(CValue1 == 1);
	}
	/*test komend oddzialywujacych na tabor*/
	else if (Command == "SetDamage")
	{
		if (CValue2 == 1)
			OK = SetFlag(DamageFlag, static_cast<int>(floor(CValue1)));
		if (CValue2 == -1)
			OK = SetFlag(DamageFlag, static_cast<int>(-floor(CValue1)));
	}
	else if (Command == "Emergency_brake")
	{
		if (EmergencyBrakeSwitch(floor(CValue1) == 1)) // YB: czy to jest potrzebne?
			OK = true;
		else
			OK = false;
	}
	else if (Command == "BrakeDelay")
	{
		BrakeDelayFlag = static_cast<int>(floor(CValue1));
		OK = true;
	}
	else if (Command == "SandDoseOn")
	{
		if (SandDoseOn())
			OK = true;
		else
			OK = false;
	}
	else if (Command == "CabSignal") /*SHP,Indusi*/
	{ // Ra: to powinno działać tylko w członie obsadzonym
		if (/*(TrainType=dt_EZT)or*/ (ActiveCab != 0) && (Battery) &&
			TestFlag(SecuritySystem.SystemType,
				2)) // jeśli kabina jest obsadzona (silnikowy w EZT?)
					/*?*/ /* WITH  SecuritySystem */
		{
			SecuritySystem.VelocityAllowed = static_cast<int>(floor(CValue1));
			SecuritySystem.NextVelocityAllowed = static_cast<int>(floor(CValue2));
			SecuritySystem.SystemSoundSHPTimer = 0; // hunter-091012
			SetFlag(SecuritySystem.Status, s_active);
		}
		// else OK:=false;
		OK = true; // true, gdy można usunąć komendę
	}
	/*naladunek/rozladunek*/
	else if (Pos("Load=", Command) == 1)
	{
		OK = false; // będzie powtarzane aż się załaduje
		if ((Vel == 0) && (MaxLoad > 0) &&
			(Load < MaxLoad * (1.0 + OverLoadFactor))) // czy można ładowac?
			if (Distance(Loc, CommandIn.Location, Dim, Dim) < 10) // ten peron/rampa
			{
				testload = ToLower(DUE(Command));
				if (Pos(testload, LoadAccepted) > 0) // nazwa jest obecna w CHK
					OK = LoadingDone(Min0R(CValue2, LoadSpeed), testload); // zmienia LoadStatus
			}
		// if OK then LoadStatus:=0; //nie udalo sie w ogole albo juz skonczone
	}
	else if (Pos("UnLoad=", Command) == 1)
	{
		OK = false; // będzie powtarzane aż się rozładuje
		if ((Vel == 0) && (Load > 0)) // czy jest co rozladowac?
			if (Distance(Loc, CommandIn.Location, Dim, Dim) < 10) // ten peron
			{
				testload = DUE(Command); // zgodność nazwy ładunku z CHK
				if (LoadType == testload) /*mozna to rozladowac*/
					OK = LoadingDone(-Min0R(CValue2, LoadSpeed), testload);
			}
		// if OK then LoadStatus:=0;
	}

	return OK; // dla true komenda będzie usunięta, dla false wykonana ponownie
}

// *************************************************************************************************
// Q: 20160714
// Uruchamia funkcję RunCommand aż do skutku. Jeśli będzie pozytywny to kasuje komendę.
// *************************************************************************************************
bool TMoverParameters::RunInternalCommand(void)
{
    bool OK;

    if (!CommandIn.Command.empty())
    {
        OK = RunCommand(CommandIn.Command, CommandIn.Value1, CommandIn.Value2);
        if (OK)

        {
            CommandIn.Command.clear(); // kasowanie bo rozkaz wykonany
            CommandIn.Value1 = 0;
            CommandIn.Value2 = 0;
            CommandIn.Location.X = 0;
            CommandIn.Location.Y = 0;
            CommandIn.Location.Z = 0;
            if (!PhysicActivation)
                Physic_ReActivation();
        }
    }
    else
        OK = false;
    return OK;
}

// *************************************************************************************************
// Q: 20160714
// Zwraca wartość natężenia prądu na wybranym amperomierzu. Podfunkcja do ShowCurrent.
// *************************************************************************************************
double TMoverParameters::ShowCurrentP(int AmpN)
{
    int b, Bn;
    bool Grupowy;

    // ClearPendingExceptions;
    Grupowy = ((DelayCtrlFlag) && (TrainType == dt_ET22)); // przerzucanie walu grupowego w ET22;
    Bn = RList[MainCtrlActualPos].Bn; // ile równoległych gałęzi silników

    if ((DynamicBrakeType == dbrake_automatic) && (DynamicBrakeFlag))
        Bn = 2;
    if (Power > 0.01)
    {
        if (AmpN > 0) // podać prąd w gałęzi
        {
            if ((Bn < AmpN) || ((Grupowy) && (AmpN == Bn - 1)))
                return 0;
            else // normalne podawanie pradu
                return floor(abs(Im));
        }
        else // podać całkowity
            return floor(abs(Itot));
    }
    else // pobor pradu jezeli niema mocy
    {
        int current = 0;
        for (b = 0; b < 2; b++)
            // with Couplers[b] do
            if (TestFlag(Couplers[b].CouplingFlag, ctrain_controll))
                if (Couplers[b].Connected->Power > 0.01)
                    current = static_cast<int>(Couplers[b].Connected->ShowCurrent(AmpN));
        return current;
    }
}
