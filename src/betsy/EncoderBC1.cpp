
#include "betsy/EncoderBC1.h"

#include "betsy/CpuImage.h"

#include "BC1_tables.inl"

#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>

namespace betsy
{
	struct Bc1Tables
	{
		float oMatch5[256][2];
		float oMatch6[256][2];
	};

	static Bc1Tables getBc1Tables()
	{
		Bc1Tables tables;

		for( size_t i = 0u; i < 256; ++i )
		{
			tables.oMatch5[i][0] = static_cast<float>( stb__OMatch5[i][0] );
			tables.oMatch5[i][1] = static_cast<float>( stb__OMatch5[i][1] );
			tables.oMatch6[i][0] = static_cast<float>( stb__OMatch6[i][0] );
			tables.oMatch6[i][1] = static_cast<float>( stb__OMatch6[i][1] );
		}

		return tables;
	}

	EncoderBC1::EncoderBC1() :
		m_width( 0 ),
		m_height( 0 ),
		m_srcTexture( 0 ),
		m_bc1TargetRes( 0 ),
		m_bc4TargetRes( 0 ),
		m_stitchedTarget( 0 ),
		m_dstTexture( 0 ),
		m_bc1TablesSsbo( 0 )
	{
	}
	//-------------------------------------------------------------------------
	EncoderBC1::~EncoderBC1() { assert( !m_srcTexture && "deinitResources not called!" ); }
	//-------------------------------------------------------------------------
	void EncoderBC1::initResources( const CpuImage &srcImage, const bool useBC3, const bool bDither )
	{
		m_width = srcImage.width;
		m_height = srcImage.height;

		const PixelFormat srcFormat =
			srcImage.format == PFG_RGBA8_UNORM_SRGB ? PFG_RGBA8_UNORM : srcImage.format;

		m_srcTexture = createTexture( TextureParams( m_width, m_height, srcFormat, "m_srcTexture" ) );

		m_bc1TargetRes = createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_RG32_UINT,
													   "m_bc1TargetRes", TextureFlags::Uav ) );
		m_dstTexture = createTexture(
			TextureParams( m_width, m_height, useBC3 ? PFG_BC3_UNORM : PFG_BC1_UNORM, "m_dstTexture" ) );

		{
			Bc1Tables bc1Tables = getBc1Tables();
			m_bc1TablesSsbo = createUavBuffer( sizeof( Bc1Tables ), &bc1Tables );
		}

		m_bc1Pso = createComputePsoFromFile( bDither ? "bc1_dither.glsl" : "bc1.glsl", "../Data/" );

		if( useBC3 )
		{
			m_bc4TargetRes =
				createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_RG32_UINT,
											  "m_bc4TargetRes", TextureFlags::Uav ) );
			m_stitchedTarget =
				createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_RGBA32_UINT,
											  "m_stitchedTarget", TextureFlags::Uav ) );
			m_bc4Pso = createComputePsoFromFile( "bc4.glsl", "../Data/" );
			m_stitchPso = createComputePsoFromFile( "etc2_rgba_stitch.glsl", "../Data/" );
		}

		StagingTexture stagingTex = createStagingTexture( m_width, m_height, srcImage.format, true );
		memcpy( stagingTex.data, srcImage.data, stagingTex.sizeBytes );
		uploadStagingTexture( stagingTex, m_srcTexture );
		destroyStagingTexture( stagingTex );
	}
	//-------------------------------------------------------------------------
	void EncoderBC1::deinitResources()
	{
		if( m_dstTexture )
		{
			destroyTexture( m_dstTexture );
			m_dstTexture = 0;
		}
		if( m_stitchedTarget )
		{
			destroyTexture( m_stitchedTarget );
			m_stitchedTarget = 0;
		}
		destroyTexture( m_bc1TargetRes );
		m_bc1TargetRes = 0;
		destroyTexture( m_srcTexture );
		m_srcTexture = 0;

		if( m_downloadStaging.bufferName )
			destroyStagingTexture( m_downloadStaging );

		destroyPso( m_bc1Pso );
		if( m_stitchPso.computePso )
			destroyPso( m_stitchPso );
	}
	//-------------------------------------------------------------------------
	void EncoderBC1::execute01()
	{
		bindTexture( 0u, m_srcTexture );
		bindComputePso( m_bc1Pso );
		bindUav( 0u, m_bc1TargetRes, PFG_RG32_UINT, ResourceAccess::Write );
		bindUavBuffer( 1u, m_bc1TablesSsbo, 0u, sizeof( Bc1Tables ) );

		glUniform1ui( 0, 2u );

		glDispatchCompute( alignToNextMultiple( m_width, ( 8u * 4u ) ) / ( 8u * 4u ),
						   alignToNextMultiple( m_height, ( 8u * 4u ) ) / ( 8u * 4u ), 1u );

		if( m_bc4TargetRes )
		{
			// Compress Alpha too (using BC4)
			bindComputePso( m_bc4Pso );
			bindUav( 0u, m_bc4TargetRes, PFG_RG32_UINT, ResourceAccess::Write );

			// p_channelIdx, p_useSNorm
			glUniform2f( 0, 3.0f, 0.0f );

			glDispatchCompute( 1u,  //
							   alignToNextMultiple( m_width, 16u ) / 16u,
							   alignToNextMultiple( m_height, 16u ) / 16u );
		}
	}
	//-------------------------------------------------------------------------
	void EncoderBC1::execute02()
	{
		if( !m_bc4TargetRes )
			return;  // This step is only relevant when doing BC3

		glMemoryBarrier( GL_TEXTURE_FETCH_BARRIER_BIT );
		bindTexture( 0u, m_bc1TargetRes );
		bindTexture( 1u, m_bc4TargetRes );
		bindUav( 0u, m_stitchedTarget, PFG_RGBA32_UINT, ResourceAccess::Write );
		bindComputePso( m_stitchPso );
		glDispatchCompute( alignToNextMultiple( m_width, 32u ) / 32u,
						   alignToNextMultiple( m_height, 32u ) / 32u, 1u );
	}
	//-------------------------------------------------------------------------
	void EncoderBC1::execute03()
	{
		// It's unclear which of these 2 barrier bits GL wants in order for glCopyImageSubData to work
		glMemoryBarrier( GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		// Copy "8x8" PFG_RG32_UINT   -> 32x32 PFG_BC1_UNORM
		// Copy "8x8" PFG_RGBA32_UINT -> 32x32 PFG_BC3_UNORM
		glCopyImageSubData( m_stitchedTarget ? m_stitchedTarget : m_bc1TargetRes,  //
							GL_TEXTURE_2D, 0, 0, 0, 0,                             //
							m_dstTexture, GL_TEXTURE_2D, 0, 0, 0, 0,               //
							( GLsizei )( getBlockWidth() ), ( GLsizei )( getBlockHeight() ), 1 );
	}
	//-------------------------------------------------------------------------
	void EncoderBC1::startDownload()
	{
		glMemoryBarrier( GL_PIXEL_BUFFER_BARRIER_BIT );

		if( m_downloadStaging.bufferName )
			destroyStagingTexture( m_downloadStaging );
		m_downloadStaging =
			createStagingTexture( getBlockWidth(), getBlockHeight(),
								  m_stitchedTarget ? PFG_RGBA32_UINT : PFG_RG32_UINT, false );
		downloadStagingTexture( m_stitchedTarget ? m_stitchedTarget : m_bc1TargetRes,
								m_downloadStaging );
	}
	//-------------------------------------------------------------------------
	void EncoderBC1::downloadTo( CpuImage &outImage )
	{
		glFinish();
		outImage.width = m_width;
		outImage.height = m_height;
		outImage.format = m_stitchedTarget ? PFG_BC3_UNORM : PFG_BC1_UNORM;
		outImage.data = reinterpret_cast<uint8_t *>( m_downloadStaging.data );
	}
}  // namespace betsy
