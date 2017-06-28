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
#include "Model3d.h"

#include "Globals.h"
#include "logs.h"
#include "mczapkie/mctools.h"
#include "Usefull.h"
#include "renderer.h"
#include "Timer.h"
#include "mtable.h"
#include "sn_utils.h"
//---------------------------------------------------------------------------

using namespace Mtable;

double TSubModel::fSquareDist = 0;
size_t TSubModel::iInstance; // numer renderowanego egzemplarza obiektu
texture_handle const *TSubModel::ReplacableSkinId = NULL;
int TSubModel::iAlpha = 0x30300030; // maska do testowania flag tekstur wymiennych
TModel3d *TSubModel::pRoot; // Ra: tymczasowo wskaźnik na model widoczny z submodelu
std::string *TSubModel::pasText;
// przykłady dla TSubModel::iAlpha:
// 0x30300030 - wszystkie bez kanału alfa
// 0x31310031 - tekstura -1 używana w danym cyklu, pozostałe nie
// 0x32320032 - tekstura -2 używana w danym cyklu, pozostałe nie
// 0x34340034 - tekstura -3 używana w danym cyklu, pozostałe nie
// 0x38380038 - tekstura -4 używana w danym cyklu, pozostałe nie
// 0x3F3F003F - wszystkie wymienne tekstury używane w danym cyklu
// Ale w TModel3d okerśla przezroczystość tekstur wymiennych!

TSubModel::~TSubModel()
{
/*
	if (uiDisplayList)
		glDeleteLists(uiDisplayList, 1);
*/
	if (iFlags & 0x0200)
	{ // wczytany z pliku tekstowego musi sam posprzątać
	  // SafeDeleteArray(Indices);
		SafeDelete(Next);
		SafeDelete(Child);
		delete fMatrix; // własny transform trzeba usunąć (zawsze jeden)
/*
		delete[] Vertices;
*/
	}
	delete[] smLetter; // używany tylko roboczo dla TP_TEXT, do przyspieszenia
					   // wyświetlania
};

void TSubModel::TextureNameSet(std::string const &Name)
{ // ustawienie nazwy submodelu, o
  // ile nie jest wczytany z E3D
	if (iFlags & 0x0200)
	{ // tylko jeżeli submodel zosta utworzony przez new
		pTexture = Name;
	}
};

void TSubModel::NameSet(std::string const &Name)
{ // ustawienie nazwy submodelu, o ile
  // nie jest wczytany z E3D
	if (iFlags & 0x0200)
		pName = Name;
};

// sets light level (alpha component of illumination color) to specified value
void
TSubModel::SetLightLevel( float const Level, bool const Includechildren, bool const Includesiblings ) {

    f4Emision.a = Level;
    if( true == Includesiblings ) {
        auto sibling { this };
        while( ( sibling = sibling->Next ) != nullptr ) {
            sibling->f4Emision.a = Level;
        }
    }
    if( ( true == Includechildren )
     && ( Child != nullptr ) ) {
        Child->SetLightLevel( Level, true, true ); // node's children include child's siblings and children
    }
}

int TSubModel::SeekFaceNormal(std::vector<unsigned int> const &Masks, int const Startface, unsigned int const Mask, glm::vec3 const &Position, vertex_array const &Vertices)
{ // szukanie punktu stycznego do (pt), zwraca numer wierzchołka, a nie trójkąta
	int facecount = iNumVerts / 3; // bo maska powierzchni jest jedna na trójkąt
    for( int faceidx = Startface; faceidx < facecount; ++faceidx ) {
        // pętla po trójkątach, od trójkąta (f)
        if( Masks[ faceidx ] & Mask ) {
            // jeśli wspólna maska powierzchni
            for( int vertexidx = 0; vertexidx < 2; ++vertexidx ) {
                if( Vertices[ 3 * faceidx + vertexidx ].position == Position ) {
                    return 3 * faceidx + vertexidx;
                }
            }
        }
    }
	return -1; // nie znaleziono stycznego wierzchołka
}

float emm1[] = { 1, 1, 1, 0 };
float emm2[] = { 0, 0, 0, 1 };

inline double readIntAsDouble(cParser &parser, int base = 255)
{
	int value = parser.getToken<int>(false);
	return (static_cast<double>(value) / base);
};

template <typename ColorT> inline void readColor(cParser &parser, ColorT *color)
{
    double discard;
    parser.getTokens(4, false);
    parser >> discard >> color[0] >> color[1] >> color[2];
    color[ 0 ] /= 255.0;
    color[ 1 ] /= 255.0;
    color[ 2 ] /= 255.0;
};

inline void readColor(cParser &parser, glm::vec4 &color)
{
	int discard;
	parser.getTokens(4, false);
	parser
        >> discard
        >> color.r
        >> color.g
        >> color.b;
    color /= 255.0f;
};

inline void readMatrix(cParser &parser, float4x4 &matrix)
{ // Ra: wczytanie transforma
	parser.getTokens(16, false);
	for (int x = 0; x <= 3; ++x) // wiersze
		for (int y = 0; y <= 3; ++y) // kolumny
			parser >> matrix(x)[y];
};

