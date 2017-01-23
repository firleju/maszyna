/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/
/*
    MaSzyna EU07 locomotive simulator
    Copyright (C) 2001-2004  Marcin Wozniak, Maciej Czapkiewicz and others

*/
#include "stdafx.h"

#include "Globals.h"
#include "usefull.h"
//#include "Mover.h"
#include "Console.h"
#include "Driver.h"
#include "Logs.h"
#include "PyInt.h"
#include "World.h"
#include "parser.h"

// namespace Global {

// parametry do użytku wewnętrznego
// double Global::tSinceStart=0;
TGround *Global::pGround = NULL;
// char Global::CreatorName1[30]="2001-2004 Maciej Czapkiewicz <McZapkie>";
// char Global::CreatorName2[30]="2001-2003 Marcin Woźniak <Marcin_EU>";
// char Global::CreatorName3[20]="2004-2005 Adam Bugiel <ABu>";
// char Global::CreatorName4[30]="2004 Arkadiusz Ślusarczyk <Winger>";
// char Global::CreatorName5[30]="2003-2009 Łukasz Kirchner <Nbmx>";
std::string Global::asCurrentSceneryPath = "scenery/";
std::string Global::asCurrentTexturePath = std::string(szTexturePath);
std::string Global::asCurrentDynamicPath = "";
int Global::iSlowMotion =
    0; // info o malym FPS: 0-OK, 1-wyłączyć multisampling, 3-promień 1.5km, 7-1km
TDynamicObject *Global::changeDynObj = NULL; // info o zmianie pojazdu
bool Global::detonatoryOK; // info o nowych detonatorach
double Global::ABuDebug = 0;
std::string Global::asSky = "1";
double Global::fOpenGL = 0.0; // wersja OpenGL - do sprawdzania obecności rozszerzeń
bool Global::bOpenGL_1_5 = false; // czy są dostępne funkcje OpenGL 1.5
double Global::fLuminance = 1.0; // jasność światła do automatycznego zapalania
int Global::iReCompile = 0; // zwiększany, gdy trzeba odświeżyć siatki
HWND Global::hWnd = NULL; // uchwyt okna
int Global::iCameraLast = -1;
std::string Global::asRelease = "16.0.1172.482";
std::string Global::asVersion =
    "Compilation 2017-01-10, release " + Global::asRelease + "."; // tutaj, bo wysyłany
int Global::iViewMode = 0; // co aktualnie widać: 0-kabina, 1-latanie, 2-sprzęgi, 3-dokumenty
int Global::iTextMode = 0; // tryb pracy wyświetlacza tekstowego
int Global::iScreenMode[12] = {0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0}; // numer ekranu wyświetlacza tekstowego
double Global::fSunDeclination = 0.0; // deklinacja Słońca
double Global::fTimeAngleDeg = 0.0; // godzina w postaci kąta
float Global::fClockAngleDeg[6]; // kąty obrotu cylindrów dla zegara cyfrowego
char *Global::szTexturesTGA[4] = {"tga", "dds", "tex", "bmp"}; // lista tekstur od TGA
char *Global::szTexturesDDS[4] = {"dds", "tga", "tex", "bmp"}; // lista tekstur od DDS
int Global::iKeyLast = 0; // ostatnio naciśnięty klawisz w celu logowania
GLuint Global::iTextureId = 0; // ostatnio użyta tekstura 2D
int Global::iPause = 0x10; // globalna pauza ruchu
bool Global::bActive = true; // czy jest aktywnym oknem
int Global::iErorrCounter = 0; // licznik sprawdzań do śledzenia błędów OpenGL
int Global::iTextures = 0; // licznik użytych tekstur
TWorld *Global::pWorld = NULL;
cParser *Global::pParser = NULL;
int Global::iSegmentsRendered = 90; // ilość segmentów do regulacji wydajności
TCamera *Global::pCamera = NULL; // parametry kamery
TDynamicObject *Global::pUserDynamic = NULL; // pojazd użytkownika, renderowany bez trzęsienia
bool Global::bSmudge = false; // czy wyświetlać smugę, a pojazd użytkownika na końcu
std::string Global::asTranscript[5]; // napisy na ekranie (widoczne)
TTranscripts Global::tranTexts; // obiekt obsługujący stenogramy dźwięków na ekranie

// parametry scenerii
vector3 Global::pCameraPosition;
double Global::pCameraRotation;
double Global::pCameraRotationDeg;
vector3 Global::pFreeCameraInit[10];
vector3 Global::pFreeCameraInitAngle[10];
double Global::fFogStart = 1700;
double Global::fFogEnd = 2000;
float Global::Background[3] = {0.2, 0.4, 0.33};
GLfloat Global::AtmoColor[] = {0.423f, 0.702f, 1.0f};
GLfloat Global::FogColor[] = {0.6f, 0.7f, 0.8f};
GLfloat Global::ambientDayLight[] = {0.40f, 0.40f, 0.45f, 1.0f}; // robocze
GLfloat Global::diffuseDayLight[] = {0.55f, 0.54f, 0.50f, 1.0f};
GLfloat Global::specularDayLight[] = {0.95f, 0.94f, 0.90f, 1.0f};
GLfloat Global::ambientLight[] = {0.80f, 0.80f, 0.85f, 1.0f}; // stałe
GLfloat Global::diffuseLight[] = {0.85f, 0.85f, 0.80f, 1.0f};
GLfloat Global::specularLight[] = {0.95f, 0.94f, 0.90f, 1.0f};
GLfloat Global::whiteLight[] = {1.00f, 1.00f, 1.00f, 1.0f};
GLfloat Global::noLight[] = {0.00f, 0.00f, 0.00f, 1.0f};
GLfloat Global::darkLight[] = {0.03f, 0.03f, 0.03f, 1.0f}; //śladowe
GLfloat Global::lightPos[4];
bool Global::bRollFix = true; // czy wykonać przeliczanie przechyłki
bool Global::bJoinEvents = false; // czy grupować eventy o tych samych nazwach
int Global::iHiddenEvents = 1; // czy łączyć eventy z torami poprzez nazwę toru

// parametry użytkowe (jak komu pasuje)
int Global::Keys[MaxKeys];
int Global::iWindowWidth = 800;
int Global::iWindowHeight = 600;
float Global::fDistanceFactor = 768.0; // baza do przeliczania odległości dla LoD
int Global::iFeedbackMode = 1; // tryb pracy informacji zwrotnej
int Global::iFeedbackPort = 0; // dodatkowy adres dla informacji zwrotnych
bool Global::bFreeFly = false;
bool Global::bFullScreen = false;
bool Global::bInactivePause = true; // automatyczna pauza, gdy okno nieaktywne
float Global::fMouseXScale = 1.5;
float Global::fMouseYScale = 0.2;
std::string Global::SceneryFile = "td.scn";
std::string Global::asHumanCtrlVehicle = "EU07-424";
int Global::iMultiplayer = 0; // blokada działania niektórych funkcji na rzecz komunikacji
double Global::fMoveLight = -1; // ruchome światło
double Global::fLatitudeDeg = 52.0; // szerokość geograficzna
float Global::fFriction = 1.0; // mnożnik tarcia - KURS90
double Global::fBrakeStep = 1.0; // krok zmiany hamulca dla klawiszy [Num3] i [Num9]
std::string Global::asLang = "pl"; // domyślny język - http://tools.ietf.org/html/bcp47

