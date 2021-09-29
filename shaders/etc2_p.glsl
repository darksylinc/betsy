#version 430 core

// Planar mode of ETC2

// #include "/media/matias/Datos/SyntaxHighlightingMisc.h"

#include "CrossPlatformSettings_piece_all.glsl"
#include "UavCrossPlatform_piece_all.glsl"

#define FLT_MAX 340282346638528859811704183484516925440.0f

uniform sampler2D srcTex;

layout( rg32ui, binding = 0 ) uniform restrict writeonly uimage2D dstTexture;
layout( r32f, binding = 1 ) uniform restrict writeonly image2D dstError;

shared float2 g_threadBestCandidates[4u][4u * 4u];

#define g_bestCandidates g_threadBestCandidates[gl_LocalInvocationID.z]

layout( local_size_x = 4,  //
        local_size_y = 4,  //
        local_size_z = 4 ) in;

/*
kPmodeEncoderRG table generated with:
    static const int kSigned3bit[8] = { 0, 1, 2, 3, -4, -3, -2, -1 };
    #define BITS( byteval, lowbit, highbit ) \
        ( ( ( byteval ) >> ( lowbit ) ) & ( ( 1 << ( ( highbit ) - ( lowbit ) + 1 ) ) - 1 ) )
    #define BIT( byteval, bit ) ( ( ( byteval ) >> ( bit ) ) & 0x1 )
    int main()
    {
        // kPmodeEncoderRG
        for( int GO = 0; GO < 128; GO += 127u )
        {
            for( int RO = 0; RO < 64; ++RO )
            {
                // RO_6 [2..5]
                int R = BITS( RO, 2, 5 );
                // RO_6 [0..1] + GO_7[6]
                int dR = ( BITS( RO, 0, 1 ) << 1 ) | BIT( GO, 6 );
                if( !( ( R + kSigned3bit[dR] >= 0 ) && ( R + kSigned3bit[dR] <= 31 ) ) )
                    R |= 1 << 4;
                assert( ( ( R + kSigned3bit[dR] >= 0 ) && ( R + kSigned3bit[dR] <= 31 ) ) );
                printf( "%i, ", ( R << 3 ) | dR );
            }
        }
        printf( "\n" );
        return 0;
    }
*/
const float kPmodeEncoderRG[128] = {
	0,   2,   132, 134, 8,   10,  140, 142, 16,  18,  148, 22,  24,  26,  156, 30,  32,  34,  36,
	38,  40,  42,  44,  46,  48,  50,  52,  54,  56,  58,  60,  62,  64,  66,  68,  70,  72,  74,
	76,  78,  80,  82,  84,  86,  88,  90,  92,  94,  96,  98,  100, 102, 104, 106, 108, 110, 112,
	114, 116, 118, 120, 122, 124, 126, 1,   3,   133, 135, 9,   11,  141, 15,  17,  19,  149, 23,
	25,  27,  29,  31,  33,  35,  37,  39,  41,  43,  45,  47,  49,  51,  53,  55,  57,  59,  61,
	63,  65,  67,  69,  71,  73,  75,  77,  79,  81,  83,  85,  87,  89,  91,  93,  95,  97,  99,
	101, 103, 105, 107, 109, 111, 113, 115, 117, 119, 121, 123, 125, 127
};

// kPmodeEncoderGB is exactly the same
#define kPmodeEncoderGB kPmodeEncoderRG