int TSubModel::Load( cParser &parser, TModel3d *Model, /*int Pos,*/ bool dynamic)
{ // Ra: VBO tworzone na poziomie modelu, a nie submodeli
    iNumVerts = 0;
/*
    iVboPtr = Pos; // pozycja w VBO
*/
    if (!parser.expectToken("type:"))
        Error("Model type parse failure!");
    {
        std::string type = parser.getToken<std::string>();
        if (type == "mesh")
            eType = GL_TRIANGLES; // submodel - trójkaty
        else if (type == "point")
            eType = GL_POINTS; // co to niby jest?
        else if (type == "freespotlight")
            eType = TP_FREESPOTLIGHT; //światełko
        else if (type == "text")
            eType = TP_TEXT; // wyświetlacz tekstowy (generator napisów)
        else if (type == "stars")
            eType = TP_STARS; // wiele punktów świetlnych
    };
    parser.ignoreToken();
    std::string token;
    parser.getTokens(1, false); // nazwa submodelu bez zmieny na małe
    parser >> token;
    NameSet(token);
    if (dynamic) {
        // dla pojazdu, blokujemy załączone submodele, które mogą być nieobsługiwane
        if( ( token.size() >= 3 )
         && ( token.find( "_on" ) + 3 == token.length() ) ) {
            // jeśli nazwa kończy się na "_on" to domyślnie wyłączyć, żeby się nie nakładało z obiektem "_off"
            iVisible = 0;
        }
    }
    else {
        // dla pozostałych modeli blokujemy zapalone światła, które mogą być nieobsługiwane
        if( token.compare( 0, 8, "Light_On" ) == 0 ) {
            // jeśli nazwa zaczyna się od "Light_On" to domyślnie wyłączyć, żeby się nie nakładało z obiektem "Light_Off"
            iVisible = 0;
        }
    }

    if (parser.expectToken("anim:")) // Ra: ta informacja by się przydała!
    { // rodzaj animacji
        std::string type = parser.getToken<std::string>();
        if (type != "false")
        {
            iFlags |= 0x4000; // jak animacja, to trzeba przechowywać macierz zawsze
            if (type == "seconds_jump")
                b_Anim = b_aAnim = at_SecondsJump; // sekundy z przeskokiem
            else if (type == "minutes_jump")
                b_Anim = b_aAnim = at_MinutesJump; // minuty z przeskokiem
            else if (type == "hours_jump")
                b_Anim = b_aAnim = at_HoursJump; // godziny z przeskokiem
            else if (type == "hours24_jump")
                b_Anim = b_aAnim = at_Hours24Jump; // godziny z przeskokiem
            else if (type == "seconds")
                b_Anim = b_aAnim = at_Seconds; // minuty płynnie
            else if (type == "minutes")
                b_Anim = b_aAnim = at_Minutes; // minuty płynnie
            else if (type == "hours")
                b_Anim = b_aAnim = at_Hours; // godziny płynnie
            else if (type == "hours24")
                b_Anim = b_aAnim = at_Hours24; // godziny płynnie
            else if (type == "billboard")
                b_Anim = b_aAnim = at_Billboard; // obrót w pionie do kamery
            else if (type == "wind")
                b_Anim = b_aAnim = at_Wind; // ruch pod wpływem wiatru
            else if (type == "sky")
                b_Anim = b_aAnim = at_Sky; // aniamacja nieba
            else if (type == "ik")
                b_Anim = b_aAnim = at_IK; // IK: zadający
            else if (type == "ik11")
                b_Anim = b_aAnim = at_IK11; // IK: kierunkowany
            else if (type == "ik21")
                b_Anim = b_aAnim = at_IK21; // IK: kierunkowany
            else if (type == "ik22")
                b_Anim = b_aAnim = at_IK22; // IK: kierunkowany
            else if (type == "digital")
                b_Anim = b_aAnim = at_Digital; // licznik mechaniczny
            else if (type == "digiclk")
                b_Anim = b_aAnim = at_DigiClk; // zegar cyfrowy
            else
                b_Anim = b_aAnim = at_Undefined; // nieznana forma animacji
        }
    }
    if (eType < TP_ROTATOR)
        readColor(parser, f4Ambient); // ignoruje token przed
    readColor(parser, f4Diffuse);
    if (eType < TP_ROTATOR)
        readColor(parser, f4Specular);
    parser.ignoreTokens(1); // zignorowanie nazwy "SelfIllum:"
    {
        std::string light = parser.getToken<std::string>();
        if (light == "true")
            fLight = 2.0; // zawsze świeci
        else if (light == "false")
            fLight = -1.0; // zawsze ciemy
        else
            fLight = std::stod(light);
    };
    if (eType == TP_FREESPOTLIGHT)
    {
        if (!parser.expectToken("nearattenstart:"))
        {
            Error("Model light parse failure!");
        }
        std::string discard;
        parser.getTokens(13, false);
        parser
            >> fNearAttenStart
            >> discard >> fNearAttenEnd
            >> discard >> bUseNearAtten
            >> discard >> iFarAttenDecay
            >> discard >> fFarDecayRadius
            >> discard >> fCosFalloffAngle // kąt liczony dla średnicy, a nie promienia
            >> discard >> fCosHotspotAngle; // kąt liczony dla średnicy, a nie promienia
        // convert conve parameters if specified in degrees
        if( fCosFalloffAngle > 1.0 ) {
            fCosFalloffAngle = std::cos( DegToRad( 0.5f * fCosFalloffAngle ) );
        }
        if( fCosHotspotAngle > 1.0 ) {
            fCosHotspotAngle = std::cos( DegToRad( 0.5f * fCosHotspotAngle ) );
        }
        iNumVerts = 1;
/*
        iFlags |= 0x4010; // rysowane w cyklu nieprzezroczystych, macierz musi zostać bez zmiany
*/
        iFlags |= 0x4030; // drawn both in solid (light point) and transparent (light glare) phases
    }
    else if (eType < TP_ROTATOR)
    {
        std::string discard;
        parser.getTokens(5, false);
        parser
            >> discard >> bWire
            >> discard >> fWireSize
            >> discard;
        // wymagane jest 0 dla szyb, 100 idzie w nieprzezroczyste
        Opacity = readIntAsDouble(parser, 100.0f); 
        if (Opacity > 1.0f)
            Opacity *= 0.01f; // w 2013 był błąd i aby go obejść, trzeba było wpisać 10000.0
/*
        if ((Global::iConvertModels & 1) == 0) // dla zgodności wstecz
            Opacity = 0.0; // wszystko idzie w przezroczyste albo zależnie od tekstury
*/
        if (!parser.expectToken("map:"))
            Error("Model map parse failure!");
        std::string texture = parser.getToken<std::string>();
        if (texture == "none")
        { // rysowanie podanym kolorem
            TextureID = 0;
            iFlags |= 0x10; // rysowane w cyklu nieprzezroczystych
        }
        else if (texture.find("replacableskin") != texture.npos)
        { // McZapkie-060702: zmienialne skory modelu
            TextureID = -1;
            iFlags |= (Opacity < 1.0) ? 1 : 0x10; // zmienna tekstura 1
        }
        else if (texture == "-1")
        {
            TextureID = -1;
            iFlags |= (Opacity < 1.0) ? 1 : 0x10; // zmienna tekstura 1
        }
        else if (texture == "-2")
        {
            TextureID = -2;
            iFlags |= (Opacity < 1.0) ? 2 : 0x10; // zmienna tekstura 2
        }
        else if (texture == "-3")
        {
            TextureID = -3;
            iFlags |= (Opacity < 1.0) ? 4 : 0x10; // zmienna tekstura 3
        }
        else if (texture == "-4")
        {
            TextureID = -4;
            iFlags |= (Opacity < 1.0) ? 8 : 0x10; // zmienna tekstura 4
        }
        else
        { // jeśli tylko nazwa pliku, to dawać bieżącą ścieżkę do tekstur
            TextureNameSet(texture);
            if( texture.find_first_of( "/\\" ) == texture.npos ) {
                texture.insert( 0, Global::asCurrentTexturePath );
            }
            TextureID = GfxRenderer.GetTextureId( texture, szTexturePath );
            // renderowanie w cyklu przezroczystych tylko jeśli:
            // 1. Opacity=0 (przejściowo <1, czy tam <100) oraz
            // 2. tekstura ma przezroczystość
            iFlags |=
                ( ( ( Opacity < 1.0 )
                 && ( GfxRenderer.Texture(TextureID).has_alpha ) ) ?
                    0x20 :
                    0x10 ); // 0x10-nieprzezroczysta, 0x20-przezroczysta
        };
    }
    else
        iFlags |= 0x10;

    // visibility range
	std::string discard;
	parser.getTokens(5, false);
	parser >> discard >> fSquareMaxDist >> discard >> fSquareMinDist >> discard;

    if( fSquareMaxDist <= 0.0 ) {
        // 15km to więcej, niż się obecnie wyświetla
        fSquareMaxDist = 15000.0;
    }
	fSquareMaxDist *= fSquareMaxDist;
	fSquareMinDist *= fSquareMinDist;

    // transformation matrix
	fMatrix = new float4x4();
	readMatrix(parser, *fMatrix); // wczytanie transform
	if (!fMatrix->IdentityIs())
		iFlags |= 0x8000; // transform niejedynkowy - trzeba go przechować
	if (eType < TP_ROTATOR)
	{ // wczytywanie wierzchołków
		parser.getTokens(2, false);
		parser >> discard >> token;
		// Ra 15-01: to wczytać jako tekst - jeśli pierwszy znak zawiera "*", to
		// dalej będzie nazwa wcześniejszego submodelu, z którego należy wziąć
		// wierzchołki
		// zapewni to jakąś zgodność wstecz, bo zamiast liczby będzie ciąg, którego
		// wartość powinna być uznana jako zerowa
		// parser.getToken(iNumVerts);
		if (token[0] == '*')
		{ // jeśli pierwszy znak jest gwiazdką, poszukać
		  // submodelu o nazwie bez tej gwiazdki i wziąć z
		  // niego wierzchołki
			Error("Vertices reference not yet supported!");
		}
		else
		{ // normalna lista wierzchołków
			iNumVerts = std::atoi(token.c_str());
			if (iNumVerts % 3)
			{
				iNumVerts = 0;
				Error("Mesh error, (iNumVertices=" + std::to_string(iNumVerts) + ")%3<>0");
				return 0;
			}
			// Vertices=new GLVERTEX[iNumVerts];
			if (iNumVerts) {
/*
				Vertices = new basic_vertex[iNumVerts];
*/
                Vertices.resize( iNumVerts );
                int facecount = iNumVerts / 3;
/*
                unsigned int *sg; // maski przynależności trójkątów do powierzchni
                sg = new unsigned int[iNumFaces]; // maski powierzchni: 0 oznacza brak użredniania wektorów normalnych
				int *wsp = new int[iNumVerts]; // z którego wierzchołka kopiować wektor normalny
*/
                std::vector<unsigned int> sg; sg.resize( facecount ); // maski przynależności trójkątów do powierzchni
                std::vector<int> wsp; wsp.resize( iNumVerts );// z którego wierzchołka kopiować wektor normalny
				int maska = 0;
				for (int i = 0; i < iNumVerts; ++i) {
                    // Ra: z konwersją na układ scenerii - będzie wydajniejsze wyświetlanie
					wsp[i] = -1; // wektory normalne nie są policzone dla tego wierzchołka
					if ((i % 3) == 0) {
                        // jeśli będzie maska -1, to dalej będą wierzchołki z wektorami normalnymi, podanymi jawnie
						maska = parser.getToken<int>(false); // maska powierzchni trójkąta
                        // dla maski -1 będzie 0, czyli nie ma wspólnych wektorów normalnych
						sg[i / 3] = (
                            ( maska == -1 ) ?
                                0 :
                                maska );
					}
					parser.getTokens(3, false);
					parser
                        >> Vertices[i].position.x
                        >> Vertices[i].position.y
                        >> Vertices[i].position.z;
					if (maska == -1)
					{ // jeśli wektory normalne podane jawnie
						parser.getTokens(3, false);
						parser
                            >> Vertices[i].normal.x
                            >> Vertices[i].normal.y
                            >> Vertices[i].normal.z;
						wsp[i] = i; // wektory normalne "są już policzone"
					}
					parser.getTokens(2, false);
					parser
                        >> Vertices[i].texture.s
                        >> Vertices[i].texture.t;
					if (i % 3 == 2) { 
                        // jeżeli wczytano 3 punkty
						if (Vertices[i    ].position == Vertices[i - 1].position
                         || Vertices[i - 1].position == Vertices[i - 2].position
                         || Vertices[i - 2].position == Vertices[i    ].position)
						{ // jeżeli punkty się nakładają na siebie
							--facecount; // o jeden trójkąt mniej
							iNumVerts -= 3; // czyli o 3 wierzchołki
							i -= 3; // wczytanie kolejnego w to miejsce
							WriteLog("Degenerated triangle ignored in: \"" + pName + "\", vertice " + std::to_string(i));
						}
						if (i > 0) {
                            // jeśli pierwszy trójkąt będzie zdegenerowany, to zostanie usunięty i nie ma co sprawdzać
							if ((glm::length(Vertices[i    ].position - Vertices[i - 1].position) > 1000.0)
                             || (glm::length(Vertices[i - 1].position - Vertices[i - 2].position) > 1000.0)
                             || (glm::length(Vertices[i - 2].position - Vertices[i    ].position) > 1000.0)) {
                                // jeżeli są dalej niż 2km od siebie //Ra 15-01:
                                // obiekt wstawiany nie powinien być większy niż 300m (trójkąty terenu w E3D mogą mieć 1.5km)
								--facecount; // o jeden trójkąt mniej
								iNumVerts -= 3; // czyli o 3 wierzchołki
								i -= 3; // wczytanie kolejnego w to miejsce
								WriteLog( "Too large triangle ignored in: \"" + pName + "\"" );
							}
                        }
					}
				}
/*
				glm::vec3 *n = new glm::vec3[iNumFaces]; // tablica wektorów normalnych dla trójkątów
*/
                std::vector<glm::vec3> facenormals;
                for( int i = 0; i < facecount; ++i ) {
                    // pętla po trójkątach - będzie szybciej, jak wstępnie przeliczymy normalne trójkątów
                    auto facenormal = 
                        glm::cross(
                            Vertices[ i * 3 ].position - Vertices[ i * 3 + 1 ].position,
                            Vertices[ i * 3 ].position - Vertices[ i * 3 + 2 ].position );
                    facenormals.emplace_back(
                        glm::length( facenormal ) > 0.0f ?
                            glm::normalize( facenormal ) :
                            glm::vec3() );
                }
				glm::vec3 vertexnormal; // roboczy wektor normalny
				for (int vertexidx = 0; vertexidx < iNumVerts; ++vertexidx) { 
                    // pętla po wierzchołkach trójkątów
                    if( wsp[ vertexidx ] >= 0 ) {
                        // jeśli już był liczony wektor normalny z użyciem tego wierzchołka to wystarczy skopiować policzony wcześniej
                        Vertices[ vertexidx ].normal = Vertices[ wsp[ vertexidx ] ].normal;
                    }
					else {
                        // inaczej musimy dopiero policzyć
						auto const faceidx = vertexidx / 3; // numer trójkąta
						vertexnormal = glm::vec3(); // liczenie zaczynamy od zera
						auto adjacenvertextidx = vertexidx; // zaczynamy dodawanie wektorów normalnych od własnego
						while (adjacenvertextidx >= 0) {
                            // sumowanie z wektorem normalnym sąsiada (włącznie ze sobą)
							wsp[adjacenvertextidx] = vertexidx; // informacja, że w tym wierzchołku jest już policzony wektor normalny
							vertexnormal += facenormals[adjacenvertextidx / 3];
                            // i szukanie od kolejnego trójkąta
							adjacenvertextidx = SeekFaceNormal(sg, adjacenvertextidx / 3 + 1, sg[faceidx], Vertices[vertexidx].position, Vertices);
						}
						// Ra 15-01: należało by jeszcze uwzględnić skalowanie wprowadzane przez transformy, aby normalne po przeskalowaniu były jednostkowe
                        Vertices[ vertexidx ].normal = (
                            glm::length( vertexnormal ) > 0.0f ?
                                glm::normalize( vertexnormal ) :
                                glm::vec3() ); // przepisanie do wierzchołka trójkąta
					}
				}
                Vertices.resize( iNumVerts ); // in case we had some degenerate triangles along the way
/*
				delete[] wsp;
				delete[] n;
				delete[] sg;
*/
			}
			else // gdy brak wierzchołków
			{
				eType = TP_ROTATOR; // submodel pomocniczy, ma tylko macierz przekształcenia
				/*iVboPtr =*/ iNumVerts = 0; // dla formalności
			}
		} // obsługa submodelu z własną listą wierzchołków
	}
	else if (eType == TP_STARS)
	{ // punkty świecące dookólnie - składnia jak
	  // dla smt_Mesh
		std::string discard;
		parser.getTokens(2, false);
		parser >> discard >> iNumVerts;
/*
		// Vertices=new GLVERTEX[iNumVerts];
		Vertices = new basic_vertex[iNumVerts];
*/
        Vertices.resize( iNumVerts );
        int i;
        unsigned int color;
		for (i = 0; i < iNumVerts; ++i)
		{
			if (i % 3 == 0)
			{
				parser.ignoreToken(); // maska powierzchni trójkąta
			}
			parser.getTokens(5, false);
			parser
                >> Vertices[i].position.x
                >> Vertices[i].position.y
                >> Vertices[i].position.z
                >> color // zakodowany kolor
				>> discard;
			Vertices[i].normal.x = ((color) & 0xff) / 255.0f; // R
			Vertices[i].normal.y = ((color >> 8) & 0xff) / 255.0f; // G
			Vertices[i].normal.z = ((color >> 16) & 0xff) / 255.0f; // B
		}
	}
    else if( eType == TP_FREESPOTLIGHT ) {
        // single light points only have single data point, duh
        Vertices.emplace_back();
        iNumVerts = 1;
    }
	// Visible=true; //się potem wyłączy w razie potrzeby
	// iFlags|=0x0200; //wczytano z pliku tekstowego (jest właścicielem tablic)
	if (iNumVerts < 1)
		iFlags &= ~0x3F; // cykl renderowania uzależniony od potomnych
	return iNumVerts; // do określenia wielkości VBO
};