// parametry wydajnościowe (np. regulacja FPS, szybkość wczytywania)
bool Global::bAdjustScreenFreq = true;
bool Global::bEnableTraction = true;
bool Global::bLoadTraction = true;
bool Global::bLiveTraction = true;
int Global::iDefaultFiltering = 9; // domyślne rozmywanie tekstur TGA bez alfa
int Global::iBallastFiltering = 9; // domyślne rozmywanie tekstur podsypki
int Global::iRailProFiltering = 5; // domyślne rozmywanie tekstur szyn
int Global::iDynamicFiltering = 5; // domyślne rozmywanie tekstur pojazdów
bool Global::bUseVBO = false; // czy jest VBO w karcie graficznej (czy użyć)
GLint Global::iMaxTextureSize = 16384; // maksymalny rozmiar tekstury
bool Global::bSmoothTraction = false; // wygładzanie drutów starym sposobem
char **Global::szDefaultExt = Global::szTexturesDDS; // domyślnie od DDS
int Global::iMultisampling = 2; // tryb antyaliasingu: 0=brak,1=2px,2=4px,3=8px,4=16px
bool Global::bGlutFont = false; // czy tekst generowany przez GLUT32.DLL
int Global::iConvertModels = 7; // tworzenie plików binarnych, +2-optymalizacja transformów
int Global::iSlowMotionMask = -1; // maska wyłączanych właściwości dla zwiększenia FPS
int Global::iModifyTGA = 7; // czy korygować pliki TGA dla szybszego wczytywania
// bool Global::bTerrainCompact=true; //czy zapisać teren w pliku
TAnimModel *Global::pTerrainCompact = NULL; // do zapisania terenu w pliku
std::string Global::asTerrainModel = ""; // nazwa obiektu terenu do zapisania w pliku
double Global::fFpsAverage = 20.0; // oczekiwana wartosć FPS
double Global::fFpsDeviation = 5.0; // odchylenie standardowe FPS
double Global::fFpsMin = 0.0; // dolna granica FPS, przy której promień scenerii będzie zmniejszany
double Global::fFpsMax = 0.0; // górna granica FPS, przy której promień scenerii będzie zwiększany
double Global::fFpsRadiusMax = 3000.0; // maksymalny promień renderowania
int Global::iFpsRadiusMax = 225; // maksymalny promień renderowania
double Global::fRadiusFactor = 1.1; // współczynnik jednorazowej zmiany promienia scenerii
bool Global::bOldSmudge = false; // Używanie starej smugi

// parametry testowe (do testowania scenerii i obiektów)
bool Global::bWireFrame = false;
bool Global::bSoundEnabled = true;
int Global::iWriteLogEnabled = 3; // maska bitowa: 1-zapis do pliku, 2-okienko, 4-nazwy torów
bool Global::bManageNodes = true;
bool Global::bDecompressDDS = false; // czy programowa dekompresja DDS

// parametry do kalibracji
// kolejno współczynniki dla potęg 0, 1, 2, 3 wartości odczytanej z urządzenia
double Global::fCalibrateIn[6][6] = {{0, 1, 0, 0, 0, 0},
                                     {0, 1, 0, 0, 0, 0},
                                     {0, 1, 0, 0, 0, 0},
                                     {0, 1, 0, 0, 0, 0},
                                     {0, 1, 0, 0, 0, 0},
                                     {0, 1, 0, 0, 0, 0}};
double Global::fCalibrateOut[7][6] = {{0, 1, 0, 0, 0, 0},
                                      {0, 1, 0, 0, 0, 0},
                                      {0, 1, 0, 0, 0, 0},
                                      {0, 1, 0, 0, 0, 0},
                                      {0, 1, 0, 0, 0, 0},
                                      {0, 1, 0, 0, 0, 0},
                                      {0, 1, 0, 0, 0, 0}};
double Global::fCalibrateOutMax[7] = {0, 0, 0, 0, 0, 0, 0};
int Global::iCalibrateOutDebugInfo = -1;
int Global::iPoKeysPWM[7] = {0, 1, 2, 3, 4, 5, 6};
// parametry przejściowe (do usunięcia)
// bool Global::bTimeChange=false; //Ra: ZiomalCl wyłączył starą wersję nocy
// bool Global::bRenderAlpha=true; //Ra: wywaliłam tę flagę
bool Global::bnewAirCouplers = true;
bool Global::bDoubleAmbient = false; // podwójna jasność ambient
double Global::fTimeSpeed = 1.0; // przyspieszenie czasu, zmienna do testów
bool Global::bHideConsole = false; // hunter-271211: ukrywanie konsoli
int Global::iBpp = 32; // chyba już nie używa się kart, na których 16bpp coś poprawi
// maciek001: konfiguracja wstępna portu COM
bool Global::bMWDdebugEnable = false;
bool Global::bMWDInputDataEnable = false;
unsigned int Global::iMWDBaudrate = 500000;
std::string Global::sMWDPortId = "COM1";		// nazwa portu z którego korzystamy - na razie nie działa
bool Global::bMWDBreakEnable = false;		// zmienić na FALSE!!! jak już będzie działać wczytywanie z *.ini
double Global::fMWDAnalogCalib[4][3] = {{1023, 0, 1023},{1023, 0, 1023},{1023, 0, 1023},{1023, 0, 1023}};	// wartość max potencjometru, wartość min potencjometru, rozdzielczość (max. wartość jaka może być -1)
double Global::fMWDzg[2] = {0.9, 1023};
double Global::fMWDpg[2] = {0.8, 1023};
double Global::fMWDph[2] = {0.6, 1023};
double Global::fMWDvolt[2] = {4000, 1023};
double Global::fMWDamp[2] = {800, 1023};

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

std::string Global::GetNextSymbol()
{ // pobranie tokenu z aktualnego parsera

    std::string token;
    if (pParser != nullptr)
    {

        pParser->getTokens();
        *pParser >> token;
    };
    return token;
};

void Global::LoadIniFile(std::string asFileName)
{
    for (int i = 0; i < 10; ++i)
    { // zerowanie pozycji kamer
        pFreeCameraInit[i] = vector3(0, 0, 0); // współrzędne w scenerii
        pFreeCameraInitAngle[i] = vector3(0, 0, 0); // kąty obrotu w radianach
    }
    cParser parser(asFileName, cParser::buffer_FILE);
    ConfigParse(parser);
};

