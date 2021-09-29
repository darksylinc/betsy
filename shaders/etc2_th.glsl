#version 430 core

// T & H modes of ETC2

// #include "/media/matias/Datos/SyntaxHighlightingMisc.h"

#include "CrossPlatformSettings_piece_all.glsl"
#include "UavCrossPlatform_piece_all.glsl"

#define FLT_MAX 340282346638528859811704183484516925440.0f

shared uint g_srcPixelsBlock[16];
shared float2 g_bestCandidates[120 * 8];  //.x = error; .y = threadId

uniform sampler2D srcTex;

layout( rg32ui, binding = 0 ) uniform restrict writeonly uimage2D dstTexture;
layout( r32f, binding = 1 ) uniform restrict writeonly image2D dstError;
layout( rg32ui, binding = 2 ) uniform restrict readonly uimage2DArray c0c1Texture;

layout( local_size_x = 8,    //
		local_size_y = 120,  // 15 + 14 + 13 + ... + 1
		local_size_z = 1 ) in;

const float kDistances[8] = {  //
	3.0f / 255.0f,             //
	6.0f / 255.0f,             //
	11.0f / 255.0f,            //
	16.0f / 255.0f,            //
	23.0f / 255.0f,            //
	32.0f / 255.0f,            //
	41.0f / 255.0f,            //
	64.0f / 255.0f
};

/*
kTmodeEncoderR table generated with:
	static const int kSigned3bit[8] = { 0, 1, 2, 3, -4, -3, -2, -1 };
	int main()
	{
		for( int r1_4=0;r1_4<16;++r1_4 )
		{
			int R = r1_4 >> 2;
			int dR = r1_4 & 0x3;
			for( int Rx = 0; Rx < 8; Rx++ )
			{
				for( int dRx = 0; dRx < 2; dRx++ )
				{
					int Rtry = R | ( Rx << 2 );
					int dRtry = dR | ( dRx << 2 );
					if( ( Rtry + kSigned3bit[dRtry] ) < 0 || ( Rtry + kSigned3bit[dRtry] > 31 ) )
					{
						R = Rtry;
						dR = dRtry;
						break;
					}
				}
			}

			if( ( R + kSigned3bit[dR] ) >= 0 && ( R + kSigned3bit[dR] <= 31 ) )
				// this can't happen, should be an assert
				return -1;

			printf( "%i, ", ( ( R & 0x1F ) << 3 ) | ( dR & 0x7 ) );
			printf( "\n" );
		}

		return 0;
	}
*/
const float kTmodeEncoderR[16] = { 4, 5, 6, 7, 12, 13, 14, 235, 20, 21, 242, 243, 28, 249, 250, 251 };

/*
kHmodeEncoderRG table generated with:
	static const int kSigned3bit[8] = { 0, 1, 2, 3, -4, -3, -2, -1 };
	int main()
	{
		for( int r1_4 = 0; r1_4 < 16; ++r1_4 )
		{
			for( int g1_4 = 0; g1_4 < 16; ++g1_4 )
			{
				if( !( g1_4 & 0x1 ) )
				{
					// R1 + G1a. R + [dR] must be inside [0..31]. Scanning all values. Not smart.
					int R = r1_4;
					int dR = g1_4 >> 1;
					if( ( R + kSigned3bit[dR] ) < 0 || ( R + kSigned3bit[dR] > 31 ) )
						R |= ( 1 << 4 );

					if( ( R + kSigned3bit[dR] ) < 0 || ( R + kSigned3bit[dR] > 31 ) )
						return -1;  // wtf?

					printf( "%i, ", ( ( R & 0x1F ) << 3 ) | ( dR & 0x7 ) );
				}
			}
		}
		printf( "\n" );
		return 0;
	}
*/
const float kHmodeEncoderRG[128] =  //
	{ 0,   1,   2,   3,   132, 133, 134, 135, 8,   9,   10,  11,  140, 141, 142, 15,  16,  17,  18,
	  19,  148, 149, 22,  23,  24,  25,  26,  27,  156, 29,  30,  31,  32,  33,  34,  35,  36,  37,
	  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,
	  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,
	  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,
	  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
	  114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127 };

