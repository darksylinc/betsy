
#include "betsy/EncoderETC1.h"

#include "betsy/CpuImage.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>

namespace betsy
{
	EncoderETC1::EncoderETC1() : m_srcTexture( 0 ), m_compressTargetRes( 0 ), m_dstTexture( 0 ) {}
	//-------------------------------------------------------------------------
	EncoderETC1::~EncoderETC1() { assert( !m_srcTexture && "deinitResources not called!" ); }
	//-------------------------------------------------------------------------
	void EncoderETC1::initResources( const CpuImage &srcImage )
	{
		m_width = srcImage.width;
		m_height = srcImage.height;

		const PixelFormat srcFormat =
			srcImage.format == PFG_RGBA8_UNORM_SRGB ? PFG_RGBA8_UNORM : srcImage.format;

		m_srcTexture = createTexture( TextureParams( m_width, m_height, srcFormat, "m_srcTexture" ) );

		m_compressTargetRes = createTexture( TextureParams( m_width >> 2u, m_height >> 2u, PFG_RG32_UINT,
															"m_compressTargetRes", TextureFlags::Uav ) );

		m_dstTexture =
			createTexture( TextureParams( m_width, m_height, PFG_ETC1_RGB8_UNORM, "m_dstTexture" ) );

		m_compressPso = createComputePsoFromFile( "etc1.glsl", "../Data/" );

		m_stagingTex = createStagingTexture( m_width, m_height, srcImage.format, true );
		memcpy( m_stagingTex.data, srcImage.data, m_stagingTex.sizeBytes );
		uploadStagingTexture( m_stagingTex, m_srcTexture );
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::deinitResources()
	{
		destroyTexture( m_dstTexture );
		m_dstTexture = 0;
		destroyTexture( m_compressTargetRes );
		m_compressTargetRes = 0;
		destroyTexture( m_srcTexture );
		m_srcTexture = 0;

		destroyStagingTexture( m_stagingTex );

		destroyPso( m_compressPso );
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::execute01( EncoderETC1::Etc1Quality quality )
	{
		bindTexture( 0u, m_srcTexture );
		bindUav( 0u, m_compressTargetRes, PFG_RG32_UINT, ResourceAccess::Write );
		bindComputePso( m_compressPso );

		struct Params
		{
			float quality;
			float scanDeltaAbsMin;
			float scanDeltaAbsMax;
			float maxRefinementTrials;
		};

		Params params;
		params.quality = quality;
		if( quality == cHighQuality )
		{
			params.scanDeltaAbsMin = 0;
			params.scanDeltaAbsMax = 4;
		}
		else if( quality == cMediumQuality )
		{
			params.scanDeltaAbsMin = 0;
			params.scanDeltaAbsMax = 1;
		}
		else
		{
			params.scanDeltaAbsMin = 0;
			params.scanDeltaAbsMax = 0;
		}
		// params.scanDeltaAbsMin = 0;
		// params.scanDeltaAbsMax = 0;
		params.maxRefinementTrials = quality == cLowQuality ? 2.0f : 4.0f;

		glUniform4fv( 0, 1u, &params.quality );

		glDispatchCompute( 1u,  //
						   alignToNextMultiple( m_width, 16u ) / 16u,
						   alignToNextMultiple( m_height, 16u ) / 16u );
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::execute02()
	{
		// It's unclear which of these 2 barrier bits GL wants in order for glCopyImageSubData to work
		glMemoryBarrier( GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		glFinish();

		// Copy "8x8" PFG_RG32_UINT -> 32x32 PFG_ETC1_RGB8_UNORM
		glCopyImageSubData( m_compressTargetRes, GL_TEXTURE_2D, 0, 0, 0, 0,  //
							m_dstTexture, GL_TEXTURE_2D, 0, 0, 0, 0,         //
							( GLsizei )( m_width >> 2u ), ( GLsizei )( m_height >> 2u ), 1 );

		StagingTexture stagingTex =
			createStagingTexture( m_width >> 2u, m_height >> 2u, PFG_RG32_UINT, false );
		downloadStagingTexture( m_compressTargetRes, stagingTex );
		glFinish();

		const uint8_t *result = (const uint8_t *)stagingTex.data;
		printf( "DUMP START\n" );
		for( size_t b = 0u; b < 1u; ++b )
		{
			for( size_t i = 0u; i < 8u; ++i )
				printf( "%02X ", result[b * 8u + i] );
			printf( "\n" );
		}
		destroyStagingTexture( stagingTex );
	}
}  // namespace betsy