void Global::ConfigParse(cParser &Parser)
{

    std::string token;
    do
    {

        token = "";
        Parser.getTokens();
        Parser >> token;

        if (token == "sceneryfile")
        {

            Parser.getTokens();
            Parser >> Global::SceneryFile;
        }
        else if (token == "humanctrlvehicle")
        {

            Parser.getTokens();
            Parser >> Global::asHumanCtrlVehicle;
        }
        else if (token == "width")
        {

            Parser.getTokens(1, false);
            Parser >> Global::iWindowWidth;
        }
        else if (token == "height")
        {

            Parser.getTokens(1, false);
            Parser >> Global::iWindowHeight;
        }
        else if (token == "heightbase")
        {

            Parser.getTokens(1, false);
            Parser >> Global::fDistanceFactor;
        }
        else if (token == "bpp")
        {

            Parser.getTokens();
            Parser >> token;
            Global::iBpp = (token == "32" ? 32 : 16);
        }
        else if (token == "fullscreen")
        {

            Parser.getTokens();
            Parser >> token;
            Global::bFullScreen = (token == "yes");
        }
        else if (token == "freefly")
        { // Mczapkie-130302

            Parser.getTokens();
            Parser >> token;
            Global::bFreeFly = (token == "yes");
            Parser.getTokens(3, false);
            Parser >> Global::pFreeCameraInit[0].x, Global::pFreeCameraInit[0].y,
                Global::pFreeCameraInit[0].z;
        }
        else if (token == "wireframe")
        {

            Parser.getTokens();
            Parser >> token;
            Global::bWireFrame = (token == "yes");
        }
        else if (token == "debugmode")
        { // McZapkie! - DebugModeFlag uzywana w mover.pas,
            // warto tez blokowac cheaty gdy false
            Parser.getTokens();
            Parser >> token;
            DebugModeFlag = (token == "yes");
        }
        else if (token == "soundenabled")
        { // McZapkie-040302 - blokada dzwieku - przyda
            // sie do debugowania oraz na komp. bez karty
            // dzw.
            Parser.getTokens();
            Parser >> token;
            Global::bSoundEnabled = (token == "yes");
        }
        // else if (str==AnsiString("renderalpha")) //McZapkie-1312302 - dwuprzebiegowe renderowanie
        // bRenderAlpha=(GetNextSymbol().LowerCase()==AnsiString("yes"));
        else if (token == "physicslog")
        { // McZapkie-030402 - logowanie parametrow
            // fizycznych dla kazdego pojazdu z maszynista
            Parser.getTokens();
            Parser >> token;
            WriteLogFlag = (token == "yes");
        }
        else if (token == "physicsdeactivation")
        { // McZapkie-291103 - usypianie fizyki

            Parser.getTokens();
            Parser >> token;
            PhysicActivationFlag = (token == "yes");
        }
        else if (token == "debuglog")
        {
            // McZapkie-300402 - wylaczanie log.txt
            Parser.getTokens();
            Parser >> token;
            if (token == "yes")
            {
                Global::iWriteLogEnabled = 3;
            }
            else if (token == "no")
            {
                Global::iWriteLogEnabled = 0;
            }
            else
            {
                Global::iWriteLogEnabled = stol_def(token,3);
            }
        }
        else if (token == "adjustscreenfreq")
        {
            // McZapkie-240403 - czestotliwosc odswiezania ekranu
            Parser.getTokens();
            Parser >> token;
            Global::bAdjustScreenFreq = (token == "yes");
        }
        else if (token == "mousescale")
        {
            // McZapkie-060503 - czulosc ruchu myszy (krecenia glowa)
            Parser.getTokens(2, false);
            Parser >> Global::fMouseXScale >> Global::fMouseYScale;
        }
        else if (token == "enabletraction")
        {
            // Winger 040204 - 'zywe' patyki dostosowujace sie do trakcji; Ra 2014-03: teraz łamanie
            Parser.getTokens();
            Parser >> token;
            Global::bEnableTraction = (token == "yes");
        }
        else if (token == "loadtraction")
        {
            // Winger 140404 - ladowanie sie trakcji
            Parser.getTokens();
            Parser >> token;
            Global::bLoadTraction = (token == "yes");
        }
        else if (token == "friction")
        { // mnożnik tarcia - KURS90

            Parser.getTokens(1, false);
            Parser >> Global::fFriction;
        }
        else if (token == "livetraction")
        {
            // Winger 160404 - zaleznosc napiecia loka od trakcji;
            // Ra 2014-03: teraz prąd przy braku sieci
            Parser.getTokens();
            Parser >> token;
            Global::bLiveTraction = (token == "yes");
        }
        else if (token == "skyenabled")
        {
            // youBy - niebo
            Parser.getTokens();
            Parser >> token;
            Global::asSky = (token == "yes" ? "1" : "0");
        }
        else if (token == "managenodes")
        {

            Parser.getTokens();
            Parser >> token;
            Global::bManageNodes = (token == "yes");
        }
        else if (token == "decompressdds")
        {

            Parser.getTokens();
            Parser >> token;
            Global::bDecompressDDS = (token == "yes");
        }
        else if (token == "defaultext")
        {
            // ShaXbee - domyslne rozszerzenie tekstur
            Parser.getTokens();
            Parser >> token;
            if (token == "tga")
            {
                // domyślnie od TGA
                Global::szDefaultExt = Global::szTexturesTGA;
            }
        }
        else if (token == "newaircouplers")
        {

            Parser.getTokens();
            Parser >> token;
            Global::bnewAirCouplers = (token == "yes");
        }
        else if (token == "defaultfiltering")
        {

            Parser.getTokens(1, false);
            Parser >> Global::iDefaultFiltering;
        }
        else if (token == "ballastfiltering")
        {

            Parser.getTokens(1, false);
            Parser >> Global::iBallastFiltering;
        }
        else if (token == "railprofiltering")
        {

            Parser.getTokens(1, false);
            Parser >> Global::iRailProFiltering;
        }
        else if (token == "dynamicfiltering")
        {

            Parser.getTokens(1, false);
            Parser >> Global::iDynamicFiltering;
        }
        else if (token == "usevbo")
        {

            Parser.getTokens();
            Parser >> token;
            Global::bUseVBO = (token == "yes");
        }
        else if (token == "feedbackmode")
        {

            Parser.getTokens(1, false);
            Parser >> Global::iFeedbackMode;
        }
        else if (token == "feedbackport")
        {

            Parser.getTokens(1, false);
            Parser >> Global::iFeedbackPort;
        }
        else if (token == "multiplayer")
        {

            Parser.getTokens(1, false);
            Parser >> Global::iMultiplayer;
        }
        else if (token == "maxtexturesize")
        {
            // wymuszenie przeskalowania tekstur
            Parser.getTokens(1, false);
            int size;
            Parser >> size;
            if (size <= 64)
            {
                Global::iMaxTextureSize = 64;
            }
            else if (size <= 128)
            {
                Global::iMaxTextureSize = 128;
            }
            else if (size <= 256)
            {
                Global::iMaxTextureSize = 256;
            }
            else if (size <= 512)
            {
                Global::iMaxTextureSize = 512;
            }
            else if (size <= 1024)
            {
                Global::iMaxTextureSize = 1024;
            }
            else if (size <= 2048)
            {
                Global::iMaxTextureSize = 2048;
            }
            else if (size <= 4096)
            {
                Global::iMaxTextureSize = 4096;
            }
            else if (size <= 8192)
            {
                Global::iMaxTextureSize = 8192;
            }
            else
            {
                Global::iMaxTextureSize = 16384;
            }
        }
        else if (token == "doubleambient")
        {
            // podwójna jasność ambient
            Parser.getTokens();
            Parser >> token;
            Global::bDoubleAmbient = (token == "yes");
        }
        else if (token == "movelight")
        {
            // numer dnia w roku albo -1
            Parser.getTokens(1, false);
            Parser >> Global::fMoveLight;
            if (Global::fMoveLight == 0.f)
            { // pobranie daty z systemu
                std::time_t timenow = std::time(0);
                std::tm *localtime = std::localtime(&timenow);
                Global::fMoveLight = localtime->tm_yday + 1; // numer bieżącego dnia w roku
            }
            if (fMoveLight > 0.f) // tu jest nadal zwiększone o 1
            { // obliczenie deklinacji wg:
                // http://naturalfrequency.com/Tregenza_Sharples/Daylight_Algorithms/algorithm_1_11.htm
                // Spencer J W Fourier series representation of the position of the sun Search 2 (5)
                // 172 (1971)
                Global::fMoveLight =
                    M_PI / 182.5 * (Global::fMoveLight - 1.0); // numer dnia w postaci kąta
                fSunDeclination =
                    0.006918 - 0.3999120 * std::cos(fMoveLight) + 0.0702570 * std::sin(fMoveLight) -
                    0.0067580 * std::cos(2 * fMoveLight) + 0.0009070 * std::sin(2 * fMoveLight) -
                    0.0026970 * std::cos(3 * fMoveLight) + 0.0014800 * std::sin(3 * fMoveLight);
            }
        }
        else if (token == "smoothtraction")
        {
            // podwójna jasność ambient
            Parser.getTokens();
            Parser >> token;
            Global::bSmoothTraction = (token == "yes");
        }
        else if (token == "timespeed")
        {
            // przyspieszenie czasu, zmienna do testów
            Parser.getTokens(1, false);
            Parser >> Global::fTimeSpeed;
        }
        else if (token == "multisampling")
        {
            // tryb antyaliasingu: 0=brak,1=2px,2=4px
            Parser.getTokens(1, false);
            Parser >> Global::iMultisampling;
        }
        else if (token == "glutfont")
        {
            // tekst generowany przez GLUT
            Parser.getTokens();
            Parser >> token;
            Global::bGlutFont = (token == "yes");
        }
        else if (token == "latitude")
        {
            // szerokość geograficzna
            Parser.getTokens(1, false);
            Parser >> Global::fLatitudeDeg;
        }
        else if (token == "convertmodels")
        {
            // tworzenie plików binarnych
            Parser.getTokens(1, false);
            Parser >> Global::iConvertModels;
        }
        else if (token == "inactivepause")
        {
            // automatyczna pauza, gdy okno nieaktywne
            Parser.getTokens();
            Parser >> token;
            Global::bInactivePause = (token == "yes");
        }
        else if (token == "slowmotion")
        {
            // tworzenie plików binarnych
            Parser.getTokens(1, false);
            Parser >> Global::iSlowMotionMask;
        }
        else if (token == "modifytga")
        {
            // czy korygować pliki TGA dla szybszego wczytywania
            Parser.getTokens(1, false);
            Parser >> Global::iModifyTGA;
        }
        else if (token == "hideconsole")
        {
            // hunter-271211: ukrywanie konsoli
            Parser.getTokens();
            Parser >> token;
            Global::bHideConsole = (token == "yes");
        }
        else if (token == "oldsmudge")
        {

            Parser.getTokens();
            Parser >> token;
            Global::bOldSmudge = (token == "yes");
        }
        else if (token == "rollfix")
        {
            // Ra: poprawianie przechyłki, aby wewnętrzna szyna była "pozioma"
            Parser.getTokens();
            Parser >> token;
            Global::bRollFix = (token == "yes");
        }
        else if (token == "fpsaverage")
        {
            // oczekiwana wartość FPS
            Parser.getTokens(1, false);
            Parser >> Global::fFpsAverage;
        }
        else if (token == "fpsdeviation")
        {
            // odchylenie standardowe FPS
            Parser.getTokens(1, false);
            Parser >> Global::fFpsDeviation;
        }
        else if (token == "fpsradiusmax")
        {
            // maksymalny promień renderowania
            Parser.getTokens(1, false);
            Parser >> Global::fFpsRadiusMax;
        }
        else if (token == "calibratein")
        {
            // parametry kalibracji wejść
            Parser.getTokens(1, false);
            int in;
            Parser >> in;
            if ((in < 0) || (in > 5))
            {
                in = 5; // na ostatni, bo i tak trzeba pominąć wartości
            }
            Parser.getTokens(4, false);
            Parser >> Global::fCalibrateIn[in][0] // wyraz wolny
                >> Global::fCalibrateIn[in][1] // mnożnik
                >> Global::fCalibrateIn[in][2] // mnożnik dla kwadratu
                >> Global::fCalibrateIn[in][3]; // mnożnik dla sześcianu
            Global::fCalibrateIn[in][4] = 0.0; // mnożnik 4 potęgi
            Global::fCalibrateIn[in][5] = 0.0; // mnożnik 5 potęgi
        }
        else if (token == "calibrate5din")
        {
            // parametry kalibracji wejść
            Parser.getTokens(1, false);
            int in;
            Parser >> in;
            if ((in < 0) || (in > 5))
            {
                in = 5; // na ostatni, bo i tak trzeba pominąć wartości
            }
            Parser.getTokens(6, false);
            Parser >> Global::fCalibrateIn[in][0] // wyraz wolny
                >> Global::fCalibrateIn[in][1] // mnożnik
                >> Global::fCalibrateIn[in][2] // mnożnik dla kwadratu
                >> Global::fCalibrateIn[in][3] // mnożnik dla sześcianu
                >> Global::fCalibrateIn[in][4] // mnożnik 4 potęgi
                >> Global::fCalibrateIn[in][5]; // mnożnik 5 potęgi
        }
        else if (token == "calibrateout")
        {
            // parametry kalibracji wyjść
            Parser.getTokens(1, false);
            int out;
            Parser >> out;
            if ((out < 0) || (out > 6))
            {
                out = 6; // na ostatni, bo i tak trzeba pominąć wartości
            }
            Parser.getTokens(4, false);
            Parser >> Global::fCalibrateOut[out][0] // wyraz wolny
                >> Global::fCalibrateOut[out][1] // mnożnik liniowy
                >> Global::fCalibrateOut[out][2] // mnożnik dla kwadratu
                >> Global::fCalibrateOut[out][3]; // mnożnik dla sześcianu
            Global::fCalibrateOut[out][4] = 0.0; // mnożnik dla 4 potęgi
            Global::fCalibrateOut[out][5] = 0.0; // mnożnik dla 5 potęgi
        }
        else if (token == "calibrate5dout")
        {
            // parametry kalibracji wyjść
            Parser.getTokens(1, false);
            int out;
            Parser >> out;
            if ((out < 0) || (out > 6))
            {
                out = 6; // na ostatni, bo i tak trzeba pominąć wartości
            }
            Parser.getTokens(6, false);
            Parser >> Global::fCalibrateOut[out][0] // wyraz wolny
                >> Global::fCalibrateOut[out][1] // mnożnik liniowy
                >> Global::fCalibrateOut[out][2] // mnożnik dla kwadratu
                >> Global::fCalibrateOut[out][3] // mnożnik dla sześcianu
                >> Global::fCalibrateOut[out][4] // mnożnik dla 4 potęgi
                >> Global::fCalibrateOut[out][5]; // mnożnik dla 5 potęgi
        }
        else if (token == "calibrateoutmaxvalues")
        {
            // maksymalne wartości jakie można wyświetlić na mierniku
            Parser.getTokens(7, false);
            Parser >> Global::fCalibrateOutMax[0] >> Global::fCalibrateOutMax[1] >>
                Global::fCalibrateOutMax[2] >> Global::fCalibrateOutMax[3] >>
                Global::fCalibrateOutMax[4] >> Global::fCalibrateOutMax[5] >>
                Global::fCalibrateOutMax[6];
        }
        else if (token == "calibrateoutdebuginfo")
        {
            // wyjście z info o przebiegu kalibracji
            Parser.getTokens(1, false);
            Parser >> Global::iCalibrateOutDebugInfo;
        }
        else if (token == "pwm")
        {
            // zmiana numerów wyjść PWM
            Parser.getTokens(2, false);
            int pwm_out, pwm_no;
            Parser >> pwm_out >> pwm_no;
            Global::iPoKeysPWM[pwm_out] = pwm_no;
        }
        else if (token == "brakestep")
        {
            // krok zmiany hamulca dla klawiszy [Num3] i [Num9]
            Parser.getTokens(1, false);
            Parser >> Global::fBrakeStep;
        }
        else if (token == "joinduplicatedevents")
        {
            // czy grupować eventy o tych samych nazwach
            Parser.getTokens();
            Parser >> token;
            Global::bJoinEvents = (token == "yes");
        }
        else if (token == "hiddenevents")
        {
            // czy łączyć eventy z torami poprzez nazwę toru
            Parser.getTokens(1, false);
            Parser >> Global::iHiddenEvents;
        }
        else if (token == "pause")
        {
            // czy po wczytaniu ma być pauza?
            Parser.getTokens();
            Parser >> token;
            iPause |= (token == "yes" ? 1 : 0);
        }
        else if (token == "lang")
        {
            // domyślny język - http://tools.ietf.org/html/bcp47
            Parser.getTokens(1, false);
            Parser >> Global::asLang;
        }
        else if (token == "opengl")
        {
            // deklarowana wersja OpenGL, żeby powstrzymać błędy
            Parser.getTokens(1, false);
            Parser >> Global::fOpenGL;
        }
        else if (token == "pyscreenrendererpriority")
        {
            // priority of python screen renderer
            Parser.getTokens();
            Parser >> token;
            TPythonInterpreter::getInstance()->setScreenRendererPriority(token.c_str());
        }
        else if (token == "background")
        {

            Parser.getTokens(3, false);
            Parser >> Global::Background[0] // r
                >> Global::Background[1] // g
                >> Global::Background[2]; // b
        }
        // maciek001: ustawienia MWD
        else if (token == "mwddebug")
        { // czy włączyć obslugę hamulców
            Parser.getTokens();
            Parser >> token;
            bMWDdebugEnable = (token == "yes");
        }
        else if (token == "comportname")
        {
            Parser.getTokens();
            Parser >> sMWDPortId;
            if (bMWDdebugEnable)
                WriteLog("PortName " + sMWDPortId);
        }
        else if (token == "mwdbaudrate")
        { // pobierz prędkość transmisji danych
            Parser.getTokens(1, false);
            Parser >> iMWDBaudrate;
            if (bMWDdebugEnable)
                WriteLog("PortName " + to_string(iMWDBaudrate));
        }
        else if (token == "mwdbreakenable")
        { // czy włączyć obsługę hamulców
            Parser.getTokens();
            Parser >> token;
            bMWDBreakEnable = (token == "yes");
        }
        else if (token == "mwdinputenable")
        {
            Parser.getTokens();
            Parser >> token;
            bMWDInputDataEnable = (token == "yes");
        }
        else if (token == "mwdbreak") // wartość max dla potencjometru hamulca zasadniczego
        {
            Parser.getTokens();
            Parser >> token;
            int i = stol_def(token, -1); // numer wejďż˝cia
            if ((i >= 0) || (i <= 1))
            {
                Parser.getTokens(3, false);
                Parser >> fMWDAnalogCalib[i][0] // max -> 2^16 -1
                    >> fMWDAnalogCalib[i][1] // min -> 0
                    >>
                    fMWDAnalogCalib[i][2]; // rozdzielczość -> 255 maksymalna możliwa wartość z ADC
                if (bMWDdebugEnable)
                    WriteLog("Break settings " + to_string(i) + ": " +
                             to_string(fMWDAnalogCalib[i][0]) + " " +
                             to_string(fMWDAnalogCalib[i][1]) + " " +
                             to_string(fMWDAnalogCalib[i][2]));
            }
        }
        else if (token == "mwdzbiornikglowny")
        {
            Parser.getTokens(2, false);
            Parser >> fMWDzg[0] >> fMWDzg[1];
            if (bMWDdebugEnable)
                WriteLog("AirTank settings: " + to_string(fMWDzg[0]) + " " + to_string(fMWDzg[1]));
        }
        else if (token == "mwdprzewodglowny")
        {
            Parser.getTokens(2, false);
            Parser >> fMWDpg[0] >> fMWDpg[1];
            if (bMWDdebugEnable)
                WriteLog("MainAirPipe settings: " + to_string(fMWDpg[0]) + " " +
                         to_string(fMWDpg[1]));
        }
        else if (token == "mwdcylinderhamulcowy")
        {
            Parser.getTokens(2, false);
            Parser >> fMWDph[0] >> fMWDph[1];
            if (bMWDdebugEnable)
                WriteLog("AirPipe settings: " + to_string(fMWDph[0]) + " " + to_string(fMWDph[1]));
        }
        else if (token == "mwdwoltomierzwn")
        {
            Parser.getTokens(2, false);
            Parser >> fMWDvolt[0] >> fMWDvolt[1];
            if (bMWDdebugEnable)
                WriteLog("Volt settings: " + to_string(fMWDvolt[0]) + " " + to_string(fMWDvolt[1]));
        }
        else if (token == "mwdamperomierzwn")
        {
            Parser.getTokens(2, false);
            Parser >> fMWDamp[0] >> fMWDamp[1];
            if (bMWDdebugEnable)
                WriteLog("Amp settings: " + to_string(fMWDamp[0]) + " " + to_string(fMWDamp[1]));
        }
    } while ((token != "") && (token != "endconfig")); //(!Parser->EndOfFile)
    // na koniec trochę zależności
    if (!bLoadTraction) // wczytywanie drutów i słupów
    { // tutaj wyłączenie, bo mogą nie być zdefiniowane w INI
        bEnableTraction = false; // false = pantograf się nie połamie
        bLiveTraction = false; // false = pantografy zawsze zbierają 95% MaxVoltage
    }
    // if (fMoveLight>0) bDoubleAmbient=false; //wtedy tylko jedno światło ruchome
    // if (fOpenGL<1.3) iMultisampling=0; //można by z góry wyłączyć, ale nie mamy jeszcze fOpenGL
    if (iMultisampling)
    { // antyaliasing całoekranowy wyłącza rozmywanie drutów
        bSmoothTraction = false;
    }
    if (iMultiplayer > 0)
    {
        bInactivePause = false; // okno "w tle" nie może pauzować, jeśli włączona komunikacja
        // pauzowanie jest zablokowane dla (iMultiplayer&2)>0, więc iMultiplayer=1 da się zapauzować
        // (tryb instruktora)
    }
    fFpsMin = fFpsAverage -
              fFpsDeviation; // dolna granica FPS, przy której promień scenerii będzie zmniejszany
    fFpsMax = fFpsAverage +
              fFpsDeviation; // górna granica FPS, przy której promień scenerii będzie zwiększany
    if (iPause)
        iTextMode = VK_F1; // jak pauza, to pokazać zegar
    /*  this won't execute anymore with the old parser removed
            // TBD: remove, or launch depending on passed flag?
        if (qp)
    { // to poniżej wykonywane tylko raz, jedynie po wczytaniu eu07.ini
            Console::ModeSet(iFeedbackMode, iFeedbackPort); // tryb pracy konsoli sterowniczej
            iFpsRadiusMax = 0.000025 * fFpsRadiusMax *
                        fFpsRadiusMax; // maksymalny promień renderowania 3000.0 -> 225
            if (iFpsRadiusMax > 400)
                iFpsRadiusMax = 400;
            if (fDistanceFactor > 1.0)
            { // dla 1.0 specjalny tryb bez przeliczania
                fDistanceFactor =
                    iWindowHeight /
                fDistanceFactor; // fDistanceFactor>1.0 dla rozdzielczości większych niż bazowa
                fDistanceFactor *=
                    (iMultisampling + 1.0) *
                fDistanceFactor; // do kwadratu, bo większość odległości to ich kwadraty
            }
        }
    */
}