int TSubModel::TriangleAdd(TModel3d *m, texture_handle tex, int tri)
{ // dodanie trójkątów do submodelu, używane przy tworzeniu E3D terenu
    TSubModel *s = this;
    while (s ? (s->TextureID != tex) : false)
    { // szukanie submodelu o danej teksturze
        if (s == this)
            s = Child;
        else
            s = s->Next;
    }
    if (!s)
    {
        if (TextureID <= 0)
            s = this; // użycie głównego
        else
        { // dodanie nowego submodelu do listy potomnych
            s = new TSubModel();
            m->AddTo(this, s);
        }
        s->TextureNameSet(GfxRenderer.Texture(tex).name);
        s->TextureID = tex;
        s->eType = GL_TRIANGLES;
    }
    if (s->iNumVerts < 0)
        s->iNumVerts = tri; // bo na początku jest -1, czyli że nie wiadomo
    else
        s->iNumVerts += tri; // aktualizacja ilości wierzchołków
    return s->iNumVerts - tri; // zwraca pozycję tych trójkątów w submodelu
};
/*
basic_vertex *TSubModel::TrianglePtr(int tex, int pos, glm::vec3 const &Ambient, glm::vec3 const &Diffuse, glm::vec3 const &Specular )
{ // zwraca wskaźnik do wypełnienia tabeli wierzchołków, używane przy tworzeniu E3D terenu

	TSubModel *s = this;
	while (s ? s->TextureID != tex : false)
	{ // szukanie submodelu o danej teksturze
		if (s == this)
			s = Child;
		else
			s = s->Next;
	}
	if (!s)
		return NULL; // coś nie tak poszło
	if (!s->Vertices)
	{ // utworznie tabeli trójkątów
		s->Vertices = new basic_vertex[s->iNumVerts];
		s->iVboPtr = iInstance; // pozycja submodelu w tabeli wierzchołków
		iInstance += s->iNumVerts; // pozycja dla następnego
	}
	s->ColorsSet(Ambient, Diffuse, Specular); // ustawienie kolorów świateł
	return s->Vertices + pos; // wskaźnik na wolne miejsce w tabeli wierzchołków
};
*/
#ifdef EU07_USE_OLD_RENDERCODE
void TSubModel::DisplayLists()
{ // utworznie po jednej skompilowanej liście dla
  // każdego submodelu
	if (Global::bUseVBO)
		return; // Ra: przy VBO to się nie przyda
	if (eType < TP_ROTATOR)
	{
		if (iNumVerts > 0)
		{
			uiDisplayList = glGenLists(1);
			glNewList(uiDisplayList, GL_COMPILE);
#ifdef USE_VERTEX_ARRAYS
								   // ShaXbee-121209: przekazywanie wierzcholkow hurtem
			glVertexPointer(3, GL_DOUBLE, sizeof(GLVERTEX), &Vertices[0].Point.x);
			glNormalPointer(GL_DOUBLE, sizeof(GLVERTEX), &Vertices[0].Normal.x);
			glTexCoordPointer(2, GL_FLOAT, sizeof(GLVERTEX), &Vertices[0].tu);
			glDrawArrays(eType, 0, iNumVerts);
#else
			glBegin(eType);
			for (int i = 0; i < iNumVerts; i++)
			{
				/*
				glNormal3dv(&Vertices[i].Normal.x);
				glTexCoord2f(Vertices[i].tu,Vertices[i].tv);
				glVertex3dv(&Vertices[i].Point.x);
				*/
				glNormal3fv(glm::value_ptr(Vertices[i].normal));
				glTexCoord2fv(glm::value_ptr(Vertices[i].texture));
				glVertex3fv(glm::value_ptr(Vertices[i].position));
			};
			glEnd();
#endif
            glEndList();
        }
    }
    else if (eType == TP_FREESPOTLIGHT)
    {
        uiDisplayList = glGenLists(1);
        glNewList(uiDisplayList, GL_COMPILE);
        glBegin(GL_POINTS);
        glVertex3f( 0.0f, 0.0f, -0.05f ); // shift point towards the viewer, to avoid z-fighting with the light polygons
        glEnd();
        glEndList();
    }
    else if (eType == TP_STARS)
    { // punkty świecące dookólnie
        uiDisplayList = glGenLists(1);
        glNewList(uiDisplayList, GL_COMPILE);
        glBegin(GL_POINTS);
        for (int i = 0; i < iNumVerts; ++i)
        {
            glColor3fv(glm::value_ptr(Vertices[i].normal));
            glVertex3fv(glm::value_ptr(Vertices[i].position));
        };
        glEnd();
        glEndList();
    }
    // SafeDeleteArray(Vertices); //przy VBO muszą zostać do załadowania całego
    // modelu
    if (Child)
        Child->DisplayLists();
    if (Next)
        Next->DisplayLists();
};
#endif