/*
kHmodeEncoderGB table generated with:
	static const int kSigned3bit[8] = { 0, 1, 2, 3, -4, -3, -2, -1 };
	#define BITS( byteval, lowbit, highbit ) \
		( ( ( byteval ) >> ( lowbit ) ) & ( ( 1 << ( ( highbit ) - ( lowbit ) + 1 ) ) - 1 ) )

	#define BIT( byteval, bit ) ( ( ( byteval ) >> ( bit ) ) & 0x1 )

	int main()
	{
		for( int g1_4 = 0; g1_4 < 2; ++g1_4 )
		{
			for( int b1_4 = 0; b1_4 < 16; ++b1_4 )
			{
				if( !( b1_4 & 0x1 ) )
				{
					// G1b + B1a + B1b[2 msb]. G + dG must be outside the range.
					int G = ( g1_4 & 0x1 ) << 1;
					G |= BIT( b1_4, 3 );
					int dG = BITS( b1_4, 1, 2 );
					for( int Gx = 0; Gx < 8; Gx++ )
					{
						for( int dGx = 0; dGx < 2; dGx++ )
						{
							int Gtry = G | ( Gx << 2 );
							int dGtry = dG | ( dGx << 2 );
							if( ( Gtry + kSigned3bit[dGtry] ) < 0 || ( Gtry + kSigned3bit[dGtry] > 31 ) )
							{
								G = Gtry;
								dG = dGtry;
								break;
							}
						}
					}

					if( ( G + kSigned3bit[dG] ) >= 0 && ( G + kSigned3bit[dG] <= 31 ) )
						return -1;  // wtf?

					printf( "%i, ", ( ( G & 0x1F ) << 3 ) | ( dG & 0x7 ) );
				}
			}
		}

		printf( "\n" );
		return 0;
	}
*/
const float kHmodeEncoderGB[16] =  //
	{ 4, 5, 6, 7, 12, 13, 14, 235, 20, 21, 242, 243, 28, 249, 250, 251 };

/*float rgb888to444( float3 rgbValue )
{
	rgbValue = floor( rgbValue * 15.0f / 255.0f + 0.5f );
	return rgbValue.r * 256.0f + rgbValue.g * 16.0f + rgbValue.b;
}*/
float3 rgb888to444( uint packedRgb )
{
	float3 rgbValue = unpackUnorm4x8( packedRgb ).xyz;
	rgbValue = floor( rgbValue * 15.0f + 0.5f );
	return rgbValue;
}

/// Quantizes 'srcValue' which is originally in 888 (full range),
/// converting it to 444 and then back to 888 (quantized)
uint quant4( const uint packedRgb )
{
	float3 rgbValue = unpackUnorm4x8( packedRgb ).xyz;  // Range [0; 1]
	rgbValue = floor( rgbValue * 15.0f + 0.5f );        // Convert to 444, range [0; 15]
	rgbValue = floor( rgbValue * 19.05f );              // Convert to 888, range [0; 255]
	return packUnorm4x8( float4( rgbValue * ( 1.0f / 255.0f ), 1.0f ) );
}

uint quant4( float3 rgbValue )
{
	rgbValue = floor( rgbValue * 15.0f / 255.0f + 0.5f );  // Convert to 444
	rgbValue = floor( rgbValue * 19.05f );                 // Convert to 888
	return packUnorm4x8( float4( rgbValue * ( 1.0f / 255.0f ), 1.0f ) );
}

float calcError( const uint colour0, const uint colour1 )
{
	float3 diff = unpackUnorm4x8( colour0 ).xyz - unpackUnorm4x8( colour1 ).xyz;
	return dot( diff, diff ) * 65025.0f;  // 65025 = 255 * 255
}

