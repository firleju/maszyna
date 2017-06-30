/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "GL/glew.h"
#include "Parser.h"
#include "dumb3d.h"
#include "Float3d.h"
#include "VBO.h"
#include "Texture.h"

using namespace Math3D;

// Ra: specjalne typy submodeli, poza tym GL_TRIANGLES itp.
const int TP_ROTATOR = 256;
const int TP_FREESPOTLIGHT = 257;
const int TP_STARS = 258;
const int TP_TEXT = 259;

enum TAnimType // rodzaj animacji
{
	at_None, // brak
	at_Rotate, // obrót względem wektora o kąt
	at_RotateXYZ, // obrót względem osi o kąty
	at_Translate, // przesunięcie
	at_SecondsJump, // sekundy z przeskokiem
	at_MinutesJump, // minuty z przeskokiem
	at_HoursJump, // godziny z przeskokiem 12h/360°
	at_Hours24Jump, // godziny z przeskokiem 24h/360°
	at_Seconds, // sekundy płynnie
	at_Minutes, // minuty płynnie
	at_Hours, // godziny płynnie 12h/360°
	at_Hours24, // godziny płynnie 24h/360°
	at_Billboard, // obrót w pionie do kamery
	at_Wind, // ruch pod wpływem wiatru
	at_Sky, // animacja nieba
	at_IK = 0x100, // odwrotna kinematyka - submodel sterujący (np. staw skokowy)
	at_IK11 = 0x101, // odwrotna kinematyka - submodel nadrzędny do sterowango (np. stopa)
	at_IK21 = 0x102, // odwrotna kinematyka - submodel nadrzędny do sterowango (np. podudzie)
	at_IK22 = 0x103, // odwrotna kinematyka - submodel nadrzędny do nadrzędnego sterowango (np. udo)
	at_Digital = 0x200, // dziesięciocyfrowy licznik mechaniczny (z cylindrami)
	at_DigiClk = 0x201, // zegar cyfrowy jako licznik na dziesięciościanach
	at_Undefined = 0x800000FF // animacja chwilowo nieokreślona
};

class TSubModel
{ // klasa submodelu - pojedyncza siatka, punkt świetlny albo grupa punktów
    //m7todo: zrobić normalną serializację

    friend class opengl_renderer;
    friend class TModel3d; // temporary workaround. TODO: clean up class content/hierarchy
    friend class TDynamicObject; // temporary etc

private:
    int iNext{ NULL };
    int iChild{ NULL };
    int eType{ TP_ROTATOR }; // Ra: modele binarne dają więcej możliwości niż mesh złożony z trójkątów
    int iName{ -1 }; // numer łańcucha z nazwą submodelu, albo -1 gdy anonimowy
public: // chwilowo
    TAnimType b_Anim{ at_None };

private:
    int iFlags{ 0x0200 }; // bit 9=1: submodel został utworzony a nie ustawiony na wczytany plik
                // flagi informacyjne:
				// bit  0: =1 faza rysowania zależy od wymiennej tekstury 0
				// bit  1: =1 faza rysowania zależy od wymiennej tekstury 1
				// bit  2: =1 faza rysowania zależy od wymiennej tekstury 2
				// bit  3: =1 faza rysowania zależy od wymiennej tekstury 3
				// bit  4: =1 rysowany w fazie nieprzezroczystych (stała tekstura albo brak)
				// bit  5: =1 rysowany w fazie przezroczystych (stała tekstura)
				// bit  7: =1 ta sama tekstura, co poprzedni albo nadrzędny
				// bit  8: =1 wierzchołki wyświetlane z indeksów
				// bit  9: =1 wczytano z pliku tekstowego (jest właścicielem tablic)
				// bit 13: =1 wystarczy przesunięcie zamiast mnożenia macierzy (trzy jedynki)
				// bit 14: =1 wymagane przechowanie macierzy (animacje)
				// bit 15: =1 wymagane przechowanie macierzy (transform niejedynkowy)
	union
	{ // transform, nie każdy submodel musi mieć
		float4x4 *fMatrix = nullptr; // pojedyncza precyzja wystarcza
		int iMatrix; // w pliku binarnym jest numer matrycy
	};
    int iNumVerts { -1 }; // ilość wierzchołków (1 dla FreeSpotLight)
	int tVboPtr; // początek na liście wierzchołków albo indeksów
    int iTexture { 0 }; // numer nazwy tekstury, -1 wymienna, 0 brak
    float fVisible { 0.0f }; // próg jasności światła do załączenia submodelu
    float fLight { -1.0f }; // próg jasności światła do zadziałania selfillum
	glm::vec4
        f4Ambient { 1.0f,1.0f,1.0f,1.0f },
        f4Diffuse { 1.0f,1.0f,1.0f,1.0f },
        f4Specular { 0.0f,0.0f,0.0f,1.0f },
        f4Emision { 1.0f,1.0f,1.0f,1.0f };
    bool m_normalizenormals { false }; // indicates vectors need to be normalized due to scaling etc
    float fWireSize { 0.0f }; // nie używane, ale wczytywane
    float fSquareMaxDist { 10000.0f * 10000.0f };
    float fSquareMinDist { 0.0f };
	// McZapkie-050702: parametry dla swiatla:
    float fNearAttenStart { 40.0f };
    float fNearAttenEnd { 80.0f };
    bool bUseNearAtten { false }; // te 3 zmienne okreslaja rysowanie aureoli wokol zrodla swiatla
    int iFarAttenDecay { 0 }; // ta zmienna okresla typ zaniku natezenia swiatla (0:brak, 1,2: potega 1/R)
    float fFarDecayRadius { 100.0f }; // normalizacja j.w.
    float fCosFalloffAngle { 0.5f }; // cosinus kąta stożka pod którym widać światło
    float fCosHotspotAngle { 0.3f }; // cosinus kąta stożka pod którym widać aureolę i zwiększone natężenie światła
    float fCosViewAngle { 0.0f }; // cos kata pod jakim sie teraz patrzy