void TSubModel::InitialRotate(bool doit)
{ // konwersja układu współrzędnych na zgodny ze scenerią
    if (iFlags & 0xC000) // jeśli jest animacja albo niejednostkowy transform
    { // niejednostkowy transform jest mnożony i wystarczy zabawy
        if (doit) {
            // obrót lewostronny
            if (!fMatrix) {
                // macierzy może nie być w dodanym "bananie"
                fMatrix = new float4x4(); // tworzy macierz o przypadkowej zawartości
                fMatrix->Identity(); // a zaczynamy obracanie od jednostkowej
            }
            iFlags |= 0x8000; // po obróceniu będzie raczej niejedynkowy matrix
            fMatrix->InitialRotate(); // zmiana znaku X oraz zamiana Y i Z
            if (fMatrix->IdentityIs())
                iFlags &= ~0x8000; // jednak jednostkowa po obróceniu
        }
        if (Child)
            Child->InitialRotate(false); // potomnych nie obracamy już, tylko
        // ewentualnie optymalizujemy
        else if (Global::iConvertModels & 2) // optymalizacja jest opcjonalna
            if ((iFlags & 0xC000) == 0x8000) // o ile nie ma animacji
            { // jak nie ma potomnych, można wymnożyć przez transform i wyjedynkować
                // go
                float4x4 *mat = GetMatrix(); // transform submodelu
                if( false == Vertices.empty() ) {
                    for( auto &vertex : Vertices ) {
                        vertex.position = (*mat) * vertex.position;
                    }
                    // zerujemy przesunięcie przed obracaniem normalnych
                    (*mat)(3)[0] = (*mat)(3)[1] = (*mat)(3)[2] = 0.0;
                    if( eType != TP_STARS ) {
                        // gwiazdki mają kolory zamiast normalnych, to ich wtedy nie ruszamy
                        for( auto &vertex : Vertices ) {
                            vertex.normal = (
                                glm::length( vertex.normal ) > 0.0f ?
                                    glm::normalize( ( *mat ) * vertex.normal ) :
                                    glm::vec3() );
                        }
                    }
                }
                mat->Identity(); // jedynkowanie transformu po przeliczeniu wierzchołków
                iFlags &= ~0x8000; // transform jedynkowy
            }
    }
    else // jak jest jednostkowy i nie ma animacji
        if (doit)
    { // jeśli jest jednostkowy transform, to przeliczamy
        // wierzchołki, a mnożenie podajemy dalej
        float swapcopy;
        for( auto &vertex : Vertices ) {
            vertex.position.x = -vertex.position.x; // zmiana znaku X
            swapcopy = vertex.position.y; // zamiana Y i Z
            vertex.position.y = vertex.position.z;
            vertex.position.z = swapcopy;
            // wektory normalne również trzeba przekształcić, bo się źle oświetlają
            if( eType != TP_STARS ) {
                // gwiazdki mają kolory zamiast normalnych, to // ich wtedy nie ruszamy
                vertex.normal.x = -vertex.normal.x; // zmiana znaku X
                swapcopy = vertex.normal.y; // zamiana Y i Z
                vertex.normal.y = vertex.normal.z;
                vertex.normal.z = swapcopy;
            }
        }
        if (Child)
            Child->InitialRotate(doit); // potomne ewentualnie obrócimy
    }
    if (Next)
        Next->InitialRotate(doit);
};

void TSubModel::ChildAdd(TSubModel *SubModel)
{ // dodanie submodelu potemnego (uzależnionego)
  // Ra: zmiana kolejności, żeby kolejne móc renderować po aktualnym (było
  // przed)
	if (SubModel)
		SubModel->NextAdd(Child); // Ra: zmiana kolejności renderowania
	Child = SubModel;
};

void TSubModel::NextAdd(TSubModel *SubModel)
{ // dodanie submodelu kolejnego (wspólny przodek)
	if (Next)
		Next->NextAdd(SubModel);
	else
		Next = SubModel;
};

int TSubModel::FlagsCheck()
{ // analiza koniecznych zmian pomiędzy submodelami
  // samo pomijanie glBindTexture() nie poprawi wydajności
  // ale można sprawdzić, czy można w ogóle pominąć kod do tekstur (sprawdzanie
  // replaceskin)
	int i = 0;
	if (Child)
	{ // Child jest renderowany po danym submodelu
		if (Child->TextureID) // o ile ma teksturę
			if (Child->TextureID != TextureID) // i jest ona inna niż rodzica
				Child->iFlags |= 0x80; // to trzeba sprawdzać, jak z teksturami jest
		i = Child->FlagsCheck();
		iFlags |= 0x00FF0000 & ((i << 16) | (i) | (i >> 8)); // potomny, rodzeństwo i dzieci
		if (eType == TP_TEXT)
		{ // wyłączenie renderowania Next dla znaków
		  // wyświetlacza tekstowego
			TSubModel *p = Child;
			while (p)
			{
				p->iFlags &= 0xC0FFFFFF;
				p = p->Next;
			}
		}
	}
	if (Next)
	{ // Next jest renderowany po danym submodelu (kolejność odwrócona
	  // po wczytaniu T3D)
		if (TextureID) // o ile dany ma teksturę
			if ((TextureID != Next->TextureID) ||
				(i & 0x00800000)) // a ma inną albo dzieci zmieniają
				iFlags |= 0x80; // to dany submodel musi sobie ją ustawiać
		i = Next->FlagsCheck();
		iFlags |= 0xFF000000 & ((i << 24) | (i << 8) | (i)); // następny, kolejne i ich dzieci
															 // tekstury nie ustawiamy tylko wtedy, gdy jest taka sama jak Next i jego
															 // dzieci nie zmieniają
	}
	return iFlags;
};

void TSubModel::SetRotate(float3 vNewRotateAxis, float fNewAngle)
{ // obrócenie submodelu wg podanej
  // osi (np. wskazówki w kabinie)
	v_RotateAxis = vNewRotateAxis;
	f_Angle = fNewAngle;
	if (fNewAngle != 0.0)
	{
		b_Anim = at_Rotate;
		b_aAnim = at_Rotate;
	}
	iAnimOwner = iInstance; // zapamiętanie czyja jest animacja
}

void TSubModel::SetRotateXYZ(float3 vNewAngles)
{ // obrócenie submodelu o
  // podane kąty wokół osi
  // lokalnego układu
	v_Angles = vNewAngles;
	b_Anim = at_RotateXYZ;
	b_aAnim = at_RotateXYZ;
	iAnimOwner = iInstance; // zapamiętanie czyja jest animacja
}

void TSubModel::SetRotateXYZ(vector3 vNewAngles)
{ // obrócenie submodelu o
  // podane kąty wokół osi
  // lokalnego układu
	v_Angles.x = vNewAngles.x;
	v_Angles.y = vNewAngles.y;
	v_Angles.z = vNewAngles.z;
	b_Anim = at_RotateXYZ;
	b_aAnim = at_RotateXYZ;
	iAnimOwner = iInstance; // zapamiętanie czyja jest animacja
}

void TSubModel::SetTranslate(float3 vNewTransVector)
{ // przesunięcie submodelu (np. w kabinie)
	v_TransVector = vNewTransVector;
	b_Anim = at_Translate;
	b_aAnim = at_Translate;
	iAnimOwner = iInstance; // zapamiętanie czyja jest animacja
}

void TSubModel::SetTranslate(vector3 vNewTransVector)
{ // przesunięcie submodelu (np. w kabinie)
	v_TransVector.x = vNewTransVector.x;
	v_TransVector.y = vNewTransVector.y;
	v_TransVector.z = vNewTransVector.z;
	b_Anim = at_Translate;
	b_aAnim = at_Translate;
	iAnimOwner = iInstance; // zapamiętanie czyja jest animacja
}

void TSubModel::SetRotateIK1(float3 vNewAngles)
{ // obrócenie submodelu o
  // podane kąty wokół osi
  // lokalnego układu
	v_Angles = vNewAngles;
	iAnimOwner = iInstance; // zapamiętanie czyja jest animacja
}

struct ToLower
{
	char operator()(char input)
	{
		return tolower(input);
	}
};

TSubModel *TSubModel::GetFromName(std::string const &search, bool i)
{
	return GetFromName(search.c_str(), i);
};