/*
kPmodeEncoderB table generated with:
    static const int kSigned3bit[8] = { 0, 1, 2, 3, -4, -3, -2, -1 };
    #define BITS( byteval, lowbit, highbit ) \
        ( ( ( byteval ) >> ( lowbit ) ) & ( ( 1 << ( ( highbit ) - ( lowbit ) + 1 ) ) - 1 ) )
    int main()
    {
        for( int BO = 0; BO < 32; ++BO )
        {
            if( !( BO & 0x01 ) )
            {
                // BO_6[3..4]
                int B = BITS( BO, 3, 4 );
                // BO_6[1..2]
                int dB = BITS( BO, 1, 2 );
                // B + dB must be outside the range.
                for( int Bx = 0; Bx < 8; Bx++ )
                {
                    for( int dBx = 0; dBx < 2; dBx++ )
                    {
                        int Btry = B | ( Bx << 2 );
                        int dBtry = dB | ( dBx << 2 );
                        if( ( Btry + kSigned3bit[dBtry] ) < 0 || ( Btry + kSigned3bit[dBtry] > 31 ) )
                        {
                            B = Btry;
                            dB = dBtry;
                            break;
                        }
                    }
                }
                assert( !( ( B + kSigned3bit[dB] >= 0 ) && ( B + kSigned3bit[dB] <= 31 ) ) );
                printf( "%i, ", ( B << 3 ) | dB );
            }
        }
        printf( "\n" );
        return 0;
    }
*/
const float kPmodeEncoderB[16] = { 4, 5, 6, 7, 12, 13, 14, 235, 20, 21, 242, 243, 28, 249, 250, 251 };

float3 getSrcPixel( uint idx )
{
	const uint2 pixelsToLoadBase =
	    ( ( gl_WorkGroupID.xy << 1u ) +
	      uint2( gl_LocalInvocationID.z & 0x01u, gl_LocalInvocationID.z >> 1u ) )
	    << 2u;
	uint2 pixelsToLoad = pixelsToLoadBase;
	// Note we are NOT transposing this time!
	pixelsToLoad.x += idx & 0x03u;  //+= threadId % 4
	pixelsToLoad.y += idx >> 2u;    //+= threadId / 4
	const float3 srcPixels0 = OGRE_Load2D( srcTex, int2( pixelsToLoad ), 0 ).xyz;
	return srcPixels0;
}

float3 getSrcPixel( float x, float y )
{
	const uint2 pixelsToLoadBase =
	    ( ( gl_WorkGroupID.xy << 1u ) +
	      uint2( gl_LocalInvocationID.z & 0x01u, gl_LocalInvocationID.z >> 1u ) )
	    << 2u;
	uint2 pixelsToLoad = pixelsToLoadBase + uint2( float( x ), float( y ) );
	const float3 srcPixels0 = OGRE_Load2D( srcTex, int2( pixelsToLoad ), 0 ).xyz;
	return srcPixels0;
}

/// Converts rgbValue in fp range [0; 1] to integer range [0; 64) in RB and [0; 128) in G channels
float3 rgbFpTo676( const float3 rgbFp )
{
	return floor( rgbFp * float2( 63.0f, 127.0f ).xyx + 0.5f );  // Convert to 676
}

float3 rgb676To888( float3 rgbValue )
{
	return floor( rgbValue * float2( 4.0625f, 2.0157f ).xyx );  // Convert to 888
}

float calcError( const float3 colour0, const float3 colour1 )
{
	float3 diff = colour0.xyz - colour1.xyz;
	return dot( diff, diff );
}

float calcErrorPMode( float3 cO, float3 cH, float3 cV )
{
	float err = 0.0f;

	cO = rgb676To888( cO );
	cH = rgb676To888( cH );
	cV = rgb676To888( cV );

	for( uint i = 0u; i < 16u; ++i )
	{
		const float x = float( i & 0x03u );  // i % 4
		const float y = float( i >> 2u );    // i / 4

		// Bilinear interpolation using integer arithmetic (i.e. what the HW does)
		float3 rgbn = floor( ( 4.0f * cO + x * ( cH - cO ) + y * ( cV - cO ) + 2.0f ) * 0.25f );
		rgbn = clamp( rgbn, 0.0f, 255.0f );

		err += calcError( getSrcPixel( x, y ) * 255.0f, rgbn );
	}

	return err;
}