void Global::InitKeys(std::string asFileName)
{
    //    if (FileExists(asFileName))
    //    {
    //       Error("Chwilowo plik keys.ini nie jest obsługiwany. Ładuję standardowe
    //       ustawienia.\nKeys.ini file is temporarily not functional, loading default keymap...");
    /*        TQueryParserComp *Parser;
            Parser=new TQueryParserComp(NULL);
            Parser->LoadStringToParse(asFileName);

            for (int keycount=0; keycount<MaxKeys; keycount++)
             {
              Keys[keycount]=Parser->GetNextSymbol().ToInt();
             }

            delete Parser;
    */
    //    }
    //    else
    {
        Keys[k_IncMainCtrl] = VK_ADD;
        Keys[k_IncMainCtrlFAST] = VK_ADD;
        Keys[k_DecMainCtrl] = VK_SUBTRACT;
        Keys[k_DecMainCtrlFAST] = VK_SUBTRACT;
        Keys[k_IncScndCtrl] = VK_DIVIDE;
        Keys[k_IncScndCtrlFAST] = VK_DIVIDE;
        Keys[k_DecScndCtrl] = VK_MULTIPLY;
        Keys[k_DecScndCtrlFAST] = VK_MULTIPLY;
        ///*NORMALNE
        Keys[k_IncLocalBrakeLevel] = VK_NUMPAD1; // VK_NUMPAD7;
        // Keys[k_IncLocalBrakeLevelFAST]=VK_END;  //VK_HOME;
        Keys[k_DecLocalBrakeLevel] = VK_NUMPAD7; // VK_NUMPAD1;
        // Keys[k_DecLocalBrakeLevelFAST]=VK_HOME; //VK_END;
        Keys[k_IncBrakeLevel] = VK_NUMPAD3; // VK_NUMPAD9;
        Keys[k_DecBrakeLevel] = VK_NUMPAD9; // VK_NUMPAD3;
        Keys[k_Releaser] = VK_NUMPAD6;
        Keys[k_EmergencyBrake] = VK_NUMPAD0;
        Keys[k_Brake3] = VK_NUMPAD8;
        Keys[k_Brake2] = VK_NUMPAD5;
        Keys[k_Brake1] = VK_NUMPAD2;
        Keys[k_Brake0] = VK_NUMPAD4;
        Keys[k_WaveBrake] = VK_DECIMAL;
        //*/
        /*MOJE
                Keys[k_IncLocalBrakeLevel]=VK_NUMPAD3;  //VK_NUMPAD7;
                Keys[k_IncLocalBrakeLevelFAST]=VK_NUMPAD3;  //VK_HOME;
                Keys[k_DecLocalBrakeLevel]=VK_DECIMAL;  //VK_NUMPAD1;
                Keys[k_DecLocalBrakeLevelFAST]=VK_DECIMAL; //VK_END;
                Keys[k_IncBrakeLevel]=VK_NUMPAD6;  //VK_NUMPAD9;
                Keys[k_DecBrakeLevel]=VK_NUMPAD9;   //VK_NUMPAD3;
                Keys[k_Releaser]=VK_NUMPAD5;
                Keys[k_EmergencyBrake]=VK_NUMPAD0;
                Keys[k_Brake3]=VK_NUMPAD2;
                Keys[k_Brake2]=VK_NUMPAD1;
                Keys[k_Brake1]=VK_NUMPAD4;
                Keys[k_Brake0]=VK_NUMPAD7;
                Keys[k_WaveBrake]=VK_NUMPAD8;
        */
        Keys[k_AntiSlipping] = VK_RETURN;
        Keys[k_Sand] = VkKeyScan('s');
        Keys[k_Main] = VkKeyScan('m');
        Keys[k_Active] = VkKeyScan('w');
        Keys[k_Battery] = VkKeyScan('j');
        Keys[k_DirectionForward] = VkKeyScan('d');
        Keys[k_DirectionBackward] = VkKeyScan('r');
        Keys[k_Fuse] = VkKeyScan('n');
        Keys[k_Compressor] = VkKeyScan('c');
        Keys[k_Converter] = VkKeyScan('x');
        Keys[k_MaxCurrent] = VkKeyScan('f');
        Keys[k_CurrentAutoRelay] = VkKeyScan('g');
        Keys[k_BrakeProfile] = VkKeyScan('b');
        Keys[k_CurrentNext] = VkKeyScan('z');

        Keys[k_Czuwak] = VkKeyScan(' ');
        Keys[k_Horn] = VkKeyScan('a');
        Keys[k_Horn2] = VkKeyScan('a');

        Keys[k_FailedEngineCutOff] = VkKeyScan('e');

        Keys[k_MechUp] = VK_PRIOR;
        Keys[k_MechDown] = VK_NEXT;
        Keys[k_MechLeft] = VK_LEFT;
        Keys[k_MechRight] = VK_RIGHT;
        Keys[k_MechForward] = VK_UP;
        Keys[k_MechBackward] = VK_DOWN;

        Keys[k_CabForward] = VK_HOME;
        Keys[k_CabBackward] = VK_END;

        Keys[k_Couple] = VK_INSERT;
        Keys[k_DeCouple] = VK_DELETE;

        Keys[k_ProgramQuit] = VK_F10;
        // Keys[k_ProgramPause]=VK_F3;
        Keys[k_ProgramHelp] = VK_F1;
        // Keys[k_FreeFlyMode]=VK_F4;
        Keys[k_WalkMode] = VK_F5;

        Keys[k_OpenLeft] = VkKeyScan(',');
        Keys[k_OpenRight] = VkKeyScan('.');
        Keys[k_CloseLeft] = VkKeyScan(',');
        Keys[k_CloseRight] = VkKeyScan('.');
        Keys[k_DepartureSignal] = VkKeyScan('/');

        // Winger 160204 - obsluga pantografow
        Keys[k_PantFrontUp] = VkKeyScan('p'); // Ra: zamieniony przedni z tylnym
        Keys[k_PantFrontDown] = VkKeyScan('p');
        Keys[k_PantRearUp] = VkKeyScan('o');
        Keys[k_PantRearDown] = VkKeyScan('o');
        // Winger 020304 - ogrzewanie
        Keys[k_Heating] = VkKeyScan('h');
        Keys[k_LeftSign] = VkKeyScan('y');
        Keys[k_UpperSign] = VkKeyScan('u');
        Keys[k_RightSign] = VkKeyScan('i');
        Keys[k_EndSign] = VkKeyScan('t');

        Keys[k_SmallCompressor] = VkKeyScan('v');
        Keys[k_StLinOff] = VkKeyScan('l');
        // ABu 090305 - przyciski uniwersalne, do roznych bajerow :)
        Keys[k_Univ1] = VkKeyScan('[');
        Keys[k_Univ2] = VkKeyScan(']');
        Keys[k_Univ3] = VkKeyScan(';');
        Keys[k_Univ4] = VkKeyScan('\'');
    }
}
/*
vector3 Global::GetCameraPosition()
{
    return pCameraPosition;
}
*/
void Global::SetCameraPosition(vector3 pNewCameraPosition)
{
    pCameraPosition = pNewCameraPosition;
}

