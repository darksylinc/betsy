#version 430 core

#include "/media/matias/Datos/SyntaxHighlightingMisc.h"

#include "CrossPlatformSettings_piece_all.glsl"
#include "UavCrossPlatform_piece_all.glsl"

#define cETC1ColorDeltaMin -4
#define cETC1ColorDeltaMax 3

#define cETC1IntenModifierNumBits 3
#define cETC1IntenModifierValues 8

#define FLT_MAX 340282346638528859811704183484516925440.0f

#define cLowQuality 0

shared float3 srcPixelsBlock[16];
shared float3 srcPixelSubblock[8];

uniform float p_quality;

static const float4 g_etc1_inten_tables[cETC1IntenModifierValues] = {
	float4( -8, -2, 2, 8 ),       float4( -17, -5, 5, 17 ),    float4( -29, -9, 9, 29 ),
	float4( -42, -13, 13, 42 ),   float4( -60, -18, 18, 60 ),  float4( -80, -24, 24, 80 ),
	float4( -106, -33, 33, 106 ), float4( -183, -47, 47, 183 )
};

struct PotentialSolution
{
	potential_solution() : m_coords(), error( cUINT64_MAX ), m_valid( false ) {}

	float3 rgbIntLS;  // in range [0; limit]
	uint intenTable;
	bool bUseColor4;

	// Has 8 selectors, 8-bits each.
	uint selectors[2];
	float error;

	void clear()
	{
		m_coords.clear();
		error = FLT_MAX;
		m_valid = false;
	}
};

inline float3 getScaledColor( const float3 rgbInt, const bool bUseColor4 )
{
	const float a = bUseColor4 ? 1.0f : 0.25f;
	const float b = bUseColor4 ? 16.0f : 8.0f;
	return floor( rgbInt * a ) + rgbInt * b;
}

float calcError( float3 a, float3 b )
{
	float3 diff = a - b;
	return dot( diff, diff );
}

bool evaluate_solution( const float3 blockRgbInt, const bool bUseColor4, const float3 baseColor5,
						const bool constrainAgainstBaseColor5, inout PotentialSolution &bestSolution )
{
	PotentialSolution trialSolution;

	if( constrainAgainstBaseColor5 )
	{
		const float3 d = blockRgbInt - baseColor5;

		if( ( min3( d.r, d.g, d.b ) < cETC1ColorDeltaMin ) ||  //
			( max3( d.r, d.g, d.b ) > cETC1ColorDeltaMax ) )
		{
			return false;
		}
	}

	const float3 baseColor = getScaledColor( blockRgbInt, bUseColor4 );

	const uint n = 8u;

	trialSolution.error = FLT_MAX;

	for( uint inten_table = 0; inten_table < cETC1IntenModifierValues; ++inten_table )
	{
		const float4 pIntenTable = g_etc1_inten_tables[inten_table];

		float3 blockColorsInt[4];
		for( uint s = 0u; s < 4u; s++ )
		{
			const float yd = pIntenTable[s];
			blockColorsInt[s] = baseColor + yd;
		}

		float total_error = 0;
		float4 tempSelectors[2];

		for( uint c = 0u; c < n; ++c )
		{
			const float3 srcPixel = srcPixelSubblock[c];

			float best_selector_index = 0;
			float best_error = calcError( srcPixel, blockColorsInt[0] );

			float trial_error = calcError( srcPixel, blockColorsInt[1] );
			if( trial_error < best_error )
			{
				best_error = trial_error;
				best_selector_index = 1;
			}

			trial_error = calcError( srcPixel, blockColorsInt[2] );
			if( trial_error < best_error )
			{
				best_error = trial_error;
				best_selector_index = 2;
			}

			trial_error = calcError( srcPixel, blockColorsInt[3] );
			if( trial_error < best_error )
			{
				best_error = trial_error;
				best_selector_index = 3;
			}

			tempSelectors[c >> 2u][c & 0x04u] = best_selector_index;

			total_error += best_error;
			if( total_error >= trialSolution.error )
				break;
		}

		if( total_error < trialSolution.error )
		{
			trialSolution.error = total_error;
			trialSolution.intenTable = inten_table;
			trialSolution.selectors[0] = packUnorm4x8( tempSelectors[0] * ( 1.0f / 255.0f ) );
			trialSolution.selectors[1] = packUnorm4x8( tempSelectors[1] * ( 1.0f / 255.0f ) );
			// trialSolution.valid = true;
		}
	}

	trialSolution.rgbInt = blockRgbInt;
	trialSolution.bUseColor4 = bUseColor4;

	bool success = false;
	if( trialSolution.error < bestSolution.error )
	{
		bestSolution = trialSolution;
		success = true;
	}

	return success;
}

float getScanDelta( const float iDeltai, const float scanDeltaAbsMin, const float scanDeltaAbsMax )
{
	const float numAbsDeltas = scanDeltaAbsMax - scanDeltaAbsMin + 1.0f;

	if( iDeltai < numAbsDeltas )
		return -scanDeltaAbsMax + iDeltai;
	else
		return scanDeltaAbsMin + iDeltai;
}