/// Performs:
///		packedRgb = saturate( packedRgb + value );
/// assuming 'value' is in range [0; 1]
uint addSat( const uint packedRgb, float value )
{
	float3 rgbValue = unpackUnorm4x8( packedRgb ).xyz;
	rgbValue = saturate( rgbValue + value );
	return packUnorm4x8( float4( rgbValue, 1.0f ) );
}

float etc2_th_mode_calcError( const bool hMode, const uint c0, const uint c1, float distance )
{
	uint paintColors[4];

	if( !hMode )
	{
		paintColors[0] = c0;
		paintColors[1] = addSat( c1, distance );
		paintColors[2] = c1;
		paintColors[3] = addSat( c1, -distance );
	}
	else
	{
		// We don't care about swapping c0 & c1 because we're only calculating error
		// and both variations produce the same result
		paintColors[0] = addSat( c0, distance );
		paintColors[1] = addSat( c0, -distance );
		paintColors[2] = addSat( c1, distance );
		paintColors[3] = addSat( c1, -distance );
	}

	float errAcc = 0;
	for( int k = 0; k < 16; ++k )
	{
		float bestDist = FLT_MAX;
		for( int idx = 0; idx < 4; ++idx )
		{
			const float dist = calcError( g_srcPixelsBlock[k], paintColors[idx] );
			bestDist = min( bestDist, dist );
		}

		errAcc += bestDist;
	}

	return errAcc;
}

uint etc2_gen_header_t_mode( const uint c0, const uint c1, const uint distIdx )
{
	// 4 bit colors
	const float3 rgb0 = rgb888to444( c0 );
	const float3 rgb1 = rgb888to444( c1 );

	const float fDistIdx = float( distIdx );

	float4 bytes;
	bytes.x = kTmodeEncoderR[uint( rgb0.x )];
	bytes.y = rgb0.y * 16.0f + rgb0.z;  // G0, B0
	bytes.z = rgb1.x * 16.0f + rgb1.y;  // R1, G1
	bytes.w = rgb1.z * 16.0f + floor( fDistIdx * 0.5f ) * 4.0f + 2.0f + mod( fDistIdx, 2.0f );
	// bytes.w = rgb1.z * 16.0f | ( ( distIdx >> 1u ) << 2u ) | ( 1u << 1u ) | ( distIdx & 0x1u );

	return packUnorm4x8( bytes * ( 1.0f / 255.0f ) );
}

uint etc2_gen_header_h_mode( const uint colour0, const uint colour1, const uint distIdx,
							 out bool bShouldSwap )
{
	uint c0, c1;
	// Note: if c0 == c1, no big deal because H is not the best choice of mode
	if( ( distIdx & 0x01u ) != 0u )
	{
		c0 = max( colour0, colour1 );
		c1 = min( colour0, colour1 );
		bShouldSwap = true;
	}
	else
	{
		c0 = min( colour0, colour1 );
		c1 = max( colour0, colour1 );
	}

	bShouldSwap = c0 != colour0;

	// 4 bit colors
	const float3 rgb0 = rgb888to444( c0 );
	const float3 rgb1 = rgb888to444( c1 );

	const float fDistIdx = float( distIdx );

	float4 bytes;
	// R0 (4 bits) + G0 (3 bits msb)
	bytes.x = kHmodeEncoderRG[uint( rgb0.x * 8.0f + floor( rgb0.y * 0.5f ) )];
	// G0 (1 bit lsb) + B0 (3 bits msb)
	bytes.y = kHmodeEncoderGB[uint( mod( rgb0.y, 2.0f ) * 8.0f + floor( rgb0.z * 0.5f ) )];
	// B0 (1 bit lsb) + R1 + G1 (3 bits msb)
	bytes.z = mod( rgb0.z, 2.0f ) * 128.0f + rgb1.x * 8.0f + floor( rgb1.y * 0.5f );
	// G1 (1 bit lsb) + B1 + distance (2 bits msb, the 3rd one was implicit in c0 < c1 order)
	bytes.w = mod( rgb1.g, 2.0f ) * 128.0f + rgb1.z * 8.0f + 2.0f;
	bytes.w += mod( floor( fDistIdx * 0.5f ), 2.0f ) + floor( fDistIdx * ( 1.0f / 4.0f ) ) * 4.0f;
	// bytes.w = ( rgb1.g & 0x1 ) << 7 | rgb1.z << 3 | 0x2 |
	//			 ( ( distIdx >> 1u ) & 0x01 ) | ( distIdx & 0x04 );

	return packUnorm4x8( bytes * ( 1.0f / 255.0f ) );
}