TSubModel *TSubModel::GetFromName(char const *search, bool i)
{
	TSubModel *result;
	// std::transform(search.begin(),search.end(),search.begin(),ToLower());
	// search=search.LowerCase();
	// AnsiString name=AnsiString();
	std::string search_lc = std::string(search);
	if (i)
		std::transform(search_lc.begin(), search_lc.end(), search_lc.begin(), ::tolower);
	std::string pName_lc = pName;
	if (i)
		std::transform(pName_lc.begin(), pName_lc.end(), pName_lc.begin(), ::tolower);
	if (pName.size() && search)
		if (pName_lc == search_lc)
			return this;
	if (Next)
	{
		result = Next->GetFromName(search);
		if (result)
			return result;
	}
	if (Child)
	{
		result = Child->GetFromName(search);
		if (result)
			return result;
	}
	return NULL;
};

// WORD hbIndices[18]={3,0,1,5,4,2,1,0,4,1,5,3,2,3,5,2,4,0};

void TSubModel::RaAnimation(TAnimType a)
{ // wykonanie animacji niezależnie od renderowania
	switch (a)
	{ // korekcja położenia, jeśli submodel jest animowany
	case at_Translate: // Ra: było "true"
		if (iAnimOwner != iInstance)
			break; // cudza animacja
		glTranslatef(v_TransVector.x, v_TransVector.y, v_TransVector.z);
		break;
	case at_Rotate: // Ra: było "true"
		if (iAnimOwner != iInstance)
			break; // cudza animacja
		glRotatef(f_Angle, v_RotateAxis.x, v_RotateAxis.y, v_RotateAxis.z);
		break;
	case at_RotateXYZ:
		if (iAnimOwner != iInstance)
			break; // cudza animacja
		glTranslatef(v_TransVector.x, v_TransVector.y, v_TransVector.z);
		glRotatef(v_Angles.x, 1.0f, 0.0f, 0.0f);
		glRotatef(v_Angles.y, 0.0f, 1.0f, 0.0f);
		glRotatef(v_Angles.z, 0.0f, 0.0f, 1.0f);
		break;
	case at_SecondsJump: // sekundy z przeskokiem
		glRotatef(simulation::Time.data().wSecond * 6.0, 0.0, 1.0, 0.0);
		break;
	case at_MinutesJump: // minuty z przeskokiem
		glRotatef(simulation::Time.data().wMinute * 6.0, 0.0, 1.0, 0.0);
		break;
	case at_HoursJump: // godziny skokowo 12h/360°
		glRotatef(simulation::Time.data().wHour * 30.0 * 0.5, 0.0, 1.0, 0.0);
		break;
	case at_Hours24Jump: // godziny skokowo 24h/360°
		glRotatef(simulation::Time.data().wHour * 15.0 * 0.25, 0.0, 1.0, 0.0);
		break;
	case at_Seconds: // sekundy płynnie
		glRotatef(simulation::Time.second() * 6.0, 0.0, 1.0, 0.0);
		break;
	case at_Minutes: // minuty płynnie
		glRotatef(simulation::Time.data().wMinute * 6.0 + simulation::Time.second() * 0.1, 0.0, 1.0, 0.0);
		break;
	case at_Hours: // godziny płynnie 12h/360°
		glRotatef(2.0 * Global::fTimeAngleDeg, 0.0, 1.0, 0.0);
		break;
	case at_Hours24: // godziny płynnie 24h/360°
		glRotatef(Global::fTimeAngleDeg, 0.0, 1.0, 0.0);
		break;
	case at_Billboard: // obrót w pionie do kamery
	{
        matrix4x4 mat; mat.OpenGL_Matrix( OpenGLMatrices.data_array( GL_MODELVIEW ) );
		float3 gdzie = float3(mat[3][0], mat[3][1], mat[3][2]); // początek układu współrzędnych submodelu względem kamery
		glLoadIdentity(); // macierz jedynkowa
		glTranslatef(gdzie.x, gdzie.y, gdzie.z); // początek układu zostaje bez
												 // zmian
		glRotated(atan2(gdzie.x, gdzie.z) * 180.0 / M_PI, 0.0, 1.0,
			0.0); // jedynie obracamy w pionie o kąt
	}
	break;
	case at_Wind: // ruch pod wpływem wiatru (wiatr będziemy liczyć potem...)
		glRotated(1.5 * std::sin(M_PI * simulation::Time.second() / 6.0), 0.0, 1.0, 0.0);
		break;
	case at_Sky: // animacja nieba
		glRotated(Global::fLatitudeDeg, 1.0, 0.0, 0.0); // ustawienie osi OY na północ
														// glRotatef(Global::fTimeAngleDeg,0.0,1.0,0.0); //obrót dobowy osi OX
		glRotated(-fmod(Global::fTimeAngleDeg, 360.0), 0.0, 1.0, 0.0); // obrót dobowy osi OX
		break;
	case at_IK11: // ostatni element animacji szkieletowej (podudzie, stopa)
		glRotatef(v_Angles.z, 0.0f, 1.0f, 0.0f); // obrót względem osi pionowej (azymut)
		glRotatef(v_Angles.x, 1.0f, 0.0f, 0.0f); // obrót względem poziomu (deklinacja)
		break;
	case at_DigiClk: // animacja zegara cyfrowego
	{ // ustawienie animacji w submodelach potomnych
		TSubModel *sm = ChildGet();
		do
		{ // pętla po submodelach potomnych i obracanie ich o kąt zależy od czasu
			if (sm->pName.size())
			{ // musi mieć niepustą nazwę
				if ((sm->pName[0]) >= '0')
					if ((sm->pName[0]) <= '5') // zegarek ma 6 cyfr maksymalnie
						sm->SetRotate(float3(0, 1, 0),
							-Global::fClockAngleDeg[(sm->pName[0]) - '0']);
			}
			sm = sm->NextGet();
		} while (sm);
	}
	break;
	}
	if (mAnimMatrix) // można by to dać np. do at_Translate
	{
		glMultMatrixf(mAnimMatrix->readArray());
		mAnimMatrix = NULL; // jak animator będzie potrzebował, to ustawi ponownie
	}
};

   //---------------------------------------------------------------------------

void TSubModel::serialize_geometry( std::ostream &Output ) {

    if( Child ) {
        Child->serialize_geometry( Output );
    }
    if( m_geometry != NULL ) {
        for( auto const &vertex : GfxRenderer.Vertices( m_geometry ) ) {
            vertex.serialize( Output );
        }
    }
    if( Next ) {
        Next->serialize_geometry( Output );
    }
};

void
TSubModel::create_geometry( std::size_t &Dataoffset, geometrybank_handle const &Bank ) {

    // data offset is used to determine data offset of each submodel into single shared geometry bank
    // (the offsets are part of legacy system which we now need to work around for backward compatibility)

    if( Child )
        Child->create_geometry( Dataoffset, Bank );

    if( false == Vertices.empty() ) {
        tVboPtr = static_cast<int>( Dataoffset );
        Dataoffset += Vertices.size();
        // conveniently all relevant custom node types use GL_POINTS, or we'd have to determine the type on individual basis
        auto type = (
            eType < TP_ROTATOR ?
                eType :
                GL_POINTS );
        m_geometry = GfxRenderer.Insert( Vertices, Bank, type );
    }

    if( Next )
        Next->create_geometry( Dataoffset, Bank );
}

// NOTE: leftover from static distance factor adjustment.
// TODO: get rid of it, once we have the dynamic adjustment code in place
void TSubModel::AdjustDist()
{ // aktualizacja odległości faz LoD, zależna od
  // rozdzielczości pionowej oraz multisamplingu
	if (fSquareMaxDist > 0.0)
		fSquareMaxDist *= Global::fDistanceFactor;
	if (fSquareMinDist > 0.0)
		fSquareMinDist /= Global::fDistanceFactor;
	// if (fNearAttenStart>0.0) fNearAttenStart*=Global::fDistanceFactor;
	// if (fNearAttenEnd>0.0) fNearAttenEnd*=Global::fDistanceFactor;
	if (Child)
		Child->AdjustDist();
	if (Next)
		Next->AdjustDist();
};

void TSubModel::ColorsSet( glm::vec3 const &Ambient, glm::vec3 const &Diffuse, glm::vec3 const &Specular )
{ // ustawienie kolorów dla modelu terenu
    f4Ambient = glm::vec4( Ambient, 1.0f );
    f4Diffuse = glm::vec4( Diffuse, 1.0f );
    f4Specular = glm::vec4( Specular, 1.0f );
/*
    int i;
	if (a)
		for (i = 0; i < 4; ++i)
			f4Ambient[i] = a[i] / 255.0;
	if (d)
		for (i = 0; i < 4; ++i)
			f4Diffuse[i] = d[i] / 255.0;
	if (s)
		for (i = 0; i < 4; ++i)
			f4Specular[i] = s[i] / 255.0;
*/
};