/**
@brief etc1_optimizer_compute
@param scanDeltaAbsMin
@param scanDeltaAbsMax
@param avgColour
@param avgColourLS
	Same as avgColour but in limit space, i.e. range [0; limit]
@param bUseColor4
@param baseColor5
@param constrainAgainstBaseColor5
@param bestSolution
@return
	True if we found a solution
	False if we failed to find any
*/
bool etc1_optimizer_compute( const float scanDeltaAbsMin, const float scanDeltaAbsMax, float3 avgColour,
							 float3 avgColourLS, const bool bUseColor4, const float3 baseColor5,
							 const bool constrainAgainstBaseColor5,
							 inout PotentialSolution &bestSolution )
{
	// const float numSrcPixels = 8;  // Subblock of 2x4 or 4x2

	const float limit = bUseColor4 ? 15 : 31;

	const float scanDeltaSize = ( scanDeltaAbsMax - scanDeltaAbsMin ) * 2.0f;

	for( float zdi = 0; zdi < scanDeltaSize; ++zdi )
	{
		float3 blockRgbInt;
		const float zd = getScanDelta( zdi, scanDeltaAbsMin, scanDeltaAbsMax );
		blockRgbInt.b = avgColourLS.b + zd;
		if( blockRgbInt.b < 0 )
			continue;
		else if( blockRgbInt.b > limit )
			break;

		for( float ydi = 0; ydi < scanDeltaSize; ++ydi )
		{
			const float yd = getScanDelta( ydi, scanDeltaAbsMin, scanDeltaAbsMax );
			blockRgbInt.g = avgColourLS.g + yd;
			if( blockRgbInt.g < 0 )
				continue;
			else if( blockRgbInt.g > limit )
				break;

			for( float xdi = 0; xdi < scanDeltaSize; xdi++ )
			{
				const float xd = getScanDelta( xdi, scanDeltaAbsMin, scanDeltaAbsMax );
				blockRgbInt.r = avgColourLS.r + xd;
				if( blockRgbInt.r < 0 )
					continue;
				else if( blockRgbInt.r > limit )
					break;

				if( !evaluate_solution( blockRgbInt, bUseColor4, baseColor5, constrainAgainstBaseColor5,
										bestSolution ) )
				{
					continue;
				}

				// Now we have the input block, the avg. color of the input pixels, a set of trial
				// selector indices, and the block color+intensity index. Now, for each component,
				// attempt to refine the current solution by solving a simple linear equation.
				// So what this means: optimal_block_color = avg_input - avg_inten_delta
				// So the optimal block color can be computed by taking the average block color and
				// subtracting the current average of the intensity delta.
				// Unfortunately, optimal_block_color must then be quantized to 555 or 444 so it's not
				// always possible to improve matters using this formula. Also, the formula is for
				// unclamped intensity deltas. The actual implementation takes into account clamping.

				float4 pSelectors[2];
				pSelectors[0] = unpackUnorm4x8( bestSolution.selectors[0] ) * 255.0f;
				pSelectors[1] = unpackUnorm4x8( bestSolution.selectors[1] ) * 255.0f;

				const int maxRefinementTrials =
					p_quality == cLowQuality ? 2 : ( ( ( xd == 0 | yd == 0 | zd == 0 ) == 0 ) ? 4 : 2 );
				for( int refinementTrial = 0; refinementTrial < maxRefinementTrials; ++refinementTrial )
				{
					const float4 pIntenTable = g_etc1_inten_tables[bestSolution.intenTable];

					float3 deltaSum = float3( 0.0f, 0.0f, 0.0f );
					float3 base_color = getScaledColor( bestSolution.rgbInt, bUseColor4 );

					for( uint r = 0u; r < 8u; ++r )
					{
						const uint s = pSelectors[r >> 2u][r & 0x04u] * 255.0f;
						const int yd = pIntenTable[s];
						// Compute actual delta being applied to each pixel,
						// taking into account clamping.
						deltaSum += clamp( base_color + yd, 0.0, 255.0 ) - base_color;
					}

					if( deltaSum.r == 0.0f && deltaSum.g == 0.0f && deltaSum.b == 0.0f )
						break;

					const float3 avgDelta = deltaSum * 0.125f;
					const float3 blockRgbInt1 =
						clamp( round( ( avgColour - avgDelta ) * limit * ( 1.0f / 255.0f ) ), 0, limit );

					bool skip = false;

					if( ( blockRgbInt.r == blockRgbInt1.r &&  //
						  blockRgbInt.g == blockRgbInt1.g &&  //
						  blockRgbInt.b == blockRgbInt1.b ) ||
						( bestSolution.rgbIntLS.r == blockRgbInt1.r &&  //
						  bestSolution.rgbIntLS.g == blockRgbInt1.g &&  //
						  bestSolution.rgbIntLS.b == blockRgbInt1.b ) ||
						( avgColourLS.r == blockRgbInt1.r &&  //
						  avgColourLS.g == blockRgbInt1.g &&  //
						  avgColourLS.b == blockRgbInt1.b ) )
					{
						// Skip refinement
						break;
					}

					if( !evaluate_solution( blockRgbInt1, bUseColor4, baseColor5,
											constrainAgainstBaseColor5, bestSolution ) )
					{
						break;
					}

				}  // refinementTrial
			}
		}
	}
}

void main()
{
}
