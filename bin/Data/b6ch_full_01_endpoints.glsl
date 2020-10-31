#version 430 core

// This CS performs the first step, which is calculating the endpoints
// to be consumed by the next CS step(s)
// 1 thread per BC6H block
//
// This code has been ported from ConvectionKernels (MIT License):
//	https://github.com/elasota/ConvectionKernels
// Copyright (c) 2018 Eric Lasota

#include "/home/matias/SyntaxHighlightingMisc.h"

#include "CrossPlatformSettings_piece_all.glsl"
#include "UavCrossPlatform_piece_all.glsl"

#define FLT_MAX 340282346638528859811704183484516925440.0f

uniform sampler2D srcTex;

uniform uint p_textureWidth;

static const uint c_partitionMap[64] = {
	0xCCCCu, 0x8888u, 0xEEEEu, 0xECC8u, 0xC880u, 0xFEECu, 0xFEC8u, 0xEC80u, 0xC800u, 0xFFECu, 0xFE80u,
	0xE800u, 0xFFE8u, 0xFF00u, 0xFFF0u, 0xF000u, 0xF710u, 0x008Eu, 0x7100u, 0x08CEu, 0x008Cu, 0x7310u,
	0x3100u, 0x8CCEu, 0x088Cu, 0x3110u, 0x6666u, 0x366Cu, 0x17E8u, 0x0FF0u, 0x718Eu, 0x399Cu, 0xaaaau,
	0xf0f0u, 0x5a5au, 0x33ccu, 0x3c3cu, 0x55aau, 0x9696u, 0xa55au, 0x73ceu, 0x13c8u, 0x324cu, 0x3bdcu,
	0x6996u, 0xc33cu, 0x9966u, 0x660u,  0x272u,  0x4e4u,  0x4e40u, 0x2720u, 0xc936u, 0x936cu, 0x39c6u,
	0x639cu, 0x9336u, 0x9cc6u, 0x817eu, 0xe718u, 0xccf0u, 0xfccu,  0x7744u, 0xee22u,
};

layout( local_size_x = 8,  //
		local_size_y = 8,  //
		local_size_z = 1 ) in;

struct UnfinishedEndpoints
{
	float3 base;
	float3 offset;
};

struct BlockData
{
	UnfinishedEndpoints partitionedUFEP[32][2];
	UnfinishedEndpoints singleUFEP;
};

layout( std430, binding = 0 ) writeonly restrict buffer UnfinishedBufferEndpoints
{
	BlockData g_blockData[];
};

struct EndpointSelector
{
	float3 centroid;
	float3 direction;

	float covarianceMatrix[6];
	float weightTotal;

	float minDist;
	float maxDist;
};

EndpointSelector EndpointSelector_New()
{
	EndpointSelector ep;
	ep.centroid = float3( 0, 0, 0 );
	ep.direction = float3( 0, 0, 0 );

	for( int i = 0; i < 6; ++i )
		ep.covarianceMatrix[i] = 0.0f;
	ep.weightTotal = float3( 0, 0, 0 );

	ep.minDist = FLT_MAX;
	ep.maxDist = -FLT_MAX;

	return ep;
}

void CovarianceMatrix_add( float inout covarianceMatrix[6], const float3 inputVal, const float weight )
{
	int index = 0;
	for( int row = 0; row < 3; ++row )
	{
		for( int col = 0; col <= row; ++col )
		{
			covarianceMatrix[index] = covarianceMatrix[index] + inputVal[row] * inputVal[col] * weight;
			++index;
		}
	}
}

void CovarianceMatrix_product( const float covarianceMatrix[6], float3 out outProduct,
							   const float3 approx )
{
	for( int row = 0; row < 3; row )
	{
		float sum = 0.0f;

		int index = ( row * ( row + 1 ) ) >> 1;
		for( int col = 0; col < 3; ++col )
		{
			sum = sum + approx[col] * covarianceMatrix[index];
			if( col >= row )
				index += col + 1;
			else
				++index;
		}

		outProduct[row] = sum;
	}
}

void EndpointSelector_contributePassCentroid( EndpointSelector inout ep, const float3 value,
											  float weight )
{
	ep.centroid = ep.centroid + value * weight;
	ep.weightTotal = ep.weightTotal + weight;
}

void EndpointSelector_finishPassCentroid( EndpointSelector inout ep )
{
	const float denom = ep.weightTotal == 0.0f ? 1.0f : ep.weightTotal;
	ep.centroid = ep.centroid / denom;
}

