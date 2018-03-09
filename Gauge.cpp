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
    Copyright (C) 2001-2004  Marcin Wozniak

*/

#include "stdafx.h"
#include "Gauge.h"
#include "parser.h"
#include "Model3d.h"
#include "Timer.h"
#include "Logs.h"
#include "renderer.h"

void TGauge::Init(TSubModel *NewSubModel, TGaugeType eNewType, double fNewScale, double fNewOffset, double fNewFriction, double fNewValue)
{ // ustawienie parametrów animacji submodelu
    if (NewSubModel) {
        // warunek na wszelki wypadek, gdyby się submodel nie podłączył
        fFriction = fNewFriction;
        fValue = fNewValue;
        fOffset = fNewOffset;
        fScale = fNewScale;
        SubModel = NewSubModel;
        eType = eNewType;
        if (eType == gt_Digital) {

            TSubModel *sm = SubModel->ChildGet();
            do {
                // pętla po submodelach potomnych i obracanie ich o kąt zależy od cyfry w (fValue)
                if (sm->pName.size())
                { // musi mieć niepustą nazwę
                    if (sm->pName[0] >= '0')
                        if (sm->pName[0] <= '9')
                            sm->WillBeAnimated(); // wyłączenie optymalizacji
                }
                sm = sm->NextGet();
            } while (sm);
        }
        else // a banan może być z optymalizacją?
            NewSubModel->WillBeAnimated(); // wyłączenie ignowania jedynkowego transformu
        // pass submodel location to defined sounds
        auto const offset { model_offset() };
        m_soundfxincrease.offset( offset );
        m_soundfxdecrease.offset( offset );
        for( auto &soundfxrecord : m_soundfxvalues ) {
            soundfxrecord.second.offset( offset );
        }
    }
};

bool TGauge::Load( cParser &Parser, TDynamicObject const *Owner, TModel3d *md1, TModel3d *md2, double mul ) {

    std::string submodelname, gaugetypename;
    double scale, offset, friction;

    Parser.getTokens();
    if( Parser.peek() != "{" ) {
        // old fixed size config
        Parser >> submodelname;
        gaugetypename = Parser.getToken<std::string>( true );
        Parser.getTokens( 3, false );
        Parser
            >> scale
            >> offset
            >> friction;
    }
    else {
        // new, block type config
        // TODO: rework the base part into yaml-compatible flow style mapping
        submodelname = Parser.getToken<std::string>( false );
        gaugetypename = Parser.getToken<std::string>( true );
        Parser.getTokens( 3, false );
        Parser
            >> scale
            >> offset
            >> friction;
        // new, variable length section
        while( true == Load_mapping( Parser ) ) {
            ; // all work done by while()
        }
    }

    // bind defined sounds with the button owner
    m_soundfxincrease.owner( Owner );
    m_soundfxdecrease.owner( Owner );
    for( auto &soundfxrecord : m_soundfxvalues ) {
        soundfxrecord.second.owner( Owner );
    }

	scale *= mul;
    TSubModel *submodel = md1->GetFromName( submodelname );
    if( scale == 0.0 ) {
        ErrorLog( "Bad model: scale of 0.0 defined for sub-model \"" + submodelname + "\" in 3d model \"" + md1->NameGet() + "\". Forcing scale of 1.0 to prevent division by 0", logtype::model );
        scale = 1.0;
    }
    if (submodel) // jeśli nie znaleziony
        md2 = nullptr; // informacja, że znaleziony
    else if (md2) // a jest podany drugi model (np. zewnętrzny)
        submodel = md2->GetFromName(submodelname); // to może tam będzie, co za różnica gdzie
    if( submodel == nullptr ) {
        ErrorLog( "Bad model: failed to locate sub-model \"" + submodelname + "\" in 3d model \"" + md1->NameGet() + "\"", logtype::model );
    }

    std::map<std::string, TGaugeType> gaugetypes {
        { "mov", gt_Move },
        { "wip", gt_Wiper },
        { "dgt", gt_Digital }
    };
    auto lookup = gaugetypes.find( gaugetypename );
    auto const type = (
        lookup != gaugetypes.end() ?
            lookup->second :
            gt_Rotate );
    Init(submodel, type, scale, offset, friction);

    return md2 != nullptr; // true, gdy podany model zewnętrzny, a w kabinie nie było
};

