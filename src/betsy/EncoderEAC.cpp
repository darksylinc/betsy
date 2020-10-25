
#include "betsy/EncoderEAC.h"

#include "betsy/CpuImage.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>

namespace betsy
{
	EncoderEAC::EncoderEAC() : m_width( 0 ), m_height( 0 ), m_srcTexture( 0 ), m_stitchedTarget( 0 )
	{
		memset( m_eacTargetRes, 0, sizeof( m_eacTargetRes ) );
	}
	//-------------------------------------------------------------------------
	EncoderEAC::~EncoderEAC() { assert( !m_srcTexture && "deinitResources not called!" ); }
	//-------------------------------------------------------------------------
	void EncoderEAC::initResources( const CpuImage &srcImage, const bool rg11 )
	{
		m_width = srcImage.width;
		m_height = srcImage.height;

		const PixelFormat srcFormat =
			srcImage.format == PFG_RGBA8_UNORM_SRGB ? PFG_RGBA8_UNORM : srcImage.format;

		m_srcTexture = createTexture( TextureParams( m_width, m_height, srcFormat, "m_srcTexture" ) );

		m_eacTargetRes[0] = createTexture( TextureParams(
			getBlockWidth(), getBlockHeight(), PFG_RG32_UINT, "m_eacTargetRes[0]", TextureFlags::Uav ) );
		if( rg11 )
		{
			m_eacTargetRes[1] =
				createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_RG32_UINT,
											  "m_eacTargetRes[1]", TextureFlags::Uav ) );
		}

		m_eacPso = createComputePsoFromFile( "eac_r11.glsl", "../Data/" );

		if( rg11 )
		{
			m_stitchedTarget =
				createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_RGBA32_UINT,
											  "m_stitchedTarget", TextureFlags::Uav ) );
			m_stitchPso = createComputePsoFromFile( "etc2_rgba_stitch.glsl", "../Data/" );
		}

		StagingTexture stagingTex = createStagingTexture( m_width, m_height, srcImage.format, true );
		memcpy( stagingTex.data, srcImage.data, stagingTex.sizeBytes );
		uploadStagingTexture( stagingTex, m_srcTexture );
		destroyStagingTexture( stagingTex );
	}
	//-------------------------------------------------------------------------
	void EncoderEAC::deinitResources()
	{
		if( m_stitchedTarget )
		{
			destroyTexture( m_stitchedTarget );
			m_stitchedTarget = 0;
		}
		for( size_t i = 0u; i < 2u; ++i )
		{
			if( m_eacTargetRes[i] )
			{
				destroyTexture( m_eacTargetRes[i] );
				m_eacTargetRes[i] = 0;
			}
		}
		destroyTexture( m_srcTexture );
		m_srcTexture = 0;

		if( m_downloadStaging.bufferName )
			destroyStagingTexture( m_downloadStaging );

		destroyPso( m_eacPso );
		if( m_stitchPso.computePso )
			destroyPso( m_stitchPso );
	}
	//-------------------------------------------------------------------------
	void EncoderEAC::execute01()
	{
		bindTexture( 0u, m_srcTexture );
		bindComputePso( m_eacPso );

		const size_t numChannels = m_eacTargetRes[1] ? 2u : 1u;
		for( size_t i = 0u; i < numChannels; ++i )
		{
			bindUav( 0u, m_eacTargetRes[i], PFG_RG32_UINT, ResourceAccess::Write );

			glUniform1f( 0, i == 0u ? 0.0f : 1.0f );

			glDispatchCompute( alignToNextMultiple( m_width, 4u ) / 4u,
							   alignToNextMultiple( m_height, 4u ) / 4u, 1u );
		}
	}
	//-------------------------------------------------------------------------
	void EncoderEAC::execute02()
	{
		if( !m_eacTargetRes[1] )
		{
			// This step is only relevant when doing RG11
			return;
		}

		glMemoryBarrier( GL_TEXTURE_FETCH_BARRIER_BIT );
		bindTexture( 0u, m_eacTargetRes[1] );
		bindTexture( 1u, m_eacTargetRes[0] );
		bindUav( 0u, m_stitchedTarget, PFG_RGBA32_UINT, ResourceAccess::Write );
		bindComputePso( m_stitchPso );
		glDispatchCompute( alignToNextMultiple( m_width, 32u ) / 32u,
						   alignToNextMultiple( m_height, 32u ) / 32u, 1u );
	}
	//-------------------------------------------------------------------------
	void EncoderEAC::startDownload()
	{
		glMemoryBarrier( GL_PIXEL_BUFFER_BARRIER_BIT );

		if( m_downloadStaging.bufferName )
			destroyStagingTexture( m_downloadStaging );
		m_downloadStaging =
			createStagingTexture( getBlockWidth(), getBlockHeight(),
								  m_eacTargetRes[1] ? PFG_RGBA32_UINT : PFG_RG32_UINT, false );
		downloadStagingTexture( m_eacTargetRes[1] ? m_stitchedTarget : m_eacTargetRes[0],
								m_downloadStaging );
	}
	//-------------------------------------------------------------------------
	void EncoderEAC::downloadTo( CpuImage &outImage )
	{
		glFinish();
		outImage.width = m_width;
		outImage.height = m_height;
		outImage.format = m_eacTargetRes[1] ? PFG_EAC_R11_UNORM : PFG_EAC_RG11_UNORM;
		outImage.data = reinterpret_cast<uint8_t *>( m_downloadStaging.data );
	}
}  // namespace betsy
