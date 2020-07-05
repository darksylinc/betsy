#version 430 core

// RGB and Alpha components of ETC2 RGBA are computed separately.
//
// ETC2 also adds modes T and H (which we compute together) and mode P
//
// This shader will:
//	1. Select the mode with the lowest error
//	2. If not using Alpha, it will output to where P mode was stored (to save VRAM)
//	3. If using Alpha, it will also stitch the alpha and output to another texture
//
// See etc2_rgba_selector.glsl for this shader performing stitching (defines HAS_ALPHA)

// #include "/media/matias/Datos/SyntaxHighlightingMisc.h"

#include "CrossPlatformSettings_piece_all.glsl"
#include "UavCrossPlatform_piece_all.glsl"

layout( local_size_x = 8,  //
		local_size_y = 8,  //
		local_size_z = 1 ) in;

layout( binding = 0 ) uniform sampler2D texErrorEtc1;
layout( binding = 1 ) uniform sampler2D texErrorTh;
layout( binding = 2 ) uniform sampler2D texErrorP;

layout( binding = 3 ) uniform usampler2D srcEtc1;
layout( binding = 4 ) uniform usampler2D srcThModes;
#ifdef HAS_ALPHA
layout( binding = 5 ) uniform usampler2D srcPMode;
layout( binding = 6 ) uniform usampler2D srcAlpha;

layout( rgba32ui, binding = 0 ) uniform restrict writeonly uimage2D dstTexture;
#	define outputValue uint4( etcAlpha.xy, etcRgb.xy )
#else
layout( rg32ui, binding = 0 ) uniform restrict uimage2D inOutPMode;
#	define srcPMode inOutPMode
#	define dstTexture inOutPMode
#	define outputValue uint4( etcRgb.xy, 0u, 0u )
#endif

void main()
{
	const float errorEtc1 = OGRE_Load2D( texErrorEtc1, int2( gl_GlobalInvocationID.xy ), 0 ).x;
	const float errorThModes = OGRE_Load2D( texErrorTh, int2( gl_GlobalInvocationID.xy ), 0 ).x;
	const float errorPMode = OGRE_Load2D( texErrorP, int2( gl_GlobalInvocationID.xy ), 0 ).x;

#ifdef HAS_ALPHA
	const uint2 etcAlpha = OGRE_Load2D( srcAlpha, int2( gl_GlobalInvocationID.xy ), 0 ).xy;
#endif

	if( errorEtc1 <= errorThModes && errorEtc1 <= errorPMode )
	{
		const uint2 etcRgb = OGRE_Load2D( srcEtc1, int2( gl_GlobalInvocationID.xy ), 0 ).xy;
		imageStore( dstTexture, int2( gl_GlobalInvocationID.xy ), outputValue );
	}
	else if( errorThModes < errorPMode )
	{
		const uint2 etcRgb = OGRE_Load2D( srcThModes, int2( gl_GlobalInvocationID.xy ), 0 ).xy;
		imageStore( dstTexture, int2( gl_GlobalInvocationID.xy ), outputValue );
	}
	else
	{
#ifdef HAS_ALPHA
		const uint2 etcRgb = OGRE_Load2D( srcPMode, int2( gl_GlobalInvocationID.xy ), 0 ).xy;
#else
		const uint2 etcRgb = OGRE_imageLoad2D( srcPMode, int2( gl_GlobalInvocationID.xy ) ).xy;
#endif
		imageStore( dstTexture, int2( gl_GlobalInvocationID.xy ), outputValue );
	}
}
