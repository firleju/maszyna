/**   @file
     @brief
*/

/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "mtable.h"

// using namespace Mtable;
std::shared_ptr<TMTableTime> Mtable::GlobalTime;

double CompareTime(double t1h, double t1m, double t2h, double t2m) /*roznica czasu w minutach*/
// zwraca r�nic� czasu
// je�li pierwsza jest aktualna, a druga rozk�adowa, to ujemna oznacza op�nienie
// na d�u�sz� met� trzeba uwzgl�dni� dat�, jakby op�nienia mia�y przekracza� 12h (towarowych)
{
    double t;

    if ((t2h < 0))
        return 0;
    else
    {
        t = (t2h - t1h) * 60 + t2m - t1m; // je�li t2=00:05, a t1=23:50, to r�nica wyjdzie ujemna
        if ((t < -720)) // je�li r�nica przekracza 12h na minus
            t = t + 1440; // to dodanie doby minut;else
        if ((t > 720)) // je�li przekracza 12h na plus
            t = t - 1440; // to odj�cie doby minut
        return t;
    }
}

double TTrainParameters::CheckTrainLatency()
{
    if ((LastStationLatency > 1.0) || (LastStationLatency < 0))
        return LastStationLatency; /*spoznienie + lub do przodu - z tolerancja 1 min*/
    else
        return 0;
}

double TTrainParameters::WatchMTable(double DistCounter)
{ // zwraca odleglo�� do najblizszej stacji z zatrzymaniem
    double dist;

    if (Direction == 1)
        dist = TimeTable[StationIndex].km - TimeTable[0].km - DistCounter;
    else
        dist = TimeTable[0].km - TimeTable[StationIndex].km - DistCounter;
    return dist;
}

std::string TTrainParameters::NextStop()
{ // pobranie nazwy nast�pnego miejsca zatrzymania
    if (StationIndex <= StationCount)
        return "PassengerStopPoint:" + NextStationName; // nazwa nast�pnego przystanku;
    else
        return "[End of route]"; //�e niby koniec
}

bool TTrainParameters::IsStop()
{ // zapytanie, czy zatrzymywa� na nast�pnym punkcie rozk�adu
    if ((StationIndex < StationCount))
        return TimeTable[StationIndex].Ah >= 0; //-1 to brak postoju
    else
        return true; // na ostatnim si� zatrzyma� zawsze
}

bool TTrainParameters::UpdateMTable(double hh, double mm, std::string NewName)
/*odfajkowanie dojechania do stacji (NewName) i przeliczenie op�nienia*/
{
    bool OK;
    OK = false;
    if (StationIndex <= StationCount) // Ra: "<=", bo ostatni przystanek jest traktowany wyj�tkowo
    {
        if (NewName == NextStationName) // je�li dojechane do nast�pnego
        { // Ra: wywo�anie mo�e by� powtarzane, jak stoi na W4
            if (TimeTable[StationIndex + 1].km - TimeTable[StationIndex].km <
                0) // to jest bez sensu
                Direction = -1;
            else
                Direction = 1; // prowizorka bo moze byc zmiana kilometrazu
            // ustalenie, czy op�niony (por�wnanie z czasem odjazdu)
            LastStationLatency =
                CompareTime(hh, mm, TimeTable[StationIndex].Dh, TimeTable[StationIndex].Dm);
            // inc(StationIndex); //przej�cie do nast�pnej pozycji StationIndex<=StationCount
            if (StationIndex <
                StationCount) // Ra: "<", bo dodaje 1 przy przej�ciu do nast�pnej stacji
            { // je�li nie ostatnia stacja
                NextStationName = TimeTable[StationIndex + 1].StationName; // zapami�tanie nazwy
                TTVmax = TimeTable[StationIndex + 1]
                             .vmax; // Ra: nowa pr�dko�� rozk�adowa na kolejnym odcinku
            }
            else // gdy ostatnia stacja
                NextStationName = ""; // nie ma nast�pnej stacji
            OK = true;
        }
    }
    return OK; /*czy jest nastepna stacja*/
}

void TTrainParameters::StationIndexInc()
{ // przej�cie do nast�pnej pozycji StationIndex<=StationCount
    ++StationIndex;
}

bool TTrainParameters::IsTimeToGo(double hh, double mm)
// sprawdzenie, czy mo�na ju� odjecha� z aktualnego zatrzymania
// StationIndex to numer nast�pnego po dodarciu do aktualnego
{
    if ((StationIndex < 1))
        return true; // przed pierwsz� jecha�
    else if ((StationIndex < StationCount))
    { // opr�cz ostatniego przystanku
        if ((TimeTable[StationIndex].Ah < 0)) // odjazd z poprzedniego
            return true; // czas przyjazdu nie by� podany - przelot
        else
            return CompareTime(hh, mm, TimeTable[StationIndex].Dh, TimeTable[StationIndex].Dm) <= 0;
    }
    else // gdy rozk�ad si� sko�czy�
        return false; // dalej nie jecha�
}