void etc2_th_mode_write( const bool hMode, uint c0, uint c1, float distance, uint distIdx )
{
	uint paintColors[4];

	uint2 outputBytes;

	if( !hMode )
	{
		outputBytes.x = etc2_gen_header_t_mode( c0, c1, distIdx );

		paintColors[0] = c0;
		paintColors[1] = addSat( c1, distance );
		paintColors[2] = c1;
		paintColors[3] = addSat( c1, -distance );
	}
	else
	{
		bool bShouldSwap;
		outputBytes.x = etc2_gen_header_h_mode( c0, c1, distIdx, bShouldSwap );

		if( bShouldSwap )
		{
			// swap( c0, c1 )
			const uint tmp = c0;
			c0 = c1;
			c1 = tmp;
		}

		paintColors[0] = addSat( c0, distance );
		paintColors[1] = addSat( c0, -distance );
		paintColors[2] = addSat( c1, distance );
		paintColors[3] = addSat( c1, -distance );
	}

	outputBytes.y = 0u;

	for( uint k = 0u; k < 16u; ++k )
	{
		float bestDist = FLT_MAX;
		uint bestIdx = 0u;

		for( uint idx = 0u; idx < 4u; ++idx )
		{
			const float dist = calcError( g_srcPixelsBlock[k], paintColors[idx] );
			if( dist < bestDist )
			{
				bestDist = dist;
				bestIdx = idx;
			}
		}

		// When k < 8 write bestIdx to region bits [8; 16) and [24; 32)
		// When k >= 8 write bestIdx to region bits [0; 8) and [16; 24)
		const uint bitStart0 = k < 8 ? 8u : 0u;
		const uint bitStart1 = k < 8 ? 24u : 16u;
		outputBytes.y |= ( ( ( bestIdx & 0x2u ) != 0u ? 1u : 0u ) << ( k & 0x7u ) ) << bitStart0;
		outputBytes.y |= ( ( bestIdx & 0x1u ) << ( k & 0x7u ) ) << bitStart1;
	}

	const uint2 dstUV = gl_WorkGroupID.xy;
	imageStore( dstTexture, int2( dstUV ), uint4( outputBytes.xy, 0u, 0u ) );
}