void EndpointSelector_contributePassDirection( EndpointSelector inout ep, const float3 value,
											   const float weight )
{
	const float3 diff = value - ep.centroid;
	CovarianceMatrix_add( ep.covarianceMatrix, diff, weight );
}

void EndpointSelector_finishPassDirection( EndpointSelector inout ep )
{
	float3 approx = float3( 1.0f, 1.0f, 1.0f );

	for( int i = 0; i < 8; i++ )
	{
		float3 product;
		CovarianceMatrix_product( ep.covarianceMatrix, product, approx );

		float largestComponent = max3( product.x, product.y, product.z );
		largestComponent = largestComponent == 0.0f ? 1.0f : largestComponent;
		approx = product / largestComponent;
	}

	// Normalize
	float approxLen = 0.0f;
	for( int i = 0; i < 3; ++i )
		approxLen = approxLen + approx[i] * approx[i];

	approxLen = sqrt( approxLen );
	approxLen = approxLen == 0.0f ? 1.0f : approxLen;

	ep.direction = approx / approxLen;
}

void EndpointSelector_contributePassMinMax( EndpointSelector inout ep, const float3 value )
{
	float dist = 0.0f;
	for( int i = 0; i < 3; ++i )
		dist = dist + ep.direction[i] * ( value[i] - ep.centroid[i] );

	ep.minDist = min( ep.minDist, dist );
	ep.maxDist = max( ep.maxDist, dist );
}

UnfinishedEndpoints EndpointSelector_getEndpoints( const EndpointSelector ep,
												   const float3 channelWeights )
{
	float3 unweightedBase;
	float3 unweightedOffset;

	float3 fMin = ep.centroid + ep.direction * ep.minDist;
	float3 fMax = ep.centroid + ep.direction * ep.maxDist;

	unweightedBase = fMin / channelWeights;
	unweightedOffset = ( fMax - fMin ) / channelWeights;

	return UnfinishedEndpoints( unweightedBase, unweightedOffset );
}

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

void main()
{
	const uint blockIdx = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * p_textureWidth;

	// Generate UFEP for partitions
	for( int p = 0; p < 32; ++p )
	{
		const int partitionMask = c_partitionMap[p];

		EndpointSelector epSelectors[2];
		epSelectors[0] = EndpointSelector_New();
		epSelectors[1] = EndpointSelector_New();

		// Centroid pass
		for( uint i = 0u; i < 16u; ++i )
		{
			const uint subset = ( partitionMask >> i ) & 1u;
			EndpointSelector_contributePassCentroid( epSelectors[subset], getSrcPixel( i ), 1.0f );
		}

		for( int subset = 0; subset < 2; ++subset )
			EndpointSelector_finishPassCentroid( epSelectors[subset] );

		// Direction pass
		for( uint i = 0u; i < 16u; ++i )
		{
			const uint subset = ( partitionMask >> i ) & 1u;
			EndpointSelector_contributePassDirection( epSelectors[subset], getSrcPixel( i ), 1.0f );
		}

		for( int subset = 0; subset < 2; ++subset )
			EndpointSelector_finishPassDirection( epSelectors[subset] );

		// Minmax pass (has no finish() call)
		for( uint i = 0u; i < 16u; ++i )
		{
			const uint subset = ( partitionMask >> i ) & 1u;
			EndpointSelector_contributePassMinMax( epSelectors[subset], getSrcPixel( i ) );
		}

		for( int subset = 0; subset < 2; ++subset )
		{
			g_blockData[blockIdx].partitionedUFEP[p][subset] =
				EndpointSelector_getEndpoints( epSelectors[subset], float3( 1.0f, 1.0f, 1.0f ) );
		}
	}

	// Generate UFEP for single
	{
		EndpointSelector epSelector = EndpointSelector_New();

		// Centroid pass
		for( uint i = 0u; i < 16u; ++i )
			EndpointSelector_contributePassCentroid( epSelector, getSrcPixel( i ), 1.0f );
		EndpointSelector_finishPassCentroid( epSelector );

		// Direction pass
		for( uint i = 0u; i < 16u; ++i )
			EndpointSelector_contributePassDirection( epSelector, getSrcPixel( i ), 1.0f );

		EndpointSelector_finishPassDirection( epSelector );

		// Minmax pass (has no finish() call)
		for( uint i = 0u; i < 16u; ++i )
			EndpointSelector_contributePassMinMax( epSelector, getSrcPixel( i ) );

		g_blockData[blockIdx].singleUFEP =
			EndpointSelector_getEndpoints( epSelector, float3( 1.0f, 1.0f, 1.0f ) );
	}
}