    TSubModel *Next { nullptr };
    TSubModel *Child { nullptr };
    geometry_handle m_geometry { NULL, NULL }; // geometry of the submodel
    texture_handle TextureID { NULL }; // numer tekstury, -1 wymienna, 0 brak
    bool bWire { false }; // nie używane, ale wczytywane
    float Opacity { 1.0f };
    float f_Angle { 0.0f };
    float3 v_RotateAxis { 0.0f, 0.0f, 0.0f };
	float3 v_Angles { 0.0f, 0.0f, 0.0f };

public: // chwilowo
    float3 v_TransVector { 0.0f, 0.0f, 0.0f };
/*
	basic_vertex *Vertices; // roboczy wskaźnik - wczytanie T3D do VBO
*/
    vertex_array Vertices;
    size_t iAnimOwner{ NULL }; // roboczy numer egzemplarza, który ustawił animację
    TAnimType b_aAnim{ at_None }; // kody animacji oddzielnie, bo zerowane
public:
    float4x4 *mAnimMatrix{ nullptr }; // macierz do animacji kwaternionowych (należy do AnimContainer)
public:
    TSubModel **smLetter{ nullptr }; // wskaźnik na tablicę submdeli do generoania tekstu (docelowo zapisać do E3D)
    TSubModel *Parent{ nullptr }; // nadrzędny, np. do wymnażania macierzy
    int iVisible{ 1 }; // roboczy stan widoczności
	std::string pTexture; // robocza nazwa tekstury do zapisania w pliku binarnym
	std::string pName; // robocza nazwa
private:
	int SeekFaceNormal( std::vector<unsigned int> const &Masks, int const Startface, unsigned int const Mask, glm::vec3 const &Position, vertex_array const &Vertices );
	void RaAnimation(TAnimType a);

public:
	static size_t iInstance; // identyfikator egzemplarza, który aktualnie renderuje model
	static texture_handle const *ReplacableSkinId;
	static int iAlpha; // maska bitowa dla danego przebiegu
	static double fSquareDist;
	static TModel3d *pRoot;
	static std::string *pasText; // tekst dla wyświetlacza (!!!! do przemyślenia)
	~TSubModel();
	int Load(cParser &Parser, TModel3d *Model, /*int Pos,*/ bool dynamic);
	void ChildAdd(TSubModel *SubModel);
	void NextAdd(TSubModel *SubModel);
	TSubModel * NextGet() { return Next; };
	TSubModel * ChildGet() { return Child; };
	int TriangleAdd(TModel3d *m, texture_handle tex, int tri);
/*
	basic_vertex * TrianglePtr(int tex, int pos, glm::vec3 const &Ambient, glm::vec3 const &Diffuse, glm::vec3 const &Specular );
*/
	void SetRotate(float3 vNewRotateAxis, float fNewAngle);
	void SetRotateXYZ(vector3 vNewAngles);
	void SetRotateXYZ(float3 vNewAngles);
	void SetTranslate(vector3 vNewTransVector);
	void SetTranslate(float3 vNewTransVector);
	void SetRotateIK1(float3 vNewAngles);
	TSubModel * GetFromName(std::string const &search, bool i = true);
	TSubModel * GetFromName(char const *search, bool i = true);
	inline float4x4 * GetMatrix() { return fMatrix; };
	inline void Hide() { iVisible = 0; };