/**
@param cO
    xz in range [0; 64) y in range [0; 128)
@param cH
    xz in range [0; 64) y in range [0; 128)
@param cV
    xz in range [0; 64) y in range [0; 128)
@param srcPixelsBlock
*/
void etc2_planar_mode_write( const float3 cO, const float3 cH, const float3 cV )
{
	float4 bytes;

	// cO.r bits [0; 6)
	// cO.g bits [6; 7)
	bytes.x = kPmodeEncoderRG[uint( cO.x + ( cO.y >= 64.0f ? 64.0f : 0.0f ) )];
	// cO.g bits [0; 6)
	// cO.b bits [5; 6)
	bytes.y = kPmodeEncoderGB[uint( mod( cO.y, 64.0f ) + ( cO.z >= 32.0f ? 64.0f : 0.0f ) )];
	// cO.b bits [1; 5)
	bytes.z = kPmodeEncoderB[uint( mod( floor( cO.z * 0.5f ), 16.0f ) )];
	bytes.w = mod( cO.z, 2.0f ) * 128.0f + floor( cH.x * 0.5f ) * 4.0f + 2.0f + mod( cH.x, 2.0f );

	uint2 outputBytes;
	outputBytes.x = packUnorm4x8( bytes * ( 1.0f / 255.0f ) );

	bytes.x = cH.y * 2.0f + floor( cH.z * 0.03125f );
	bytes.y = mod( cH.z, 32.0f ) * 8.0f + floor( cV.x * 0.125f );
	bytes.z = mod( cV.x, 8.0f ) * 32.0f + floor( cV.y * 0.25f );
	bytes.w = mod( cV.y, 4.0f ) * 64.0f + cV.z;

	outputBytes.y = packUnorm4x8( bytes * ( 1.0f / 255.0f ) );

	const uint2 dstUV = ( gl_WorkGroupID.xy << 1u ) +
	                    uint2( gl_LocalInvocationID.z & 0x01u, gl_LocalInvocationID.z >> 1u );
	imageStore( dstTexture, int2( dstUV ), uint4( outputBytes.xy, 0u, 0u ) );
}

/** Uses a Simple Linear Regression to find the best curve that fits all 4 samples in a row
@param bVertical
@param yOffset
    When bVertical = true, yOffset becomes xOffset
@param startIdx
@param endIdx
@param minEndpoint [out]
    Value for cO
@param maxEndpoint [out]
    Value for either cH (bVertical = false) or cV (bVertical = true)
*/
void getCoeffSLR( const bool bVertical, const float yOffset, const float startIdx, const float endIdx,
                  out float3 minEndpoint, out float3 maxEndpoint )
{
	float3 srcCol[4];
	float3 avgCol = float3( 0.0f, 0.0f, 0.0f );
	for( float i = 0.0f; i < 4.0f; ++i )
	{
		const float idx = clamp( i, startIdx, endIdx );
		float2 uv = float2( idx, yOffset );
		uv = bVertical ? uv.yx : uv.xy;
		srcCol[uint( i )] = getSrcPixel( uv.x, uv.y );
		avgCol += srcCol[uint( i )];
	}

	avgCol *= 0.25f;

	float3 abscissaNum = float3( 0.0f, 0.0f, 0.0f );
	float3 abscissaDen = float3( 0.0f, 0.0f, 0.0f );

	for( float i = 0.0f; i < 4.0f; ++i )
	{
		abscissaNum += ( i - 1.5f ) * ( srcCol[uint( i )] - avgCol );
		abscissaDen += ( i - 1.5f ) * ( i - 1.5f );
	}

	const float3 abscissa = abscissaNum / abscissaDen;
	const float3 ordinate = avgCol - abscissa * 1.5f;

	minEndpoint = ordinate;
	maxEndpoint = ordinate + abscissa * 3.0f;
}

/** Calculates O, H and V coefficients using SLR (Simple Linear Regression)
@param oIdx
    Where to start sampling O from. Must be in range [0; 2)
@param hIdx
    Where to end sampling H at. Must be in range [2; 4)
@param vIdx
    Where to end sampling V at. Must be in range [2; 4)
@param cO [out]
    xz in range [0; 64) y in range [0; 128)
@param cH [out]
    xz in range [0; 64) y in range [0; 128)
@param cV [out]
    xz in range [0; 64) y in range [0; 128)
*/
void getCoeffs( const uint2 oIdx, const uint hIdx, const uint vIdx, out float3 cO, out float3 cH,
                out float3 cV )
{
	float3 cOx, cOy;
	getCoeffSLR( false, oIdx.y, oIdx.x, hIdx, cOx, cH );
	getCoeffSLR( true, oIdx.x, oIdx.y, vIdx, cOy, cV );

	// We have two cO produced by each SLR. Just average them together and cross fingers
	cO = ( cOx + cOy ) * 0.5f;

	cH = rgbFpTo676( saturate( lerp( cO, cH, 4.0f / 3.0f ) ) );
	cV = rgbFpTo676( saturate( lerp( cO, cV, 4.0f / 3.0f ) ) );
	cO = rgbFpTo676( saturate( cO ) );
}