bool
TGauge::Load_mapping( cParser &Input ) {

    // token can be a key or block end
    std::string const key { Input.getToken<std::string>( true, "\n\r\t  ,;" ) };
    if( ( true == key.empty() ) || ( key == "}" ) ) { return false; }
    // if not block end then the key is followed by assigned value or sub-block
    if( key == "soundinc:" ) {
        m_soundfxincrease.deserialize( Input, sound_type::single );
    }
    else if( key == "sounddec:" ) {
        m_soundfxdecrease.deserialize( Input, sound_type::single );
    }
    else if( key.compare( 0, std::min<std::size_t>( key.size(), 5 ), "sound" ) == 0 ) {
        // sounds assigned to specific gauge values, defined by key soundFoo: where Foo = value
        auto const indexstart { key.find_first_of( "-1234567890" ) };
        auto const indexend { key.find_first_not_of( "-1234567890", indexstart ) };
        if( indexstart != std::string::npos ) {
            m_soundfxvalues.emplace(
                std::stoi( key.substr( indexstart, indexend - indexstart ) ),
                sound_source( sound_placement::internal, EU07_SOUND_CABCONTROLSCUTOFFRANGE ).deserialize( Input, sound_type::single ) );
        }
    }
    return true; // return value marks a key: value pair was extracted, nothing about whether it's recognized
}

void TGauge::PermIncValue(double fNewDesired)
{
    fDesiredValue = fDesiredValue + fNewDesired * fScale + fOffset;
    if (fDesiredValue - fOffset > 360 / fScale)
    {
        fDesiredValue = fDesiredValue - (360 / fScale);
        fValue = fValue - (360 / fScale);
    }
};

void TGauge::IncValue(double fNewDesired)
{ // używane tylko dla uniwersali
    fDesiredValue = fDesiredValue + fNewDesired * fScale + fOffset;
    if (fDesiredValue > fScale + fOffset)
        fDesiredValue = fScale + fOffset;
};

void TGauge::DecValue(double fNewDesired)
{ // używane tylko dla uniwersali
    fDesiredValue = fDesiredValue - fNewDesired * fScale + fOffset;
    if (fDesiredValue < 0)
        fDesiredValue = 0;
};

void
TGauge::UpdateValue( double fNewDesired ) {

    return UpdateValue( fNewDesired, nullptr );
}

void
TGauge::UpdateValue( double fNewDesired, sound_source &Fallbacksound ) {

    return UpdateValue( fNewDesired, &Fallbacksound );
}

// ustawienie wartości docelowej. plays provided fallback sound, if no sound was defined in the control itself
void
TGauge::UpdateValue( double fNewDesired, sound_source *Fallbacksound ) {

    auto const desiredtimes100 = static_cast<int>( std::round( 100.0 * fNewDesired ) );
    if( static_cast<int>( std::round( 100.0 * ( fDesiredValue - fOffset ) / fScale ) ) == desiredtimes100 ) {
        return;
    }
    fDesiredValue = fNewDesired * fScale + fOffset;
    // if there's any sound associated with new requested value, play it
    // check value-specific table first...
    if( desiredtimes100 % 100 == 0 ) {
        // filter out values other than full integers
        auto const lookup = m_soundfxvalues.find( desiredtimes100 / 100 );
        if( lookup != m_soundfxvalues.end() ) {
            lookup->second.play();
            return;
        }
    }
    // ...and if there isn't any, fall back on the basic set...
    auto const currentvalue = GetValue();
    if( ( currentvalue < fNewDesired )
     && ( false == m_soundfxincrease.empty() ) ) {
        // shift up
        m_soundfxincrease.play();
    }
    else if( ( currentvalue > fNewDesired )
          && ( false == m_soundfxdecrease.empty() ) ) {
        // shift down
        m_soundfxdecrease.play();
    }
    else if( Fallbacksound != nullptr ) {
        // ...and if that fails too, try the provided fallback sound from legacy system
        Fallbacksound->play();
    }
};