std::string TTrainParameters::ShowRelation()
/*zwraca informacj� o relacji*/
{
    // if (Relation1=TimeTable[1].StationName) and (Relation2=TimeTable[StationCount].StationName)
    if ((Relation1 != "") && (Relation2 != ""))
        return Relation1 + " - " + Relation2;
    else
        return "";
}

TTrainParameters::TTrainParameters(std::string NewTrainName)
/*wst�pne ustawienie parametr�w rozk�adu jazdy*/
{
    NewName(NewTrainName);
}

void TTrainParameters::NewName(std::string NewTrainName)
/*wst�pne ustawienie parametr�w rozk�adu jazdy*/
{
    TrainName = NewTrainName;
    StationCount = 0;
    StationIndex = 0;
    NextStationName = "nowhere";
    LastStationLatency = 0;
    Direction = 1;
    Relation1 = "";
    Relation2 = "";
    for (int i = 0; i < MaxTTableSize + 1; ++i)
    {
        TMTableLine *t = &TimeTable[i];
        t->km = 0;
        t->vmax = -1;
        t->StationName = "nowhere";
        t->StationWare = "";
        t->TrackNo = 1;
        t->Ah = -1;
        t->Am = -1;
        t->Dh = -1;
        t->Dm = -1;
        t->tm = 0;
        t->WaitTime = 0;
    }
    TTVmax = 100; /*wykasowac*/
    BrakeRatio = 0;
    LocSeries = "";
    LocLoad = 0;
}

void TTrainParameters::UpdateVelocity(int StationCount, double vActual)
// zapisywanie pr�dko�ci maksymalnej do wcze�niejszych odcink�w
// wywo�ywane z numerem ostatniego przetworzonego przystanku
{
    int i = StationCount;
    // TTVmax:=vActual;  {PROWIZORKA!!!}
    while ((i >= 0) && (TimeTable[i].vmax == -1))
    {
        TimeTable[i].vmax = vActual; // pr�dko�� dojazdu do przystanku i
        --i; // ewentualnie do poprzedniego te�
    }
}

// bool TTrainParameters::LoadTTfile(std::string scnpath, int iPlus, double vmax)
//{
//	return false;
//}

