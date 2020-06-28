
#include "betsy/EncoderETC2.h"

#include "betsy/CpuImage.h"

#include "ETC1_tables.inl"

#include <assert.h>
#include <stdio.h>
#include <limits>

namespace betsy
{
	EncoderETC2::EncoderETC2() : m_thModesTargetRes( 0 ), m_thModesError( 0 ) {}
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

		m_thModesPso = createComputePsoFromFile( "etc2_th.glsl", "../Data/" );
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::deinitResources()
	{
		destroyTexture( m_thModesError );
		m_thModesError = 0;
		destroyTexture( m_thModesTargetRes );
		m_thModesTargetRes = 0;
		destroyPso( m_thModesPso );

		EncoderETC1::deinitResources();
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
		bindComputePso( m_thModesPso );
		glDispatchCompute( alignToNextMultiple( m_width, 4u ) / 4u,
						   alignToNextMultiple( m_height, 4u ) / 4u, 1u );
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::execute02()
	{
		// Decide which the best modes and merge

		glMemoryBarrier( GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		// Copy "8x8" PFG_RG32_UINT -> 32x32 PFG_ETC1_RGB8_UNORM
		glCopyImageSubData( m_thModesTargetRes, GL_TEXTURE_2D, 0, 0, 0, 0,  //
							m_dstTexture, GL_TEXTURE_2D, 0, 0, 0, 0,        //
							( GLsizei )( m_width >> 2u ), ( GLsizei )( m_height >> 2u ), 1 );

		StagingTexture stagingTex =
			createStagingTexture( m_width >> 2u, m_height >> 2u, PFG_RG32_UINT, false );
		downloadStagingTexture( m_thModesTargetRes, stagingTex );
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
		downloadStagingTexture( m_eacTargetRes ? m_stitchedTarget : m_thModesTargetRes,
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