void main()
{
	if( gl_LocalInvocationIndex < 16u )
	{
		const uint2 pixelsToLoadBase = gl_WorkGroupID.xy << 2u;
		uint2 pixelsToLoad = pixelsToLoadBase;
		// Note ETC2 wants the src pixels transposed!
		pixelsToLoad.x += gl_LocalInvocationIndex >> 2u;    //+= threadId / 4
		pixelsToLoad.y += gl_LocalInvocationIndex & 0x03u;  //+= threadId % 4
		const float3 srcPixels0 = OGRE_Load2D( srcTex, int2( pixelsToLoad ), 0 ).xyz;
		g_srcPixelsBlock[gl_LocalInvocationIndex] = packUnorm4x8( float4( srcPixels0, 1.0f ) );
	}

	__sharedOnlyBarrier;

	// We have 120 potential pairs of colour candidates (some of these candidates may repeat)
	// ETC2 has 8 distance modes (3 bits) for each pair (should have high thread convergence)
	//
	// So we assign 1 thread to each
	const uint distIdx = gl_LocalInvocationID.x;

	const uint2 c0c1 =
		OGRE_imageLoad2DArray( c0c1Texture, uint3( gl_WorkGroupID.xy, gl_LocalInvocationID.y ) ).xy;
	const uint c0 = c0c1.x;
	const uint c1 = c0c1.y;

	float minErr = FLT_MAX;
	uint bestC0 = 0u;
	uint bestC1 = 0u;
	bool bestModeIsH;

	float err;

	const float distance = kDistances[distIdx];

	// T modes (swapping c0 / c1 makes produces different result)
	err = etc2_th_mode_calcError( false, c0, c1, distance );
	if( err < minErr )
	{
		minErr = err;
		bestC0 = c0;
		bestC1 = c1;
		bestModeIsH = false;
	}

	err = etc2_th_mode_calcError( false, c1, c0, distance );
	if( err < minErr )
	{
		minErr = err;
		bestC0 = c1;
		bestC1 = c0;
		bestModeIsH = false;
	}

	// H mode (swapping c0 / c1 is pointless, and is used in encoding to increase 1 bit)
	err = etc2_th_mode_calcError( true, c0, c1, distance );
	if( err < minErr )
	{
		minErr = err;
		bestC0 = c0;
		bestC1 = c1;
		bestModeIsH = true;
	}

	g_bestCandidates[gl_LocalInvocationIndex] = float2( minErr, gl_LocalInvocationIndex );

	__sharedOnlyBarrier;

#if 1
	// Parallel reduction to find the thread with the best solution
	// Because 960 != 1024, the last few operations on the last threads will repeat a bit.
	// However we don't care because the minimum of 2 values will always be the same.
	const uint iterations = 10u;  // 960 threads = 960 reductions <= 2¹⁰ -> 10 iterations
	for( uint i = 0u; i < iterations; ++i )
	{
		const uint mask = ( 1u << ( i + 1u ) ) - 1u;
		const uint idx = 1u << i;
		if( ( gl_LocalInvocationIndex & mask ) == 0u )
		{
			// nextThreadId can overflow (off by 1) since we're not power of 2
			const uint thisThreadId = gl_LocalInvocationIndex;
			const uint nextThreadId = min( gl_LocalInvocationIndex + idx, 960u - 1u );
			const float thisError = g_bestCandidates[thisThreadId].x;
			const float nextError = g_bestCandidates[nextThreadId].x;
			if( nextError < thisError )
				g_bestCandidates[thisThreadId] = float2( nextError, g_bestCandidates[nextThreadId].y );
		}
		__sharedOnlyBarrier;
	}
#else
	// Serial reduction, for ground-truth debugging
	if( gl_LocalInvocationIndex == 0u )
	{
		for( uint i = 1u; i < 960u; ++i )
		{
			const uint thisThreadId = 0u;
			const uint nextThreadId = i;
			const float thisError = g_bestCandidates[thisThreadId].x;
			const float nextError = g_bestCandidates[nextThreadId].x;
			if( nextError < thisError )
				g_bestCandidates[thisThreadId] = float2( nextError, nextThreadId );
		}
	}

	__sharedOnlyBarrier;
#endif

	if( gl_LocalInvocationIndex == uint( g_bestCandidates[0].y ) )
	{
		// This thread is the winner! Save the result
		etc2_th_mode_write( bestModeIsH, bestC0, bestC1, kDistances[distIdx], distIdx );

		const uint2 dstUV = gl_WorkGroupID.xy;
		imageStore( dstError, int2( dstUV ), float4( g_bestCandidates[0].x, 0.0f, 0.0f, 0.0f ) );
	}
}
