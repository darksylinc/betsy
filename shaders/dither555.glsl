#version 430 core

// #include "/media/matias/Datos/SyntaxHighlightingMisc.h"

#include "CrossPlatformSettings_piece_all.glsl"
#include "UavCrossPlatform_piece_all.glsl"

uniform sampler2D srcTex;

layout( rgba8 ) uniform restrict writeonly image2D dstTexture;

layout( local_size_x = 8,  //
		local_size_y = 8,  //
		local_size_z = 1 ) in;

/// Quantizes 'srcValue' which is originally in 888 (full range),
/// converting it to 555 and then back to 888 (quantized)
float3 quant( float3 srcValue )
{
	srcValue = clamp( srcValue, 0.0f, 255.0f );
	// Convert 888 -> 555
	srcValue = floor( srcValue * 31.0f / 255.0f + 0.5f );
	// Convert 555 -> 888 back
	srcValue = floor( srcValue * 8.25f );

	return srcValue;
}

void main()
{
	float3 ep1[4] = { float3( 0, 0, 0 ), float3( 0, 0, 0 ), float3( 0, 0, 0 ), float3( 0, 0, 0 ) };
	float3 ep2[4] = { float3( 0, 0, 0 ), float3( 0, 0, 0 ), float3( 0, 0, 0 ), float3( 0, 0, 0 ) };

	const uint2 pixelsToLoadBase = gl_GlobalInvocationID.xy << 2u;
	float3 srcPixel0 = OGRE_Load2D( srcTex, int2( pixelsToLoadBase ), 0 ).xyz * 255.0f;
	bool bAllColoursEqual = true;

	for( uint y = 0u; y < 4u; ++y )
	{
		float3 srcPixel, dithPixel;
		int2 iUV;

		iUV = int2( pixelsToLoadBase + uint2( 0u, y ) );
		srcPixel = OGRE_Load2D( srcTex, iUV, 0 ).xyz * 255.0f;
		bAllColoursEqual = bAllColoursEqual && srcPixel0 == srcPixel;
		dithPixel = quant( srcPixel + trunc( ( 3 * ep2[1] + 5 * ep2[0] ) * ( 1.0f / 16.0f ) ) );
		ep1[0] = srcPixel - dithPixel;
		imageStore( dstTexture, iUV, float4( dithPixel * ( 1.0f / 255.0f ), 1.0f ) );

		iUV = int2( pixelsToLoadBase + uint2( 1u, y ) );
		srcPixel = OGRE_Load2D( srcTex, iUV, 0 ).xyz * 255.0f;
		bAllColoursEqual = bAllColoursEqual && srcPixel0 == srcPixel;
		dithPixel = quant(
			srcPixel + trunc( ( 7 * ep1[0] + 3 * ep2[2] + 5 * ep2[1] + ep2[0] ) * ( 1.0f / 16.0f ) ) );
		ep1[1] = srcPixel - dithPixel;
		imageStore( dstTexture, iUV, float4( dithPixel * ( 1.0f / 255.0f ), 1.0f ) );

		iUV = int2( pixelsToLoadBase + uint2( 2u, y ) );
		srcPixel = OGRE_Load2D( srcTex, iUV, 0 ).xyz * 255.0f;
		bAllColoursEqual = bAllColoursEqual && srcPixel0 == srcPixel;
		dithPixel = quant(
			srcPixel + trunc( ( 7 * ep1[1] + 3 * ep2[3] + 5 * ep2[2] + ep2[1] ) * ( 1.0f / 16.0f ) ) );
		ep1[2] = srcPixel - dithPixel;
		imageStore( dstTexture, iUV, float4( dithPixel * ( 1.0f / 255.0f ), 1.0f ) );

		iUV = int2( pixelsToLoadBase + uint2( 3u, y ) );
		srcPixel = OGRE_Load2D( srcTex, iUV, 0 ).xyz * 255.0f;
		bAllColoursEqual = bAllColoursEqual && srcPixel0 == srcPixel;
		dithPixel = quant( srcPixel + trunc( ( 7 * ep1[2] + 5 * ep2[3] + ep2[2] ) * ( 1.0f / 16.0f ) ) );
		ep1[3] = srcPixel - dithPixel;
		imageStore( dstTexture, iUV, float4( dithPixel * ( 1.0f / 255.0f ), 1.0f ) );

		// swap( ep1, ep2 )
		for( uint i = 0u; i < 4u; ++i )
		{
			float3 tmp = ep1[i];
			ep1[i] = ep2[i];
			ep2[i] = tmp;
		}
	}

	if( bAllColoursEqual )
	{
		// Oops. All colours were equal. We shouldn't have applied dither.
		// Overwrite our changes with a raw copy.
		for( uint i = 0u; i < 16u; ++i )
		{
			const int2 iUV = int2( pixelsToLoadBase + uint2( i & 0x03u, i >> 2u ) );
			const float3 srcPixels0 = OGRE_Load2D( srcTex, iUV, 0 ).xyz;
			imageStore( dstTexture, iUV, float4( srcPixels0, 1.0f ) );
		}
	}
}
