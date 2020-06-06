
#include "betsy/EncoderETC1.h"

#include "betsy/CpuImage.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>

namespace betsy
{
	EncoderETC1::EncoderETC1() :
		m_srcTexture( 0 ),
		m_compressTargetRes( 0 ),
		m_eacTargetRes( 0 ),
		m_stitchedTarget( 0 ),
		m_dstTexture( 0 )
	{
	}
	//-------------------------------------------------------------------------
	EncoderETC1::~EncoderETC1() { assert( !m_srcTexture && "deinitResources not called!" ); }
	//-------------------------------------------------------------------------
	void EncoderETC1::initResources( const CpuImage &srcImage, bool bCompressAlpha )
	{
		m_width = srcImage.width;
		m_height = srcImage.height;

		const PixelFormat srcFormat =
			srcImage.format == PFG_RGBA8_UNORM_SRGB ? PFG_RGBA8_UNORM : srcImage.format;

		m_srcTexture = createTexture( TextureParams( m_width, m_height, srcFormat, "m_srcTexture" ) );

		m_compressTargetRes = createTexture( TextureParams( m_width >> 2u, m_height >> 2u, PFG_RG32_UINT,
															"m_compressTargetRes", TextureFlags::Uav ) );

		m_dstTexture =
			createTexture( TextureParams( m_width, m_height, PFG_EAC_R11_UNORM, "m_dstTexture" ) );

		m_compressPso = createComputePsoFromFile( "etc1.glsl", "../Data/" );

		if( bCompressAlpha )
		{
			m_eacTargetRes = createTexture( TextureParams( m_width >> 2u, m_height >> 2u, PFG_RG32_UINT,
														   "m_eacTargetRes", TextureFlags::Uav ) );
			m_stitchedTarget =
				createTexture( TextureParams( m_width >> 2u, m_height >> 2u, PFG_RGBA32_UINT,
											  "m_stitchedTarget", TextureFlags::Uav ) );
			m_eacPso = createComputePsoFromFile( "eac.glsl", "../Data/" );
			m_stitchPso = createComputePsoFromFile( "etc2_rgba_stitch.glsl", "../Data/" );
		}

		StagingTexture stagingTex = createStagingTexture( m_width, m_height, srcImage.format, true );
		memcpy( stagingTex.data, srcImage.data, stagingTex.sizeBytes );
		uploadStagingTexture( stagingTex, m_srcTexture );
		destroyStagingTexture( stagingTex );
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::deinitResources()
	{
		destroyTexture( m_dstTexture );
		m_dstTexture = 0;
		destroyTexture( m_compressTargetRes );
		m_compressTargetRes = 0;
		if( m_eacTargetRes )
		{
			destroyTexture( m_stitchedTarget );
			m_stitchedTarget = 0;

			destroyTexture( m_eacTargetRes );
			m_eacTargetRes = 0;
		}
		destroyTexture( m_srcTexture );
		m_srcTexture = 0;

		if( m_downloadStaging.bufferName )
			destroyStagingTexture( m_downloadStaging );

		if( m_eacPso.computePso )
		{
			destroyPso( m_stitchPso );
			destroyPso( m_eacPso );
		}
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

		if( m_eacTargetRes )
		{
			// Compress Alpha too (using EAC compressor)
			bindUav( 0u, m_eacTargetRes, PFG_RG32_UINT, ResourceAccess::Write );
			bindComputePso( m_eacPso );
			glDispatchCompute( alignToNextMultiple( m_width, 4u ) / 4u,
							   alignToNextMultiple( m_height, 4u ) / 4u, 1u );
		}
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::execute02()
	{
		if( !m_eacTargetRes )
		{
			// This step is only relevant when doing ETC2_RGBA
			return;
		}

		glMemoryBarrier( GL_TEXTURE_FETCH_BARRIER_BIT );
		bindTexture( 0u, m_compressTargetRes );
		bindTexture( 1u, m_eacTargetRes );
		bindUav( 0u, m_stitchedTarget, PFG_RGBA32_UINT, ResourceAccess::Write );
		bindComputePso( m_stitchPso );
		glDispatchCompute( alignToNextMultiple( m_width, 4u ) / 4u,
						   alignToNextMultiple( m_height, 4u ) / 4u, 1u );
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::execute03()
	{
		// It's unclear which of these 2 barrier bits GL wants in order for glCopyImageSubData to work
		glMemoryBarrier( GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

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
	//-------------------------------------------------------------------------
	void EncoderETC1::startDownload()
	{
		glMemoryBarrier( GL_PIXEL_BUFFER_BARRIER_BIT );

		if( m_downloadStaging.bufferName )
			destroyStagingTexture( m_downloadStaging );
		m_downloadStaging = createStagingTexture(
			m_width >> 2u, m_height >> 2u, m_eacTargetRes ? PFG_RGBA32_UINT : PFG_RG32_UINT, false );
		downloadStagingTexture( m_eacTargetRes ? m_stitchedTarget : m_compressTargetRes,
								m_downloadStaging );
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::downloadTo( CpuImage &outImage )
	{
		glFinish();
		outImage.width = m_width;
		outImage.height = m_height;
		outImage.format = m_eacTargetRes ? PFG_ETC2_RGBA8_UNORM : PFG_ETC1_RGB8_UNORM;
		outImage.data = reinterpret_cast<uint8_t *>( m_downloadStaging.data );
	}
}  // namespace betsy
