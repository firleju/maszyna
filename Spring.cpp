/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "Spring.h"

TSpring::TSpring()
{
    vForce1 = vForce2 = vector3(0, 0, 0);
    Ks = 0;
    Kd = 0;
    restLen = 0;
}

TSpring::~TSpring()
{
}

void TSpring::Init(double nrestLen, double nKs, double nKd)
{
    Ks = nKs;
    Kd = nKd;
    restLen = nrestLen;
}

bool TSpring::ComputateForces(vector3 pPosition1, vector3 pPosition2)
{

    double dist, Hterm, Dterm;
    vector3 springForce, deltaV, deltaP;
    //		p1 = &system[spring->p1];
    //		p2 = &system[spring->p2];
    //		VectorDifference(&p1->pos,&p2->pos,&deltaP);	// Vector distance
    deltaP = pPosition1 - pPosition2;
    //		dist = VectorLength(&deltaP);					// Magnitude of
    // deltaP
    dist = deltaP.Length();
    if (dist == 0)
    {
        vForce1 = vForce2 = vector3(0, 0, 0);
        return false;
    }

    //		Hterm = (dist - spring->restLen) * spring->Ks;	// Ks * (dist - rest)
    Hterm = (dist - restLen) * Ks; // Ks * (dist - rest)

    //		VectorDifference(&p1->v,&p2->v,&deltaV);		// Delta Velocity Vector
    deltaV = pPosition1 - pPosition2;

    //		Dterm = (DotProduct(&deltaV,&deltaP) * spring->Kd) / dist; // Damping Term
    // Dterm = (DotProduct(deltaV,deltaP) * Kd) / dist;
    Dterm = 0;

    //		ScaleVector(&deltaP,1.0f / dist, &springForce);	// Normalize Distance Vector
    //		ScaleVector(&springForce,-(Hterm + Dterm),&springForce);	// Calc Force
    springForce = deltaP / dist * (-(Hterm + Dterm));
    //		VectorSum(&p1->f,&springForce,&p1->f);			// Apply to Particle 1
    //		VectorDifference(&p2->f,&springForce,&p2->f);	// - Force on Particle 2
    vForce1 = springForce;
    vForce2 = springForce;

    return true;
}

void TSpring::Render()
{
}

//---------------------------------------------------------------------------