void TSubModel::ParentMatrix(float4x4 *m)
{ // pobranie transformacji względem wstawienia modelu
  // jeśli nie zostało wykonane Init() (tzn. zaraz po wczytaniu T3D), to
  // dodatkowy obrót
  // obrót T3D jest wymagany np. do policzenia wysokości pantografów
	*m = float4x4(*fMatrix); // skopiowanie, bo będziemy mnożyć
							 // m(3)[1]=m[3][1]+0.054; //w górę o wysokość ślizgu (na razie tak)
	TSubModel *sm = this;
	while (sm->Parent)
	{ // przenieść tę funkcję do modelu
		if (sm->Parent->GetMatrix())
			*m = *sm->Parent->GetMatrix() * *m;
		sm = sm->Parent;
	}
	// dla ostatniego może być potrzebny dodatkowy obrót, jeśli wczytano z T3D, a
	// nie obrócono jeszcze
};

// obliczenie maksymalnej wysokości, na początek ślizgu w pantografie
float TSubModel::MaxY( float4x4 const &m ) {
    // tylko dla trójkątów liczymy
    if( eType != 4 ) { return 0; }

    auto maxy { 0.0f };
    // binary and text models invoke this function at different stages, either after or before geometry data was sent to the geometry manager
    if( m_geometry != NULL ) {

        for( auto const &vertex : GfxRenderer.Vertices( m_geometry ) ) {
            maxy = std::max(
                maxy,
                  m[ 0 ][ 1 ] * vertex.position.x
                + m[ 1 ][ 1 ] * vertex.position.y
                + m[ 2 ][ 1 ] * vertex.position.z
                + m[ 3 ][ 1 ] );
        }
    }
    else if( false == Vertices.empty() ) {

        for( auto const &vertex : Vertices ) {
            maxy = std::max(
                maxy,
                  m[ 0 ][ 1 ] * vertex.position.x
                + m[ 1 ][ 1 ] * vertex.position.y
                + m[ 2 ][ 1 ] * vertex.position.z
                + m[ 3 ][ 1 ] );
        }
    }

    return maxy;
};
//---------------------------------------------------------------------------

TModel3d::TModel3d()
{
	Root = NULL;
	iFlags = 0;
	iSubModelsCount = 0;
	iModel = NULL; // tylko jak wczytany model binarny
	iNumVerts = 0; // nie ma jeszcze wierzchołków
};

TModel3d::~TModel3d()
{
	// SafeDeleteArray(Materials);
	if (iFlags & 0x0200)
	{ // wczytany z pliku tekstowego, submodele sprzątają same
		SafeDelete(Root); // submodele się usuną rekurencyjnie
	}
	else
	{ // wczytano z pliku binarnego (jest właścicielem tablic)
		Root = nullptr;
		delete[] iModel; // usuwamy cały wczytany plik i to wystarczy
	}
	// później się jeszcze usuwa obiekt z którego dziedziczymy tabelę VBO
};

TSubModel *TModel3d::AddToNamed(const char *Name, TSubModel *SubModel)
{
	TSubModel *sm = Name ? GetFromName(Name) : NULL;
	AddTo(sm, SubModel); // szukanie nadrzędnego
	return sm; // zwracamy wskaźnik do nadrzędnego submodelu
};

void TModel3d::AddTo(TSubModel *tmp, TSubModel *SubModel)
{ // jedyny poprawny sposób dodawania
  // submodeli, inaczej mogą zginąć
  // przy zapisie E3D
	if (tmp)
	{ // jeśli znaleziony, podłączamy mu jako potomny
		tmp->ChildAdd(SubModel);
	}
	else
	{ // jeśli nie znaleziony, podczepiamy do łańcucha głównego
		SubModel->NextAdd(Root); // Ra: zmiana kolejności renderowania wymusza zmianę tu
		Root = SubModel;
	}
	++iSubModelsCount; // teraz jest o 1 submodel więcej
	iFlags |= 0x0200; // submodele są oddzielne
};

TSubModel *TModel3d::GetFromName(const char *sName)
{ // wyszukanie submodelu po nazwie
	if (!sName)
		return Root; // potrzebne do terenu z E3D
	if (iFlags & 0x0200) // wczytany z pliku tekstowego, wyszukiwanie rekurencyjne
		return Root ? Root->GetFromName(sName) : NULL;
	else // wczytano z pliku binarnego, można wyszukać iteracyjnie
	{
		// for (int i=0;i<iSubModelsCount;++i)
		return Root ? Root->GetFromName(sName) : NULL;
	}
};

bool TModel3d::LoadFromFile(std::string const &FileName, bool dynamic)
{
    // wczytanie modelu z pliku
    std::string name = ToLower(FileName);
    // trim extension if needed
    if( name.rfind( '.' ) != std::string::npos )
    {
        name.erase(name.rfind('.'));
    }

	asBinary = name + ".e3d";
	if (FileExists(asBinary))
	{
		LoadFromBinFile(asBinary, dynamic);
		asBinary = ""; // wyłączenie zapisu
		Init();
        // cache the file name, in case someone wants it later
        m_filename = name + ".e3d";
    }
	else
	{
		if (FileExists(name + ".t3d"))
		{
			LoadFromTextFile(FileName, dynamic); // wczytanie tekstowego
            if( !dynamic ) {
                // pojazdy dopiero po ustawieniu animacji
                Init(); // generowanie siatek i zapis E3D
            }
            // cache the file name, in case someone wants it later
            m_filename = name + ".t3d";
        }
	}
	bool const result =
		Root ? (iSubModelsCount > 0) : false; // brak pliku albo problem z wczytaniem
	if (false == result)
	{
		ErrorLog("Failed to load 3d model \"" + FileName + "\"");
	}
	return result;
};

// E3D serialization
// http://rainsted.com/pl/Format_binarny_modeli_-_E3D


//m7todo: wymyślić lepszą nazwę
template <typename L, typename T>
size_t get_container_pos(L &list, T o)
{
	auto i = std::find(list.begin(), list.end(), o);
	if (i == list.end())
	{
		list.push_back(o);
		return list.size() - 1;
	}
	else
	{
		return std::distance(list.begin(), i);
	}
}

//m7todo: za dużo argumentów, może przenieść do osobnej
//klasy serializera mającej własny stan, albo zrobić
//strukturę TModel3d::SerializerContext?
void TSubModel::serialize(std::ostream &s,
	std::vector<TSubModel*> &models,
	std::vector<std::string> &names,
	std::vector<std::string> &textures,
	std::vector<float4x4> &transforms)
{
	size_t end = (size_t)s.tellp() + 256;

	if (!Next)
		sn_utils::ls_int32(s, -1);
	else
		sn_utils::ls_int32(s, (int32_t)get_container_pos(models, Next));
	if (!Child)
		sn_utils::ls_int32(s, -1);
	else
		sn_utils::ls_int32(s, (int32_t)get_container_pos(models, Child));

	sn_utils::ls_int32(s, eType);
	if (pName.size() == 0)
		sn_utils::ls_int32(s, -1);
	else
		sn_utils::ls_int32(s, (int32_t)get_container_pos(names, pName));
	sn_utils::ls_int32(s, (int)b_Anim);

	sn_utils::ls_int32(s, iFlags);
	sn_utils::ls_int32(s, (int32_t)get_container_pos(transforms, *fMatrix));

	sn_utils::ls_int32(s, iNumVerts);
	sn_utils::ls_int32(s, tVboPtr);

	if (TextureID <= 0)
		sn_utils::ls_int32(s, TextureID);
	else
		sn_utils::ls_int32(s, (int32_t)get_container_pos(textures, pTexture));

	sn_utils::ls_float32(s, fVisible);
	sn_utils::ls_float32(s, fLight);

	for (size_t i = 0; i < 4; i++)
		sn_utils::ls_float32(s, f4Ambient[i]);
	for (size_t i = 0; i < 4; i++)
		sn_utils::ls_float32(s, f4Diffuse[i]);
	for (size_t i = 0; i < 4; i++)
		sn_utils::ls_float32(s, f4Specular[i]);
	for (size_t i = 0; i < 4; i++)
		sn_utils::ls_float32(s, f4Emision[i]);

	sn_utils::ls_float32(s, fWireSize);
	sn_utils::ls_float32(s, fSquareMaxDist);
	sn_utils::ls_float32(s, fSquareMinDist);

	sn_utils::ls_float32(s, fNearAttenStart);
	sn_utils::ls_float32(s, fNearAttenEnd);
	sn_utils::ls_uint32(s, bUseNearAtten ? 1 : 0);

	sn_utils::ls_int32(s, iFarAttenDecay);
	sn_utils::ls_float32(s, fFarDecayRadius);
	sn_utils::ls_float32(s, fCosFalloffAngle);
	sn_utils::ls_float32(s, fCosHotspotAngle);
	sn_utils::ls_float32(s, fCosViewAngle);

	size_t fill = end - s.tellp();
	for (size_t i = 0; i < fill; i++)
		s.put(0);
}