void Global::SetCameraRotation(double Yaw)
{ // ustawienie bezwzględnego kierunku kamery z korekcją do przedziału <-M_PI,M_PI>
    pCameraRotation = Yaw;
    while (pCameraRotation < -M_PI)
        pCameraRotation += 2 * M_PI;
    while (pCameraRotation > M_PI)
        pCameraRotation -= 2 * M_PI;
    pCameraRotationDeg = pCameraRotation * 180.0 / M_PI;
}

void Global::BindTexture(GLuint t)
{ // ustawienie aktualnej tekstury, tylko gdy się zmienia
    if (t != iTextureId)
    {
        iTextureId = t;
    }
};

void Global::TrainDelete(TDynamicObject *d)
{ // usunięcie pojazdu prowadzonego przez użytkownika
    if (pWorld)
        pWorld->TrainDelete(d);
};

TDynamicObject *Global::DynamicNearest()
{ // ustalenie pojazdu najbliższego kamerze
    return pGround->DynamicNearest(pCamera->Pos);
};

TDynamicObject *Global::CouplerNearest()
{ // ustalenie pojazdu najbliższego kamerze
    return pGround->CouplerNearest(pCamera->Pos);
};

bool Global::AddToQuery(TEvent *event, TDynamicObject *who)
{
    return pGround->AddToQuery(event, who);
};
//---------------------------------------------------------------------------

