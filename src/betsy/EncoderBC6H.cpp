
#include "betsy/EncoderBC6H.h"

#include "betsy/CpuImage.h"

#include <assert.h>
#include <memory.h>

namespace betsy
{
	EncoderBC6H::EncoderBC6H() : m_srcTexture( 0 ), m_compressTargetRes( 0 ), m_dstTexture( 0 ) {}
	//-------------------------------------------------------------------------
	EncoderBC6H::~EncoderBC6H() { assert( !m_srcTexture && "deinitResources not called!" ); }
	//-------------------------------------------------------------------------
	void EncoderBC6H::initResources( const CpuImage &srcImage )
	{
		m_width = srcImage.width;
		m_height = srcImage.height;

		m_srcTexture =
			createTexture( TextureParams( m_width, m_height, srcImage.format, "m_srcTexture" ) );

		m_compressTargetRes = createTexture( TextureParams(
			m_width >> 2u, m_height >> 2u, PFG_RGBA32_UINT, "m_compressTargetRes", TextureFlags::Uav ) );

		m_dstTexture =
			createTexture( TextureParams( m_width, m_height, PFG_BC6H_UF16, "m_dstTexture" ) );

		m_compressPso = createComputePsoFromFile( "bc6h.glsl", "../Data/" );

		m_stagingTex = createStagingTexture( m_width, m_height, srcImage.format, true );
		memcpy( m_stagingTex.data, srcImage.data, m_stagingTex.sizeBytes );
		uploadStagingTexture( m_stagingTex, m_srcTexture );
	}
	//-------------------------------------------------------------------------
	void EncoderBC6H::deinitResources()
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
	void EncoderBC6H::execute01()
	{
		bindTexture( 0u, m_srcTexture );
		bindUav( 0u, m_compressTargetRes, PFG_RGBA32_UINT, ResourceAccess::Write );
		bindComputePso( m_compressPso );

		glUniform2f( 0, 1.0f / m_width, 1.0f / m_height );

		glDispatchCompute( alignToNextMultiple( m_width, 8u ) / 32u,
						   alignToNextMultiple( m_height, 8u ) / 32u, 1u );
	}
	//-------------------------------------------------------------------------
	void EncoderBC6H::execute02()
	{
		// It's unclear which of these 2 barrier bits GL wants in order for glCopyImageSubData to work
		glMemoryBarrier( GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		// Copy "8x8" PFG_RGBA32_UINT -> 32x32 PFG_BC6H_UF16
		glCopyImageSubData( m_compressTargetRes, GL_TEXTURE_2D, 0, 0, 0, 0,  //
							m_dstTexture, GL_TEXTURE_2D, 0, 0, 0, 0,         //
							( GLsizei )( m_width >> 2u ), ( GLsizei )( m_height >> 2u ), 1 );
	}
}  // namespace betsy