void TModel3d::SaveToBinFile(std::string const &FileName)
{
	WriteLog("saving e3d model..");

	//m7todo: można by zoptymalizować robiąc unordered_map
	//na wyszukiwanie numerów już dodanych stringów i osobno
	//vector na wskaźniki do stringów w kolejności numeracji
	//tylko czy potrzeba?
	std::vector<TSubModel*> models;
	models.push_back(Root);
	std::vector<std::string> names;
	std::vector<std::string> textures;
	textures.push_back("");
	std::vector<float4x4> transforms;

	std::ofstream s(FileName, std::ios::binary);

	sn_utils::ls_uint32(s, MAKE_ID4('E', '3', 'D', '0'));
	size_t e3d_spos = s.tellp();
	sn_utils::ls_uint32(s, 0);

	{
		sn_utils::ls_uint32(s, MAKE_ID4('S', 'U', 'B', '0'));
		size_t sub_spos = s.tellp();
		sn_utils::ls_uint32(s, 0);
		for (size_t i = 0; i < models.size(); i++)
			models[i]->serialize(s, models, names, textures, transforms);
		size_t pos = s.tellp();
		s.seekp(sub_spos);
		sn_utils::ls_uint32(s, (uint32_t)(4 + pos - sub_spos));
		s.seekp(pos);
	}

	sn_utils::ls_uint32(s, MAKE_ID4('T', 'R', 'A', '0'));
	sn_utils::ls_uint32(s, 8 + (uint32_t)transforms.size() * 64);
	for (size_t i = 0; i < transforms.size(); i++)
		transforms[i].serialize_float32(s);

    sn_utils::ls_uint32(s, MAKE_ID4('V', 'N', 'T', '0'));
	sn_utils::ls_uint32(s, 8 + iNumVerts * 32);
    Root->serialize_geometry( s );

	if (textures.size())
	{
		sn_utils::ls_uint32(s, MAKE_ID4('T', 'E', 'X', '0'));
		size_t tex_spos = s.tellp();
		sn_utils::ls_uint32(s, 0);
		for (size_t i = 0; i < textures.size(); i++)
			sn_utils::s_str(s, textures[i]);
		size_t pos = s.tellp();
		s.seekp(tex_spos);
		sn_utils::ls_uint32(s, (uint32_t)(4 + pos - tex_spos));
		s.seekp(pos);
	}

	if (names.size())
	{
		sn_utils::ls_uint32(s, MAKE_ID4('N', 'A', 'M', '0'));
		size_t nam_spos = s.tellp();
		sn_utils::ls_uint32(s, 0);
		for (size_t i = 0; i < names.size(); i++)
			sn_utils::s_str(s, names[i]);
		size_t pos = s.tellp();
		s.seekp(nam_spos);
		sn_utils::ls_uint32(s, (uint32_t)(4 + pos - nam_spos));
		s.seekp(pos);
	}

	size_t end = s.tellp();
	s.seekp(e3d_spos);
	sn_utils::ls_uint32(s, (uint32_t)(4 + end - e3d_spos));
	s.close();

	WriteLog("..done.");
}

void TSubModel::deserialize(std::istream &s)
{
	iNext = sn_utils::ld_int32(s);
	iChild = sn_utils::ld_int32(s);

	eType = sn_utils::ld_int32(s);
	iName = sn_utils::ld_int32(s);

	b_Anim = (TAnimType)sn_utils::ld_int32(s);

	iFlags = sn_utils::ld_int32(s);
	iMatrix = sn_utils::ld_int32(s);

	iNumVerts = sn_utils::ld_int32(s);
	tVboPtr = sn_utils::ld_int32(s);
	iTexture = sn_utils::ld_int32(s);

	fVisible = sn_utils::ld_float32(s);
	fLight = sn_utils::ld_float32(s);

	for (size_t i = 0; i < 4; ++i)
		f4Ambient[i] = sn_utils::ld_float32(s);
	for (size_t i = 0; i < 4; ++i)
		f4Diffuse[i] = sn_utils::ld_float32(s);
	for (size_t i = 0; i < 4; ++i)
		f4Specular[i] = sn_utils::ld_float32(s);
	for (size_t i = 0; i < 4; ++i)
		f4Emision[i] = sn_utils::ld_float32(s);

	fWireSize = sn_utils::ld_float32(s);
	fSquareMaxDist = sn_utils::ld_float32(s);
	fSquareMinDist = sn_utils::ld_float32(s);

	fNearAttenStart = sn_utils::ld_float32(s);
	fNearAttenEnd = sn_utils::ld_float32(s);
	bUseNearAtten = sn_utils::ld_uint32(s) != 0;
	iFarAttenDecay = sn_utils::ld_int32(s);
	fFarDecayRadius = sn_utils::ld_float32(s);
	fCosFalloffAngle = sn_utils::ld_float32(s);
	fCosHotspotAngle = sn_utils::ld_float32(s);
	fCosViewAngle = sn_utils::ld_float32(s);
}

void TModel3d::deserialize(std::istream &s, size_t size, bool dynamic)
{
	Root = nullptr;
	float4x4 *tm = nullptr;
    if( m_geometrybank == NULL ) {
        m_geometrybank = GfxRenderer.Create_Bank();
    }

	std::streampos end = s.tellg() + (std::streampos)size;

	while (s.tellg() < end)
	{
		uint32_t type = sn_utils::ld_uint32(s);
		uint32_t size = sn_utils::ld_uint32(s) - 8;
		std::streampos end = s.tellg() + (std::streampos)size;

		if ((type & 0x00FFFFFF) == MAKE_ID4('S', 'U', 'B', 0))
		{
			if (Root != nullptr)
				throw std::runtime_error("e3d: duplicated SUB chunk");

			size_t sm_size = 256 + 64 * (((type & 0xFF000000) >> 24) - '0');
			size_t sm_cnt = size / sm_size;
			iSubModelsCount = (int)sm_cnt;
			Root = new TSubModel[sm_cnt];
			size_t pos = s.tellg();
			for (size_t i = 0; i < sm_cnt; i++)
			{
				s.seekg(pos + sm_size * i);
				Root[i].deserialize(s);
			}
		}
		else if (type == MAKE_ID4('V', 'N', 'T', '0'))
		{
/*
            if (m_pVNT != nullptr)
				throw std::runtime_error("e3d: duplicated VNT chunk");

            size_t vt_cnt = size / 32;
			iNumVerts = (int)vt_cnt;
			m_nVertexCount = (int)vt_cnt;
#ifdef EU07_USE_OLD_VERTEXBUFFER
            assert( m_pVNT == nullptr );
            m_pVNT = new basic_vertex[vt_cnt];
#else
            m_pVNT.resize( vt_cnt );
#endif
			for (size_t i = 0; i < vt_cnt; i++)
				m_pVNT[i].deserialize(s);
*/
            // we rely on the SUB chunk coming before the vertex data, and on the overall vertex count matching the size of data in the chunk
            // geometry associated with chunks isn't stored in the same order as the chunks themselves, so we need to sort that out first
            std::vector< std::pair<int, int> > submodeloffsets;
            submodeloffsets.reserve( iSubModelsCount );
            for( int submodelindex = 0; submodelindex < iSubModelsCount; ++submodelindex ) {
                auto const &submodel = Root[ submodelindex ];
                if( submodel.iNumVerts <= 0 ) { continue; }
                submodeloffsets.emplace_back( submodel.tVboPtr, submodelindex );
            }
            std::sort(
                submodeloffsets.begin(),
                submodeloffsets.end(),
                []( std::pair<int, int> const &Left, std::pair<int, int> const &Right ) {
                    return (Left.first) < (Right.first); } );
            // once sorted we can grab geometry as it comes, and assign it to the chunks it belongs to
            for( auto const &submodeloffset : submodeloffsets ) {
                auto &submodel = Root[ submodeloffset.second ];
                vertex_array vertices; vertices.resize( submodel.iNumVerts );
                iNumVerts += submodel.iNumVerts;
                for( auto &vertex : vertices ) {
                    vertex.deserialize( s );
                }
                // remap geometry type for custom type submodels
                int type;
                switch( submodel.eType ) {
                    case TP_FREESPOTLIGHT:
                    case TP_STARS: {
                        type = GL_POINTS;
                        break; }
                    default: {
                        type = submodel.eType;
                        break;
                    }
                }
                submodel.m_geometry = GfxRenderer.Insert( vertices, m_geometrybank, type );
            }

		}
		else if (type == MAKE_ID4('T', 'R', 'A', '0'))
		{
			if (tm != nullptr)
				throw std::runtime_error("e3d: duplicated TRA chunk");
			size_t t_cnt = size / 64;

			tm = new float4x4[t_cnt];
			for (size_t i = 0; i < t_cnt; i++)
				tm[i].deserialize_float32(s);
		}
		else if (type == MAKE_ID4('T', 'R', 'A', '1'))
		{
			if (tm != nullptr)
				throw std::runtime_error("e3d: duplicated TRA chunk");
			size_t t_cnt = size / 128;

			tm = new float4x4[t_cnt];
			for (size_t i = 0; i < t_cnt; i++)
				tm[i].deserialize_float64(s);
		}
		else if (type == MAKE_ID4('T', 'E', 'X', '0'))
		{
			if (Textures.size())
				throw std::runtime_error("e3d: duplicated TEX chunk");
			while (s.tellg() < end)
				Textures.push_back(sn_utils::d_str(s));
		}
		else if (type == MAKE_ID4('N', 'A', 'M', '0'))
		{
			if (Names.size())
				throw std::runtime_error("e3d: duplicated NAM chunk");
			while (s.tellg() < end)
				Names.push_back(sn_utils::d_str(s));
		}

		s.seekg(end);
	}

	if (!Root)
		throw std::runtime_error("e3d: no submodels");
/*
#ifdef EU07_USE_OLD_VERTEXBUFFER
    if (!m_pVNT)
#else
    if(m_pVNT.empty() )
#endif
		throw std::runtime_error("e3d: no vertices");
*/
	for (size_t i = 0; (int)i < iSubModelsCount; ++i)
	{
        Root[i].BinInit( Root, tm, &Textures, &Names, dynamic );

        if (Root[i].ChildGet())
			Root[i].ChildGet()->Parent = &Root[i];
		if (Root[i].NextGet())
			Root[i].NextGet()->Parent = Root[i].Parent;
	}
}

