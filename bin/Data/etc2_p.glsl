#version 430 core

// Planar mode of ETC2

// #include "/media/matias/Datos/SyntaxHighlightingMisc.h"

#include "CrossPlatformSettings_piece_all.glsl"
#include "UavCrossPlatform_piece_all.glsl"

#define FLT_MAX 340282346638528859811704183484516925440.0f

uniform sampler2D srcTex;

layout( rg32ui, binding = 0 ) uniform restrict writeonly uimage2D dstTexture;
layout( r32f, binding = 1 ) uniform restrict writeonly image2D dstError;

layout( local_size_x = 8,  //
		local_size_y = 8,  //
		local_size_z = 1 ) in;

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
	const uint2 pixelsToLoadBase = gl_GlobalInvocationID.xy << 2u;
	uint2 pixelsToLoad = pixelsToLoadBase;
	// Note we are NOT transposing this time!
	pixelsToLoad.x += idx & 0x03u;  //+= threadId % 4
	pixelsToLoad.y += idx >> 2u;    //+= threadId / 4
	const float3 srcPixels0 = OGRE_Load2D( srcTex, int2( pixelsToLoad ), 0 ).xyz;
	return srcPixels0;
}

float3 getSrcPixel( float x, float y )
{
	const uint2 pixelsToLoadBase = gl_GlobalInvocationID.yz << 2u;
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

float3 quant676( float3 rgbValue )
{
	rgbValue = floor( rgbValue * float2( 63.0f, 127.0f ).xyx + 0.5f );  // Convert to 676
	rgbValue = floor( rgbValue * float2( 4.0625f, 2.0157f ).xyx );      // Convert to 888
	return rgbValue * ( 1.0f / 255.0f );
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

	const uint2 dstUV = gl_GlobalInvocationID.xy;
	imageStore( dstTexture, int2( dstUV ), uint4( outputBytes.xy, 0u, 0u ) );
}

void main()
{
	// TODO: Scan a broader range to avoid artifacts when the
	// points O, H or V are exceptions

	// O is at (0,0)
	// H is at (4,0)
	// V is at (0,4)
	// So, H and V are outside the block.
	// We extrapolate the values from (0,3) and (3,0).

	const float3 rgb0 = getSrcPixel( 0u );

	const float3 cO = rgbFpTo676( rgb0 );
	const float3 cH = rgbFpTo676( saturate( lerp( rgb0, getSrcPixel( 3u ), 4.0f / 3.0f ) ) );
	const float3 cV = rgbFpTo676( saturate( lerp( rgb0, getSrcPixel( 12u ), 4.0f / 3.0f ) ) );

	// Use a Simple Linear Regression to find the best curve that goes through all pixels
	// in the row

	etc2_planar_mode_write( cO, cH, cV );

	const float err = calcErrorPMode( cO, cH, cV );
	const uint2 dstUV = gl_GlobalInvocationID.xy;
	imageStore( dstError, int2( dstUV ), float4( err, 0.0f, 0.0f, 0.0f ) );
}