void main()
{
	// P mode is for interpolating slowly varying gradients.
	//
	// O is at (0,0)
	// H is at (4,0)
	// V is at (0,4)
	// So, H and V are outside the block.
	// We extrapolate the values from (0,3) and (3,0).
	//
	// Some gradients are not possible due to clamping, others due to us having
	// only 3 endpoints instead of 4.
	// For those gradients, modes other than P may be more suitable.
	//
	// There is an edge case though: What if we have a smooth gradient,
	// but O, H and/or V happens to be an outlier? (e.g. a gradient with a bad pixel or two)
	//
	// To test these edge case we try several cases:
	//	O starts at:
	//		1. (0,0)
	//		2. (0,1)
	//		3. (1,0)
	//		4. (1,1)
	// However the same can happen to H and V, thus we try (before extrapolation):
	//		1. H at (3, N) -> original case
	//		2. H at (2, N) -> alternate case
	//		3. V at (N, 3) -> original case
	//		4. V at (N, 2) -> alternate case
	//
	// 'N' depends on the value of O we're trying
	//
	// Therefore we will try 4x4 = 16 cases in case our block's border(s) are outliers
	//
	// One of these tries will only use O(1,1) H(2,1) V(1,2) which means a gradient
	// in a 2x2 rectangle and we ignore the outside borders. This is the most distorted
	// one and surely another ETC2 mode will handle this case better, but we evaluate it anyway
	//
	// These 16 cases are handled by 1 thread each.

	const uint2 oIdxStart = uint2( gl_LocalInvocationID.x & 0x1u, gl_LocalInvocationID.x >> 1u );
	const uint hIdxEnd = 3u - ( gl_LocalInvocationID.y & 0x1u );
	const uint vIdxEnd = 3u - ( gl_LocalInvocationID.y >> 1u );

	float3 cO, cH, cV;
	getCoeffs( oIdxStart, hIdxEnd, vIdxEnd, cO, cH, cV );

	const float err = calcErrorPMode( cO, cH, cV );
	const uint thisThreadLocalIdx = gl_LocalInvocationID.y * 4u + gl_LocalInvocationID.x;
	g_bestCandidates[thisThreadLocalIdx] = float2( err, thisThreadLocalIdx );

	__sharedOnlyBarrier;

	// Parallel reduction to find the thread with the best solution
	const uint iterations = 4u;  // 16 threads = 16 reductions = 2⁴ -> 4 iterations
	for( uint i = 0u; i < iterations; ++i )
	{
		const uint mask = ( 1u << ( i + 1u ) ) - 1u;
		const uint idx = 1u << i;
		if( ( thisThreadLocalIdx & mask ) == 0u )
		{
			const uint nextThreadLocalIdx = thisThreadLocalIdx + idx;
			const float thisError = g_bestCandidates[thisThreadLocalIdx].x;
			const float nextError = g_bestCandidates[nextThreadLocalIdx].x;
			if( nextError < thisError )
			{
				g_bestCandidates[thisThreadLocalIdx] =
				    float2( nextError, g_bestCandidates[nextThreadLocalIdx].y );
			}
		}
		__sharedOnlyBarrier;
	}

	if( thisThreadLocalIdx == uint( g_bestCandidates[0u].y ) )
	{
		// This thread is the winner! Save the result
		const uint2 dstUV = ( gl_WorkGroupID.xy << 1u ) +
		                    uint2( gl_LocalInvocationID.z & 0x01u, gl_LocalInvocationID.z >> 1u );
		imageStore( dstError, int2( dstUV ), float4( err, 0.0f, 0.0f, 0.0f ) );

		etc2_planar_mode_write( cO, cH, cV );
	}
}