    void create_geometry( std::size_t &Dataoffset, geometrybank_handle const &Bank );
	int FlagsCheck();
	void WillBeAnimated()
	{
		if (this)
			iFlags |= 0x4000;
	};
	void InitialRotate(bool doit);
	void BinInit(TSubModel *s, float4x4 *m, std::vector<std::string> *t, std::vector<std::string> *n, bool dynamic);
	void ReplacableSet(texture_handle const *r, int a)
	{
		ReplacableSkinId = r;
		iAlpha = a;
	};
	void TextureNameSet( std::string const &Name );
	void NameSet( std::string const &Name );
	// Ra: funkcje do budowania terenu z E3D
	int Flags() { return iFlags; };
	void UnFlagNext() { iFlags &= 0x00FFFFFF; };
	void ColorsSet( glm::vec3 const &Ambient, glm::vec3 const &Diffuse, glm::vec3 const &Specular );
    // sets light level (alpha component of illumination color) to specified value
    void SetLightLevel( float const Level, bool const Includechildren = false, bool const Includesiblings = false );
	inline float3 Translation1Get() {
		return fMatrix ? *(fMatrix->TranslationGet()) + v_TransVector : v_TransVector; }
	inline float3 Translation2Get() {
		return *(fMatrix->TranslationGet()) + Child->Translation1Get(); }
	int GetTextureId() {
		return TextureID; }
	void ParentMatrix(float4x4 *m);
	float MaxY( float4x4 const &m );
	void AdjustDist();

	void deserialize(std::istream&);
	void serialize(std::ostream&,
		std::vector<TSubModel*>&,
		std::vector<std::string>&,
		std::vector<std::string>&,
		std::vector<float4x4>&);
    void serialize_geometry( std::ostream &Output );

};

class TModel3d : public CMesh
{
    friend class opengl_renderer;

private:
	TSubModel *Root; // drzewo submodeli
	int iFlags; // Ra: czy submodele mają przezroczyste tekstury
public: // Ra: tymczasowo
	int iNumVerts; // ilość wierzchołków (gdy nie ma VBO, to m_nVertexCount=0)
private:
	std::vector<std::string> Textures; // nazwy tekstur
	std::vector<std::string> Names; // nazwy submodeli
	int *iModel; // zawartość pliku binarnego
	int iSubModelsCount; // Ra: używane do tworzenia binarnych
	std::string asBinary; // nazwa pod którą zapisać model binarny
    std::string m_filename;
public:
	inline TSubModel * GetSMRoot() { return (Root); };
	TModel3d();
	~TModel3d();
	TSubModel * GetFromName(const char *sName);
	TSubModel * AddToNamed(const char *Name, TSubModel *SubModel);
	void AddTo(TSubModel *tmp, TSubModel *SubModel);
	void LoadFromTextFile(std::string const &FileName, bool dynamic);
	void LoadFromBinFile(std::string const &FileName, bool dynamic);
	bool LoadFromFile(std::string const &FileName, bool dynamic);
	void SaveToBinFile(std::string const &FileName);
	void BreakHierarhy();
	int Flags() const { return iFlags; };
	void Init();
	std::string NameGet() { return m_filename; };
	int TerrainCount();
	TSubModel * TerrainSquare(int n);
	void deserialize(std::istream &s, size_t size, bool dynamic);
};

//---------------------------------------------------------------------------
