#pragma	once

#include "dumb3d.h"
#include "float3d.h"

// sky gradient based on "A practical analytic model for daylight" 
// by A. J. Preetham Peter Shirley Brian Smits (University of Utah)

class CSkyDome {
public:
    CSkyDome( int const Tesselation = 54 );
	~CSkyDome();
	void Generate();
	void RebuildColors();

	bool SetSunPosition( Math3D::vector3 const &Direction );
	
	void SetTurbidity( const float Turbidity = 5.0f );
	void SetExposure( const bool Linearexposure, const float Expfactor );		
	void SetOvercastFactor( const float Overcast = 0.0f );
	void SetGammaCorrection( const float Gamma = 2.2f );

	// update skydome
    void Update( Math3D::vector3 const &Sun );
	// render skydome to screen
	void Render();

    // retrieves average colour of the sky dome
    float3 GetAverageColor() { return m_averagecolour; }

private:
	// shading parametrs
    float3 m_sundirection;
    float m_thetasun, m_phisun;
    float m_turbidity;
    bool m_linearexpcontrol;
    float m_expfactor;
    float m_overcast;
    float m_gammacorrection;
    float3 m_averagecolour;

	// data
    int const m_tesselation;
    std::vector<float3> m_vertices;
//    std::vector<float3> m_normals;
    std::vector<float3> m_colours;

	static float m_distributionluminance[ 5 ][ 2 ];
    static float m_distributionxcomp[ 5 ][ 2 ];
    static float m_distributionycomp[ 5 ][ 2 ];

    static float m_zenithxmatrix[ 3 ][ 4 ];
    static float m_zenithymatrix[ 3 ][ 4 ];
	
	// coloring
	void GetPerez( float *Perez, float Distribution[ 5 ][ 2 ], const float Turbidity );
	float GetZenith( float Zenithmatrix[ 3 ][ 4 ], const float Theta, const float Turbidity );		
	float PerezFunctionO1( float Perezcoeffs[ 5 ], const float Thetasun, const float Zenithval );
	float PerezFunctionO2( float Perezcoeffs[ 5 ], const float Icostheta, const float Gamma, const float Cosgamma2, const float Zenithval );
};