bool Global::DoEvents()
{ // wywoływać czasem, żeby nie robił wrażenia zawieszonego
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return FALSE;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return TRUE;
}
//---------------------------------------------------------------------------

TTranscripts::TTranscripts()
{
    iCount = 0; // brak linijek do wyświetlenia
    iStart = 0; // wypełniać od linijki 0
    for (int i = 0; i < MAX_TRANSCRIPTS; ++i)
    { // to do konstruktora można by dać
        aLines[i].fHide = -1.0; // wolna pozycja (czas symulacji, 360.0 to doba)
        aLines[i].iNext = -1; // nie ma kolejnej
    }
    fRefreshTime = 360.0; // wartośc zaporowa
};
TTranscripts::~TTranscripts(){};
void TTranscripts::AddLine(char const *txt, float show, float hide, bool it)
{ // dodanie linii do tabeli, (show) i (hide) w [s] od aktualnego czasu
    if (show == hide)
        return; // komentarz jest ignorowany
    show = Global::fTimeAngleDeg + show / 240.0; // jeśli doba to 360, to 1s będzie równe 1/240
    hide = Global::fTimeAngleDeg + hide / 240.0;
    int i = iStart, j, k; // od czegoś trzeba zacząć
    while ((aLines[i].iNext >= 0) ? (aLines[aLines[i].iNext].fShow <= show) :
                                    false) // póki nie koniec i wcześniej puszczane
        i = aLines[i].iNext; // przejście do kolejnej linijki
    //(i) wskazuje na linię, po której należy wstawić dany tekst, chyba że
    while (txt ? *txt : false)
        for (j = 0; j < MAX_TRANSCRIPTS; ++j)
            if (aLines[j].fHide < 0.0)
            { // znaleziony pierwszy wolny
                aLines[j].iNext = aLines[i].iNext; // dotychczasowy następny będzie za nowym
                if (aLines[iStart].fHide < 0.0) // jeśli tablica jest pusta
                    iStart = j; // fHide trzeba sprawdzić przed ewentualnym nadpisaniem, gdy i=j=0
                else
                    aLines[i].iNext = j; // a nowy będzie za tamtym wcześniejszym
                aLines[j].fShow = show; // wyświetlać od
                aLines[j].fHide = hide; // wyświetlać do
                aLines[j].bItalic = it;
                aLines[j].asText = std::string(txt); // bez sensu, wystarczyłby wskaźnik
                if ((k = aLines[j].asText.find("|")) != std::string::npos)
                { // jak jest podział linijki na wiersze
                    aLines[j].asText = aLines[j].asText.substr(0, k - 1);
                    txt += k;
                    i = j; // kolejna linijka dopisywana będzie na koniec właśnie dodanej
                }
                else
                    txt = NULL; // koniec dodawania
                if (fRefreshTime > show) // jeśli odświeżacz ustawiony jest na później
                    fRefreshTime = show; // to odświeżyć wcześniej
                break; // więcej już nic
            }
};
void TTranscripts::Add(char const *txt, float len, bool backgorund)
{ // dodanie tekstów, długość dźwięku, czy istotne
    if (!txt)
        return; // pusty tekst
    int i = 0, j = int(0.5 + 10.0 * len); //[0.1s]
    if (*txt == '[')
    { // powinny być dwa nawiasy
        while (*++txt ? *txt != ']' : false)
            if ((*txt >= '0') && (*txt <= '9'))
                i = 10 * i + int(*txt - '0'); // pierwsza liczba aż do ]
        if (*txt ? *++txt == '[' : false)
        {
            j = 0; // drugi nawias określa czas zakończenia wyświetlania
            while (*++txt ? *txt != ']' : false)
                if ((*txt >= '0') && (*txt <= '9'))
                    j = 10 * j + int(*txt - '0'); // druga liczba aż do ]
            if (*txt)
                ++txt; // pominięcie drugiego ]
        }
    }
    AddLine(txt, 0.1 * i, 0.1 * j, false);
};
void TTranscripts::Update()
{ // usuwanie niepotrzebnych (nie częściej niż 10 razy na sekundę)
    if (fRefreshTime > Global::fTimeAngleDeg)
        return; // nie czas jeszcze na zmiany
    // czas odświeżenia można ustalić wg tabelki, kiedy coś się w niej zmienia
    fRefreshTime = Global::fTimeAngleDeg + 360.0; // wartość zaporowa
    int i = iStart, j = -1; // od czegoś trzeba zacząć
    bool change = false; // czy zmieniać napisy?
    do
    {
        if (aLines[i].fHide >= 0.0) // o ile aktywne
            if (aLines[i].fHide < Global::fTimeAngleDeg)
            { // gdy czas wyświetlania upłynął
                aLines[i].fHide = -1.0; // teraz będzie wolną pozycją
                if (i == iStart)
                    iStart = aLines[i].iNext >= 0 ? aLines[i].iNext : 0; // przestawienie pierwszego
                else if (j >= 0)
                    aLines[j].iNext = aLines[i].iNext; // usunięcie ze środka
                change = true;
            }
            else
            { // gdy ma być pokazane
                if (aLines[i].fShow > Global::fTimeAngleDeg) // będzie pokazane w przyszłości
                    if (fRefreshTime > aLines[i].fShow) // a nie ma nic wcześniej
                        fRefreshTime = aLines[i].fShow;
                if (fRefreshTime > aLines[i].fHide)
                    fRefreshTime = aLines[i].fHide;
            }
        // można by jeszcze wykrywać, które nowe mają być pokazane
        j = i;
        i = aLines[i].iNext; // kolejna linijka
    } while (i >= 0); // póki po tablicy
    change = true; // bo na razie nie ma warunku, że coś się dodało
    if (change)
    { // aktualizacja linijek ekranowych
        i = iStart;
        j = -1;
        do
        {
            if (aLines[i].fHide > 0.0) // jeśli nie ukryte
                if (aLines[i].fShow < Global::fTimeAngleDeg) // to dodanie linijki do wyświetlania
                    if (j < 5 - 1) // ograniczona liczba linijek
                        Global::asTranscript[++j] = aLines[i].asText; // skopiowanie tekstu
            i = aLines[i].iNext; // kolejna linijka
        } while (i >= 0); // póki po tablicy
        for (++j; j < 5; ++j)
            Global::asTranscript[j] = ""; // i czyszczenie nieużywanych linijek
    }
};