void TGauge::PutValue(double fNewDesired)
{ // McZapkie-281102: natychmiastowe wpisanie wartosci
    fDesiredValue = fNewDesired * fScale + fOffset;
    fValue = fDesiredValue;
};

double TGauge::GetValue() const {
    // we feed value in range 0-1 so we should be getting it reported in the same range
    return ( fValue - fOffset ) / fScale;
}

double TGauge::GetDesiredValue() const {
    // we feed value in range 0-1 so we should be getting it reported in the same range
    return ( fDesiredValue - fOffset ) / fScale;
}

void TGauge::Update() {

    if( fValue != fDesiredValue ) {
        float dt = Timer::GetDeltaTime();
        if( ( fFriction > 0 ) && ( dt < 0.5 * fFriction ) ) {
            // McZapkie-281102: zabezpieczenie przed oscylacjami dla dlugich czasow
            fValue += dt * ( fDesiredValue - fValue ) / fFriction;
            if( std::abs( fDesiredValue - fValue ) <= 0.0001 ) {
                // close enough, we can stop updating the model
                fValue = fDesiredValue; // set it exactly as requested just in case it matters
            }
        }
        else {
            fValue = fDesiredValue;
        }
    }
    if( SubModel )
    { // warunek na wszelki wypadek, gdyby się submodel nie podłączył
        TSubModel *sm;
        switch (eType)
        {
        case gt_Rotate:
            SubModel->SetRotate(float3(0, 1, 0), fValue * 360.0);
            break;
        case gt_Move:
            SubModel->SetTranslate(float3(0, 0, fValue));
            break;
        case gt_Wiper:
            SubModel->SetRotate(float3(0, 1, 0), fValue * 360.0);
            sm = SubModel->ChildGet();
            if (sm)
            {
                sm->SetRotate(float3(0, 1, 0), fValue * 360.0);
                sm = sm->ChildGet();
                if (sm)
                    sm->SetRotate(float3(0, 1, 0), fValue * 360.0);
            }
            break;
        case gt_Digital: // Ra 2014-07: licznik cyfrowy
            sm = SubModel->ChildGet();
/*			std::string n = FormatFloat( "0000000000", floor( fValue ) ); // na razie tak trochę bez sensu
*/			std::string n( "000000000" + std::to_string( static_cast<int>( std::floor( fValue ) ) ) );
			if( n.length() > 10 ) { n.erase( 0, n.length() - 10 ); } // also dumb but should work for now
            do
            { // pętla po submodelach potomnych i obracanie ich o kąt zależy od cyfry w (fValue)
                if( sm->pName.size() ) {
                    // musi mieć niepustą nazwę
                    if( ( sm->pName[ 0 ] >= '0' )
                     && ( sm->pName[ 0 ] <= '9' ) ) {

                        sm->SetRotate(
                            float3( 0, 1, 0 ),
                            -36.0 * ( n[ '0' + 9 - sm->pName[ 0 ] ] - '0' ) );
                    }
                }
                sm = sm->NextGet();
            } while (sm);
            break;
        }
    }
};

void TGauge::AssignFloat(float *fValue)
{
    cDataType = 'f';
    fData = fValue;
};
void TGauge::AssignDouble(double *dValue)
{
    cDataType = 'd';
    dData = dValue;
};
void TGauge::AssignInt(int *iValue)
{
    cDataType = 'i';
    iData = iValue;
};
void TGauge::UpdateValue()
{ // ustawienie wartości docelowej z parametru
    switch (cDataType)
    { // to nie jest zbyt optymalne, można by zrobić osobne funkcje
    case 'f':
        fDesiredValue = (*fData) * fScale + fOffset;
        break;
    case 'd':
        fDesiredValue = (*dData) * fScale + fOffset;
        break;
    case 'i':
        fDesiredValue = (*iData) * fScale + fOffset;
        break;
    }
};

// returns offset of submodel associated with the button from the model centre
glm::vec3
TGauge::model_offset() const {

    return (
        SubModel != nullptr ?
            SubModel->offset( 1.f ) :
            glm::vec3() );
}
