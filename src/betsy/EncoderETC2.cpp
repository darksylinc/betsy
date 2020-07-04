
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

		m_thModesTargetRes = createTexture( TextureParams( m_width >> 2u, m_height >> 2u, PFG_RG32_UINT,
														   "m_thModesTargetRes", TextureFlags::Uav ) );
		m_thModesError = createTexture( TextureParams( m_width >> 2u, m_height >> 2u, PFG_R32_FLOAT,
													   "m_thModesError", TextureFlags::Uav ) );
		m_thModesC0C1 = createTexture( TextureParams( m_width >> 2u, m_height >> 2u, PFG_RG32_UINT,
													  "m_thModesC0C1", TextureFlags::Uav, 120u ) );

		m_pModeTargetRes = createTexture( TextureParams( m_width >> 2u, m_height >> 2u, PFG_RG32_UINT,
														 "m_pModeTargetRes", TextureFlags::Uav ) );
		m_pModeError = createTexture( TextureParams( m_width >> 2u, m_height >> 2u, PFG_R32_FLOAT,
													 "m_pModeError", TextureFlags::Uav ) );

		m_thModesPso = createComputePsoFromFile( "etc2_th.glsl", "../Data/" );
		m_thModesFindBestC0C1 =
			createComputePsoFromFile( "etc2_th_find_best_c0c1_k_means.glsl", "../Data/" );
		m_pModePso = createComputePsoFromFile( "etc2_p.glsl", "../Data/" );
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
		glMemoryBarrier( GL_ALL_BARRIER_BITS );
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::execute01( EncoderETC2::Etc1Quality quality )
	{
#if 0
		EncoderETC1::execute01( quality );
#endif

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
		// Decide which the best modes and merge

		glMemoryBarrier( GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		// Copy "8x8" PFG_RG32_UINT -> 32x32 PFG_ETC1_RGB8_UNORM
		glCopyImageSubData( m_pModeTargetRes, GL_TEXTURE_2D, 0, 0, 0, 0,  //
							m_dstTexture, GL_TEXTURE_2D, 0, 0, 0, 0,      //
							( GLsizei )( m_width >> 2u ), ( GLsizei )( m_height >> 2u ), 1 );

		StagingTexture stagingTex =
			createStagingTexture( m_width >> 2u, m_height >> 2u, PFG_RG32_UINT, false );
		downloadStagingTexture( m_pModeTargetRes, stagingTex );
		glFinish();
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::execute03()
	{
		// It's not a typo. Stitching EAC into ETC2_RGBA must happen in our
		// EncoderETC2::execute03 which is EncoderETC1::execute02 for our base class
#if 0
		EncoderETC1::execute02();
#endif
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::execute04()
	{
#if 0
		// Also not a typo. Our 04 is our base's 03
		EncoderETC1::execute03();
#endif
	}
#if 1
	//-------------------------------------------------------------------------
	void EncoderETC2::startDownload()
	{
		glMemoryBarrier( GL_PIXEL_BUFFER_BARRIER_BIT );

		if( m_downloadStaging.bufferName )
			destroyStagingTexture( m_downloadStaging );
		m_downloadStaging = createStagingTexture(
			m_width >> 2u, m_height >> 2u, m_eacTargetRes ? PFG_RGBA32_UINT : PFG_RG32_UINT, false );
		downloadStagingTexture( m_eacTargetRes ? m_stitchedTarget : m_pModeTargetRes,
								m_downloadStaging );
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::downloadTo( CpuImage &outImage )
	{
		glFinish();
		outImage.width = m_width;
		outImage.height = m_height;
		outImage.format = m_eacTargetRes ? PFG_ETC2_RGBA8_UNORM : PFG_ETC1_RGB8_UNORM;
		outImage.data = reinterpret_cast<uint8_t *>( m_downloadStaging.data );
	}
#endif
}  // namespace betsy
