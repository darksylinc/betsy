
#include "betsy/EncoderETC2.h"

#include "betsy/CpuImage.h"

#include <assert.h>

#define TODO_better_barrier

namespace betsy
{
	EncoderETC2::EncoderETC2() :
		m_thModesTargetRes( 0 ),
		m_thModesError( 0 ),
		m_thModesC0C1( 0 ),
		m_pModeTargetRes( 0 ),
		m_pModeError( 0 )
	{
	}
	//-------------------------------------------------------------------------
	EncoderETC2::~EncoderETC2() { assert( !m_thModesTargetRes && "deinitResources not called!" ); }
	//-------------------------------------------------------------------------
	void EncoderETC2::initResources( const CpuImage &srcImage, const bool bCompressAlpha,
									 const bool bDither )
	{
		EncoderETC1::initResources( srcImage, bCompressAlpha, bDither, true );

		m_thModesTargetRes =
			createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_RG32_UINT,
										  "m_thModesTargetRes", TextureFlags::Uav ) );
		m_thModesError = createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_R32_FLOAT,
													   "m_thModesError", TextureFlags::Uav ) );
		m_thModesC0C1 = createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_RG32_UINT,
													  "m_thModesC0C1", TextureFlags::Uav, 120u ) );

		m_pModeTargetRes = createTexture( TextureParams(
			getBlockWidth(), getBlockHeight(), PFG_RG32_UINT, "m_pModeTargetRes", TextureFlags::Uav ) );
		m_pModeError = createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_R32_FLOAT,
													 "m_pModeError", TextureFlags::Uav ) );

		m_thModesPso = createComputePsoFromFile( "etc2_th.glsl", "../Data/" );
		m_thModesFindBestC0C1 =
			createComputePsoFromFile( "etc2_th_find_best_c0c1_k_means.glsl", "../Data/" );
		m_pModePso = createComputePsoFromFile( "etc2_p.glsl", "../Data/" );

		if( bCompressAlpha )
			m_stitchPso = createComputePsoFromFile( "etc2_rgba_selector.glsl", "../Data/" );
		else
			m_stitchPso = createComputePsoFromFile( "etc2_rgb_selector.glsl", "../Data/" );
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::deinitResources()
	{
		destroyTexture( m_pModeError );
		m_pModeError = 0;
		destroyTexture( m_pModeTargetRes );
		m_pModeTargetRes = 0;
		destroyTexture( m_thModesError );
		m_thModesError = 0;
		destroyTexture( m_thModesTargetRes );
		m_thModesTargetRes = 0;
		destroyPso( m_pModePso );
		destroyPso( m_thModesFindBestC0C1 );
		destroyPso( m_thModesPso );

		EncoderETC1::deinitResources();
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::execute00()
	{
		EncoderETC1::execute00();

		bindTexture( 0u, m_ditheredTexture );
		bindUav( 0u, m_thModesC0C1, PFG_RG32_UINT, ResourceAccess::Write );
		bindComputePso( m_thModesFindBestC0C1 );

		glDispatchCompute( 1u,  //
						   alignToNextMultiple( m_width, 16u ) / 16u,
						   alignToNextMultiple( m_height, 8u ) / 8u );

		TODO_better_barrier;
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::execute01( EncoderETC2::Etc1Quality quality )
	{
		EncoderETC1::execute01( quality );

		bindTexture( 0u, m_ditheredTexture );
		bindUav( 0u, m_thModesTargetRes, PFG_RG32_UINT, ResourceAccess::Write );
		bindUav( 1u, m_thModesError, PFG_R32_FLOAT, ResourceAccess::Write );
		bindUav( 2u, m_thModesC0C1, PFG_RG32_UINT, ResourceAccess::Write );
		bindComputePso( m_thModesPso );
		glDispatchCompute( alignToNextMultiple( m_width, 4u ) / 4u,
						   alignToNextMultiple( m_height, 4u ) / 4u, 1u );

		bindUav( 0u, m_pModeTargetRes, PFG_RG32_UINT, ResourceAccess::Write );
		bindUav( 1u, m_pModeError, PFG_R32_FLOAT, ResourceAccess::Write );
		bindComputePso( m_pModePso );
		glDispatchCompute( alignToNextMultiple( m_width, 8u ) / 8u,
						   alignToNextMultiple( m_height, 8u ) / 8u, 1u );
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::execute02()
	{
		// Decide which the best modes and merge and stitch
		glMemoryBarrier( GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		bindTexture( 0u, m_etc1Error );
		bindTexture( 1u, m_thModesError );
		bindTexture( 2u, m_pModeError );

		bindTexture( 3u, m_compressTargetRes );
		bindTexture( 4u, m_thModesTargetRes );

		if( hasAlpha() )
		{
			bindTexture( 5u, m_pModeTargetRes );
			bindTexture( 6u, m_eacTargetRes );

			bindUav( 0u, m_stitchedTarget, PFG_RGBA32_UINT, ResourceAccess::Write );
		}
		else
		{
			bindUav( 0u, m_pModeTargetRes, PFG_RG32_UINT, ResourceAccess::ReadWrite );
		}

		bindComputePso( m_stitchPso );
		glDispatchCompute( alignToNextMultiple( m_width, 32u ) / 32u,
						   alignToNextMultiple( m_height, 32u ) / 32u, 1u );
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::execute03()
	{
		// Copy "8x8" PFG_RG32_UINT -> 32x32 PFG_ETC1_RGB8_UNORM
		// Copy "8x8" PFG_RGBA32_UINT -> 32x32 PFG_ETC2_RGBA8_UNORM
		glCopyImageSubData( hasAlpha() ? m_stitchedTarget : m_pModeTargetRes,  //
							GL_TEXTURE_2D, 0, 0, 0, 0,                         //
							m_dstTexture, GL_TEXTURE_2D, 0, 0, 0, 0,           //
							( GLsizei )( getBlockWidth() ), ( GLsizei )( getBlockHeight() ), 1 );

		StagingTexture stagingTex =
			createStagingTexture( getBlockWidth(), getBlockHeight(), PFG_RG32_UINT, false );
		downloadStagingTexture( m_pModeTargetRes, stagingTex );
		glFinish();
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::startDownload()
	{
		glMemoryBarrier( GL_PIXEL_BUFFER_BARRIER_BIT );

		if( m_downloadStaging.bufferName )
			destroyStagingTexture( m_downloadStaging );
		m_downloadStaging = createStagingTexture( getBlockWidth(), getBlockHeight(),
												  hasAlpha() ? PFG_RGBA32_UINT : PFG_RG32_UINT, false );
		downloadStagingTexture( hasAlpha() ? m_stitchedTarget : m_pModeTargetRes, m_downloadStaging );
	}
}  // namespace betsy