bool TTrainParameters::LoadTTfile(std::string scnpath, int iPlus, double vmax)
// wczytanie pliku-tabeli z rozk�adem przesuni�tym o (fPlus); (vMax) nie ma znaczenia
{
    std::string lines;
    std::string s;
    std::ifstream fin;
    bool EndTable;
    double vActual;
    int i;
    int time; // do zwi�kszania czasu

    int ConversionError = 0;
    EndTable = false;
    if ((TrainName == ""))
    { // je�li pusty rozk�ad
        // UpdateVelocity(StationCount,vMax); //ograniczenie do pr�dko�ci startowej
    }
    else
    {
        ConversionError = 666;
        vActual = -1;
        s = scnpath + TrainName + ".txt";
        // Ra 2014-09: ustali� zasady wyznaczenia pierwotnego pliku przy przesuni�tych rozk�adach
        // (kolejny poci�g dostaje numer +2)
        fin.open(s.c_str()); // otwieranie pliku

        if (!fin.is_open())
        { // je�li nie ma pliku
            vmax = atoi(TrainName.c_str()); // nie ma pliku ale jest liczba
            if ((vmax > 10) && (vmax < 200))
            {
                TTVmax = vmax; // Ra 2014-07: zamiast rozk�adu mo�na poda� Vmax
                UpdateVelocity(StationCount, vmax); // ograniczenie do pr�dko�ci startowej
                ConversionError = 0;
            }
            else
                ConversionError = -8; /*Ra: ten b��d jest niepotrzebny*/
        }
        else
        { /*analiza rozk�adu jazdy*/
            ConversionError = 0;
            while (fin.good() && !((ConversionError != 0) || EndTable))
            {
                std::getline(fin, lines); /*wczytanie linii*/
                if (lines.find("___________________") != std::string::npos) /*linia pozioma g�rna*/
                {
                    fin >> s;
                    if (s == "[") /*lewy pion*/
                    {
                        fin >> s;
                        if (s == "Rodzaj") /*"Rodzaj i numer pociagu"*/
                            do
                            {
                                fin >> s;
                            } while (!(s == "|") || (fin.eof())); /*�rodkowy pion*/
                    }
                }
                else if (lines == "")
                {
                    fin.close();
                    break;
                }
                fin >> s; /*nazwa poci�gu*/
                // if LowerCase(s)<>ExtractFileName(TrainName) then {musi by� taka sama, jak nazwa
                // pliku}
                // ConversionError:=-7 {b��d niezgodno�ci}
                TrainName = s; // nadanie nazwy z pliku TXT (bez �cie�ki do pliku)
                // else
                { /*czytaj naglowek*/
                    while (fin >> s || !fin.bad())
                    {
                        if (s.find("_______|") != std::string::npos)
                            break;
                        // fin >> s;
                    } // while (!(s.find("_______|") != std::string::npos) || fin.eof());
                    while (fin >> s || !fin.bad())
                    {
                        if (s == "[")
                            break;
                    } // while (!() || (fin.eof())); /*pierwsza linia z relacj�*/
                    while (fin >> s || !fin.bad())
                    {
                        if (s != "|")
                            break;
                    } // while (!(() || fin.eof()));
                    if (s != "|")
                        Relation1 = s;
                    else
                        ConversionError = -5;
                    while (fin >> s || !fin.bad())
                    {
                        if (s == "Relacja")
                            break;
                    } // while (
                    //  !( || (fin.eof()))); /*druga linia z relacj�*/
                    while (fin >> s || !fin.bad())
                    {
                        if (s == "|")
                            break;
                    } // while (!( || (fin.eof())));
                    fin >> Relation2;
                    while (fin >> s || !fin.bad())
                    {
                        if (s == "Wymagany")
                            break;
                    } // while (!();
                    while (fin >> s || !fin.bad())
                    {
                        if ((s == "|") || (s == "\n"))
                            break;
                    } // while (!());
                    fin >> s;
                    s = s.substr(0, s.find("%"));
                    BrakeRatio = atof(s.c_str());
                    while (fin >> s || fin.bad())
                    {
                        if (s == "Seria")
                            break;
                    } // while (!(s == "Seria"));
                    do
                    {
                        fin >> s;
                    } while (!((s == "|") || (fin.bad())));
                    fin >> LocSeries;
                    fin >> LocLoad; // = s2rE(ReadWord(fin));
                    do
                    {
                        fin >> s;
                    } while (!(s.find("[______________") != std::string::npos || fin.bad()));
                    while (!fin.bad() && !EndTable)
                    {
                        ++StationCount;
                        do
                        {
                            fin >> s;
                        } while (!((s == "[") || (fin.bad())));
                        TMTableLine *record = &TimeTable[StationCount];
                        {
                            if (s == "[")
                                fin >> s;
                            else
                                ConversionError = -4;
                            if (s.find("|") == std::string::npos)
                            {
                                record->km = atof(s.c_str());
                                fin >> s;
                            }
                            if (s.find("|_____|") !=
                                std::string::npos) /*zmiana predkosci szlakowej*/
                                UpdateVelocity(StationCount, vActual);
                            else
                            {
                                fin >> s;
                                if (s.find("|") == std::string::npos)
                                    vActual = atof(s.c_str());
                            }
                            while (s.find("|") == std::string::npos)
                                fin >> s;
                            fin >> record->StationName;
                            do
                            {
                                fin >> s;
                            } while (!((s == "1") || (s == "2") || fin.bad()));
                            record->TrackNo = atoi(s.c_str());
                            fin >> s;
                            if (s != "|")
                            {
                                if (s.find(hrsd) != std::string::npos)
                                {
                                    record->Ah = atoi(
                                        s.substr(0, s.find(hrsd)).c_str()); // godzina przyjazdu
                                    record->Am = atoi(s.substr(s.find(hrsd) + 1, s.length())
                                                          .c_str()); // minuta przyjazdu
                                }
                                else
                                {
                                    record->Ah = TimeTable[StationCount - 1]
                                                     .Ah; // godzina z poprzedniej pozycji
                                    record->Am = atoi(s.c_str()); // bo tylko minuty podane
                                }
                            }
                            do
                            {
                                fin >> s;
                            } while (!((s != "|") || (fin.bad())));
                            if (s != "]")
                                record->tm = atof(s.c_str());
                            do
                            {
                                fin >> s;
                            } while (!((s == "[") || fin.bad()));
                            fin >> s;
                            if (s.find("|") == std::string::npos)
                            {
                                /*tu s moze byc miejscem zmiany predkosci szlakowej*/
                                fin >> s;
                            }
                            if (s.find("|_____|") !=
                                std::string::npos) /*zmiana predkosci szlakowej*/
                                UpdateVelocity(StationCount, vActual);
                            else
                            {
                                fin >> s;
                                if (s.find("|") == std::string::npos)
                                    vActual = atof(s.c_str());
                            }
                            while (s.find("|") == std::string::npos)
                                fin >> s;
                            fin >> record->StationWare;
                            do
                            {
                                fin >> s;
                            } while (!((s == "1") || (s == "2") || fin.bad()));
                            record->TrackNo = atoi(s.c_str());
                            fin >> s;
                            if (s != "|")
                            {
                                if (s.find(hrsd) != std::string::npos)
                                {
                                    record->Dh =
                                        atoi(s.substr(0, s.find(hrsd)).c_str()); // godzina odjazdu
                                    record->Dm = atoi(s.substr(s.find(hrsd) + 1, s.length())
                                                          .c_str()); // minuta odjazdu
                                }
                                else
                                {
                                    record->Dh = TimeTable[StationCount - 1]
                                                     .Dh; // godzina z poprzedniej pozycji
                                    record->Dm = atoi(s.c_str()); // bo tylko minuty podane
                                }
                            }
                            else
                            {
                                record->Dh = record->Ah; // odjazd o tej samej, co przyjazd (dla
                                                         // ostatniego te�)
                                record->Dm = record->Am; // bo s� u�ywane do wyliczenia op�nienia
                                                         // po dojechaniu
                            }
                            if ((record->Ah >= 0))
                                record->WaitTime = (int)(CompareTime(record->Ah, record->Am,
                                                                     record->Dh, record->Dm) +
                                                         0.1);
                            do
                            {
                                fin >> s;
                            } while (!((s != "|") || (fin.bad())));
                            if (s != "]")
                                record->tm = atof(s.c_str());
                            do
                            {
                                fin >> s;
                            } while (!((s.find("[") != std::string::npos) || fin.bad()));
                            if (s.find("_|_") == std::string::npos)
                                fin >> s;
                            if (s.find("|") == std::string::npos)
                            {
                                /*tu s moze byc miejscem zmiany predkosci szlakowej*/
                                fin >> s;
                            }
                            if (s.find("|_____|") !=
                                std::string::npos) /*zmiana predkosci szlakowej*/
                                UpdateVelocity(StationCount, vActual);
                            else
                            {
                                fin >> s;
                                if (s.find("|") == std::string::npos)
                                    vActual = atof(s.c_str());
                            }
                            while (s.find("|") == std::string::npos)
                                fin >> s;
                            while ((s.find("]") == std::string::npos))
                                fin >> s;
                            if (s.find("_|_") != std::string::npos)
                                EndTable = true;
                        } /*timetableline*/
                    }
                }
            } /*while eof*/
            fin.close();
        }
    }
    if (ConversionError == 0)
    {
        if ((TimeTable[1].StationName == Relation1)) // je�li nazwa pierwszego zgodna z relacj�
            if ((TimeTable[1].Ah < 0)) // a nie podany czas przyjazdu
            { // to mamy zatrzymanie na pierwszym, a nie przelot
                TimeTable[1].Ah = TimeTable[1].Dh;
                TimeTable[1].Am = TimeTable[1].Dm;
            }
        // NextStationName:=TimeTable[1].StationName;
        /*  TTVmax:=TimeTable[1].vmax;  */
    }
    if ((iPlus != 0)) // je�eli jest przesuni�cie rozk�adu
    {
        long i_end = StationCount + 1;
        for (i = 1; i < i_end; ++i) // bez with, bo ci�ko si� przenosi na C++
        {
            if ((TimeTable[i].Ah >= 0))
            {
                time = iPlus + TimeTable[i].Ah * 60 + TimeTable[i].Am; // nowe minuty
                TimeTable[i].Am = time % 60;
                TimeTable[i].Ah = (time /*div*/ / 60) % 60;
            }
            if ((TimeTable[i].Dh >= 0))
            {
                time = iPlus + TimeTable[i].Dh * 60 + TimeTable[i].Dm; // nowe minuty
                TimeTable[i].Dm = time % 60;
                TimeTable[i].Dh = (time /*div*/ / 60) % 60;
            }
        }
    }
    return !(bool)ConversionError;
}

void TMTableTime::UpdateMTableTime(double deltaT)
// dodanie czasu (deltaT) w sekundach, z przeliczeniem godziny
{
    mr = mr + deltaT; // dodawanie sekund
    while (mr > 60.0) // przeliczenie sekund do w�a�ciwego przedzia�u
    {
        mr = mr - 60.0;
        ++mm;
    }
    while (mm > 59) // przeliczenie minut do w�a�ciwego przedzia�u
    {
        mm = mm - 60;
        ++hh;
    }
    while (hh > 23) // przeliczenie godzin do w�a�ciwego przedzia�u
    {
        hh = hh - 24;
        ++dd; // zwi�kszenie numeru dnia
    }
    GameTime = GameTime + deltaT;
}

bool TTrainParameters::DirectionChange()
// sprawdzenie, czy po zatrzymaniu wykona� kolejne komendy
{
    if ((StationIndex > 0) && (StationIndex < StationCount)) // dla ostatniej stacji nie
        if (TimeTable[StationIndex].StationWare.find("@") != std::string::npos)
            return true;
    return false;
}
