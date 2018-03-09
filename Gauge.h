/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "Classes.h"
#include "sound.h"

enum TGaugeType {
    // typ ruchu
    gt_Unknown, // na razie nie znany
    gt_Rotate, // obrót
    gt_Move, // przesunięcie równoległe
    gt_Wiper, // obrót trzech kolejnych submodeli o ten sam kąt (np. wycieraczka, drzwi harmonijkowe)
    gt_Digital // licznik cyfrowy, np. kilometrów
};

// animowany wskaźnik, mogący przyjmować wiele stanów pośrednich
class TGauge { 

public:
// methods
    TGauge() = default;
    inline
    void Clear() { *this = TGauge(); }
    void Init(TSubModel *NewSubModel, TGaugeType eNewTyp, double fNewScale = 1, double fNewOffset = 0, double fNewFriction = 0, double fNewValue = 0);
    bool Load(cParser &Parser, TDynamicObject const *Owner, TModel3d *md1, TModel3d *md2 = nullptr, double mul = 1.0);
    void PermIncValue(double fNewDesired);
    void IncValue(double fNewDesired);
    void DecValue(double fNewDesired);
    void UpdateValue( double fNewDesired );
    void UpdateValue( double fNewDesired, sound_source &Fallbacksound );
    void PutValue(double fNewDesired);
    double GetValue() const;
    double GetDesiredValue() const;
    void Update();
    void AssignFloat(float *fValue);
    void AssignDouble(double *dValue);
    void AssignInt(int *iValue);
    void UpdateValue();
    // returns offset of submodel associated with the button from the model centre
    glm::vec3 model_offset() const;
// members
    TSubModel *SubModel { nullptr }; // McZapkie-310302: zeby mozna bylo sprawdzac czy zainicjowany poprawnie

private:
// methods
// imports member data pair from the config file
    bool
        Load_mapping( cParser &Input );
    void UpdateValue( double fNewDesired, sound_source *Fallbacksound );
// members
    TGaugeType eType { gt_Unknown }; // typ ruchu
    double fFriction { 0.0 }; // hamowanie przy zliżaniu się do zadanej wartości
    double fDesiredValue { 0.0 }; // wartość docelowa
    double fValue { 0.0 }; // wartość obecna
    double fOffset { 0.0 }; // wartość początkowa ("0")
    double fScale { 1.0 }; // wartość końcowa ("1")
    char cDataType; // typ zmiennej parametru: f-float, d-double, i-int
    union {
        // wskaźnik na parametr pokazywany przez animację
        float *fData;
        double *dData { nullptr };
        int *iData;
    };
    sound_source m_soundfxincrease { sound_placement::internal, EU07_SOUND_CABCONTROLSCUTOFFRANGE }; // sound associated with increasing control's value
    sound_source m_soundfxdecrease { sound_placement::internal, EU07_SOUND_CABCONTROLSCUTOFFRANGE }; // sound associated with decreasing control's value
    std::map<int, sound_source> m_soundfxvalues; // sounds associated with specific values

};

//---------------------------------------------------------------------------