void TSubModel::BinInit(TSubModel *s, float4x4 *m, std::vector<std::string> *t, std::vector<std::string> *n, bool dynamic)
{ // ustawienie wskaźników w submodelu
	//m7todo: brzydko
	iVisible = 1; // tymczasowo używane
	Child = (iChild > 0) ? s + iChild : nullptr; // zerowy nie może być potomnym
	Next = (iNext > 0) ? s + iNext : nullptr; // zerowy nie może być następnym
	fMatrix = ((iMatrix >= 0) && m) ? m + iMatrix : nullptr;
	if (n->size() && (iName >= 0))
	{
		pName = n->at(iName);
		if (!pName.empty())
		{ // jeśli dany submodel jest zgaszonym światłem, to
		  // domyślnie go ukrywamy
			if ((pName.size() >= 8) && (pName.substr(0, 8) == "Light_On"))
			{ // jeśli jest światłem numerowanym
				iVisible = 0; // to domyślnie wyłączyć, żeby się nie nakładało z
			}
			// obiektem "Light_Off"
			else if (dynamic)
			{ // inaczej wyłączało smugę w latarniach
				if ((pName.size() >= 3) && (pName.substr(pName.size() - 3, 3) == "_on"))
				{ // jeśli jest kontrolką w stanie zapalonym
					iVisible = 0; // to domyślnie wyłączyć, żeby się nie nakładało z
				}
			}
			// obiektem "_off"
		}
	}
	else
		pName = "";
	if (iTexture > 0)
	{ // obsługa stałej tekstury
		pTexture = t->at(iTexture);
		if (pTexture.find_last_of("/\\") == std::string::npos)
			pTexture.insert(0, Global::asCurrentTexturePath);
		TextureID = GfxRenderer.GetTextureId(pTexture, szTexturePath);
        if( ( iFlags & 0x30 ) == 0 ) {
            // texture-alpha based fallback if for some reason we don't have opacity flag set yet
            iFlags |=
                ( GfxRenderer.Texture( TextureID ).has_alpha ?
                    0x20 :
                    0x10 ); // 0x10-nieprzezroczysta, 0x20-przezroczysta
        }
    }
	else
		TextureID = iTexture;

	b_aAnim = b_Anim; // skopiowanie animacji do drugiego cyklu

    if( (eType == TP_FREESPOTLIGHT) && (iFlags & 0x10)) {
        // we've added light glare which needs to be rendered during transparent phase,
        // but models converted to e3d before addition won't have the render flag set correctly for this
        // so as a workaround we're doing it here manually
        iFlags |= 0x20;
    }
    // intercept and fix hotspot values if specified in degrees and not directly
    if( fCosFalloffAngle > 1.0f ) {
        fCosFalloffAngle = std::cos( DegToRad( 0.5f * fCosFalloffAngle ) );
    }
    if( fCosHotspotAngle > 1.0f ) {
        fCosHotspotAngle = std::cos( DegToRad( 0.5f * fCosHotspotAngle ) );
    }

	iFlags &= ~0x0200; // wczytano z pliku binarnego (nie jest właścicielem tablic)
/*
	iVboPtr = tVboPtr;
*/
};

void TModel3d::LoadFromBinFile(std::string const &FileName, bool dynamic)
{ // wczytanie modelu z pliku binarnego
	WriteLog("Loading binary format 3d model data from \"" + FileName + "\"...");
	
	std::ifstream file(FileName, std::ios::binary);

	uint32_t type = sn_utils::ld_uint32(file);
	uint32_t size = sn_utils::ld_uint32(file) - 8;

	if (type != MAKE_ID4('E', '3', 'D', '0'))
		throw std::runtime_error("e3d: unknown main chunk");

	deserialize(file, size, dynamic);
	file.close();

	WriteLog("Finished loading 3d model data from \"" + FileName + "\"");
};

void TModel3d::LoadFromTextFile(std::string const &FileName, bool dynamic)
{ // wczytanie submodelu z pliku tekstowego
	WriteLog("Loading text format 3d model data from \"" + FileName + "\"...");
	iFlags |= 0x0200; // wczytano z pliku tekstowego (właścicielami tablic są submodle)
	cParser parser(FileName, cParser::buffer_FILE); // Ra: tu powinno być "models\\"...
	TSubModel *SubModel;
	std::string token = parser.getToken<std::string>();
	iNumVerts = 0; // w konstruktorze to jest
	while (token != "" || parser.eof())
	{
		std::string parent;
		// parser.getToken(parent);
		parser.getTokens(1, false); // nazwa submodelu nadrzędnego bez zmieny na małe
		parser >> parent;
        if( parent == "" ) {
            break;
        }
		SubModel = new TSubModel();
		iNumVerts += SubModel->Load(parser, this, /*iNumVerts,*/ dynamic);

        // będzie potrzebne do wyliczenia pozycji, np. pantografu
		SubModel->Parent = AddToNamed(parent.c_str(), SubModel); 

		parser.getTokens();
		parser >> token;
	}
	// Ra: od wersji 334 przechylany jest cały model, a nie tylko pierwszy submodel
	// ale bujanie kabiny nadal używa bananów :( od 393 przywrócone, ale z dodatkowym warunkiem
	if (Global::iConvertModels & 4)
	{ // automatyczne banany czasem psuły przechylanie kabin...
		if (dynamic && Root)
		{
			if (Root->NextGet()) // jeśli ma jakiekolwiek kolejne
			{ // dynamic musi mieć "banana", bo tylko pierwszy obiekt jest animowany, a następne nie
				SubModel = new TSubModel(); // utworzenie pustego
				SubModel->ChildAdd(Root);
				Root = SubModel;
				++iSubModelsCount;
			}
			Root->WillBeAnimated(); // bo z tym jest dużo problemów
		}
	}
}

void TModel3d::Init()
{ // obrócenie początkowe układu współrzędnych, dla
  // pojazdów wykonywane po analizie animacji
	if (iFlags & 0x8000)
		return; // operacje zostały już wykonane
	if (Root)
	{
		if (iFlags & 0x0200) // jeśli wczytano z pliku tekstowego
		{ // jest jakiś dziwny błąd, że obkręcany ma być tylko ostatni submodel
		  // głównego łańcucha
		  // TSubModel *p=Root;
		  // do
		  //{p->InitialRotate(true); //ostatniemu należy się konwersja układu
		  // współrzędnych
		  // p=p->NextGet();
		  //}
		  // while (p->NextGet())
		  // Root->InitialRotate(false); //a poprzednim tylko optymalizacja
			Root->InitialRotate(true); // argumet określa, czy wykonać pierwotny obrót
		}
		iFlags |= Root->FlagsCheck() | 0x8000; // flagi całego modelu
        if (iNumVerts) {
            if( m_geometrybank == NULL ) {
                m_geometrybank = GfxRenderer.Create_Bank();
            }
            std::size_t dataoffset = 0;
            Root->create_geometry( dataoffset, m_geometrybank );
        }
        if( ( Global::iConvertModels > 0 )
         && ( false == asBinary.empty() ) ) {
            SaveToBinFile( asBinary );
            asBinary = ""; // zablokowanie powtórnego zapisu
        }
    }
};

void TModel3d::BreakHierarhy()
{
	Error("Not implemented yet :(");
};

//-----------------------------------------------------------------------------
// 2012-02 funkcje do tworzenia terenu z E3D
//-----------------------------------------------------------------------------

int TModel3d::TerrainCount()
{ // zliczanie kwadratów kilometrowych (główna
  // linia po Next) do tworznia tablicy
	int i = 0;
	TSubModel *r = Root;
	while (r)
	{
		r = r->NextGet();
		++i;
	}
	return i;
};
TSubModel *TModel3d::TerrainSquare(int n)
{ // pobieranie wskaźnika do submodelu (n)
	int i = 0;
	TSubModel *r = Root;
	while (i < n)
	{
		r = r->NextGet();
		++i;
	}
	r->UnFlagNext(); // blokowanie wyświetlania po Next głównej listy
	return r;
};
#ifdef EU07_USE_OLD_RENDERCODE
void TModel3d::TerrainRenderVBO(int n)
{ // renderowanie terenu z VBO
	::glPushMatrix();
    ::glTranslated( -Global::pCameraPosition.x, -Global::pCameraPosition.y, -Global::pCameraPosition.z );
    TSubModel *r = Root;
    while( r ) {
        if( r->iVisible == n ) // tylko jeśli ma być widoczny w danej ramce (problem dla 0==false)
            GfxRenderer.Render( r ); // sub kolejne (Next) się nie wyrenderują
        r = r->NextGet();
    }
    ::glPopMatrix();
};
#endif