// Ra: tymczasowe rozwiązanie kwestii zagranicznych (czeskich) napisów
char bezogonkowo[] = "E?,?\"_++?%S<STZZ?`'\"\".--??s>stzz"
                        " ^^L$A|S^CS<--RZo±,l'uP.,as>L\"lz"
                     "RAAAALCCCEEEEIIDDNNOOOOxRUUUUYTB"
                     "raaaalccceeeeiiddnnoooo-ruuuuyt?";

std::string Global::Bezogonkow(std::string str, bool _)
{ // wycięcie liter z ogonkami, bo OpenGL nie umie wyświetlić
    for (unsigned int i = 1; i < str.length(); ++i)
        if (str[i] & 0x80)
            str[i] = bezogonkowo[str[i] & 0x7F];
        else if (str[i] < ' ') // znaki sterujące nie są obsługiwane
            str[i] = ' ';
        else if (_)
            if (str[i] == '_') // nazwy stacji nie mogą zawierać spacji
                str[i] = ' '; // więc trzeba wyświetlać inaczej
    return str;
};

double Global::Min0RSpeed(double vel1, double vel2)
{ // rozszerzenie funkcji Min0R o wartości -1.0
    if (vel1 == -1.0)
    {
        vel1 = std::numeric_limits<double>::max();
    }
    if (vel2 == -1.0)
    {
        vel2 = std::numeric_limits<double>::max();
    }
    return Min0R(vel1, vel2);
};

double Global::CutValueToRange(double min, double value, double max)
{ // przycinanie wartosci do podanych granic
    value = Max0R(value, min);
    value = Min0R(value, max);
    return value;
};
