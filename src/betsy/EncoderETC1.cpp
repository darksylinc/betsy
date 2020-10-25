#define NOMINMAX

#include "betsy/EncoderETC1.h"

#include "betsy/CpuImage.h"

#include "ETC1_tables.inl"

#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <algorithm>
#include <limits>

namespace betsy
{
	static size_t etc1_decode_value( size_t diff, size_t inten, size_t selector, size_t packed_c )
	{
		int c;
		if( diff )
			c = int( ( packed_c >> 2u ) | ( packed_c << 3u ) );
		else
			c = int( packed_c | ( packed_c << 4u ) );
		c += c_etc1_inten_tables[inten][selector];
		size_t retVal;
		retVal = static_cast<size_t>( std::max<int>( c, 0 ) );
		retVal = std::min<size_t>( retVal, 255u );
		return retVal;
	}

	EncoderETC1::EncoderETC1() :
		m_srcTexture( 0 ),
		m_ditheredTexture( 0 ),
		m_compressTargetRes( 0 ),
		m_etc1Error( 0 ),
		m_eacTargetRes( 0 ),
		m_stitchedTarget( 0 ),
		m_dstTexture( 0 ),
		m_etc1TablesSsbo( 0 )
	{
	}
	//-------------------------------------------------------------------------
	EncoderETC1::~EncoderETC1() { assert( !m_srcTexture && "deinitResources not called!" ); }
	//-------------------------------------------------------------------------
	size_t EncoderETC1::getEtc1TablesSize()
	{
		return sizeof( float ) * 2u * 8u * 4u * 256u + sizeof( uint32_t ) * ( 2u * 33u + 254 * 11u );
	}
	//-------------------------------------------------------------------------
	uint8_t *EncoderETC1::createFilledEtc1Tables()
	{
		uint8_t *data = new uint8_t[getEtc1TablesSize()];

		float *asFloat = reinterpret_cast<float *>( data );

		for( size_t diff = 0u; diff < 2u; ++diff )
		{
			const size_t limit = diff ? 32u : 16u;

			for( size_t inten = 0; inten < 8; ++inten )
			{
				for( size_t selector = 0u; selector < 4u; ++selector )
				{
					const size_t inverse_table_index = diff + ( inten << 1u ) + ( selector << 4u );
					for( size_t color = 0u; color < 256u; ++color )
					{
						uint32_t best_error = std::numeric_limits<uint32_t>::max();
						size_t best_packed_c = 0u;
						for( size_t packed_c = 0; packed_c < limit; ++packed_c )
						{
							const size_t v = etc1_decode_value( diff, inten, selector, packed_c );
							uint32_t err = static_cast<uint32_t>( abs( static_cast<int>( v - color ) ) );
							if( err < best_error )
							{
								best_error = err;
								best_packed_c = packed_c;
								if( !best_error )
									break;
							}
						}
						asFloat[inverse_table_index * 256u + color] =
							static_cast<float>( best_packed_c | ( best_error << 8u ) );
					}
				}
			}
		}

		const size_t startIdx = sizeof( float ) * 2u * 8u * 4u * 256u;
		memcpy( data + startIdx, c_color8_to_etc_block_config_0_255,
				sizeof( c_color8_to_etc_block_config_0_255 ) );
		memcpy( data + startIdx + sizeof( c_color8_to_etc_block_config_0_255 ),
				c_color8_to_etc_block_config_1_to_254, sizeof( c_color8_to_etc_block_config_1_to_254 ) );

		return data;
	}
	//-------------------------------------------------------------------------
	bool EncoderETC1::hasAlpha() const { return m_eacTargetRes != 0; }
	//-------------------------------------------------------------------------
	void EncoderETC1::initResources( const CpuImage &srcImage, const bool bCompressAlpha,
									 const bool bDither, const bool bForEtc2 )
	{
		m_width = srcImage.width;
		m_height = srcImage.height;

		const PixelFormat srcFormat =
			srcImage.format == PFG_RGBA8_UNORM_SRGB ? PFG_RGBA8_UNORM : srcImage.format;

		m_srcTexture = createTexture( TextureParams( m_width, m_height, srcFormat, "m_srcTexture" ) );

		if( bDither )
		{
			m_ditheredTexture = createTexture( TextureParams( m_width, m_height, PFG_RGBA8_UNORM,
															  "m_ditheredTexture", TextureFlags::Uav ) );
			m_ditherPso = createComputePsoFromFile( "dither555.glsl", "../Data/" );
		}
		else
		{
			m_ditheredTexture = m_srcTexture;
		}

		m_compressTargetRes =
			createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_RG32_UINT,
										  "m_compressTargetRes", TextureFlags::Uav ) );

		if( bForEtc2 )
		{
			m_etc1Error = createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_R32_FLOAT,
														"m_etc1Error", TextureFlags::Uav ) );
		}

		m_dstTexture =
			createTexture( TextureParams( m_width, m_height, PFG_ETC1_RGB8_UNORM, "m_dstTexture" ) );

		{
			uint8_t *filledTables = createFilledEtc1Tables();
			m_etc1TablesSsbo = createUavBuffer( getEtc1TablesSize(), filledTables );
			delete[] filledTables;
		}

		m_compressPso =
			createComputePsoFromFile( bForEtc2 ? "etc1_with_error.glsl" : "etc1.glsl", "../Data/" );

		if( bCompressAlpha )
		{
			m_eacTargetRes =
				createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_RG32_UINT,
											  "m_eacTargetRes", TextureFlags::Uav ) );
			m_stitchedTarget =
				createTexture( TextureParams( getBlockWidth(), getBlockHeight(), PFG_RGBA32_UINT,
											  "m_stitchedTarget", TextureFlags::Uav ) );
			m_eacPso = createComputePsoFromFile( "eac.glsl", "../Data/" );

			// ETC2 codec does its own stitching
			if( !bForEtc2 )
				m_stitchPso = createComputePsoFromFile( "etc2_rgba_stitch.glsl", "../Data/" );
		}

		StagingTexture stagingTex = createStagingTexture( m_width, m_height, srcImage.format, true );
		memcpy( stagingTex.data, srcImage.data, stagingTex.sizeBytes );
		uploadStagingTexture( stagingTex, m_srcTexture );
		destroyStagingTexture( stagingTex );
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::initResources( const CpuImage &srcImage, const bool bCompressAlpha,
									 const bool bDither )
	{
		initResources( srcImage, bCompressAlpha, bDither, false );
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::deinitResources()
	{
		destroyUavBuffer( m_etc1TablesSsbo );
		m_etc1TablesSsbo = 0;

		destroyTexture( m_dstTexture );
		m_dstTexture = 0;
		if( m_etc1Error )
		{
			destroyTexture( m_etc1Error );
			m_etc1Error = 0;
		}
		destroyTexture( m_compressTargetRes );
		m_compressTargetRes = 0;
		if( m_eacTargetRes )
		{
			destroyTexture( m_stitchedTarget );
			m_stitchedTarget = 0;

			destroyTexture( m_eacTargetRes );
			m_eacTargetRes = 0;
		}
		if( m_ditheredTexture != m_srcTexture )
		{
			destroyTexture( m_ditheredTexture );
			m_ditheredTexture = 0;
			destroyPso( m_ditherPso );
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
	void EncoderETC1::execute00()
	{
		if( m_ditheredTexture == m_srcTexture )
			return;

		bindTexture( 0u, m_srcTexture );
		bindUav( 0u, m_ditheredTexture, PFG_RGBA8_UNORM, ResourceAccess::Write );
		bindComputePso( m_ditherPso );

		const uint32_t pixelsPerThreadGroup = 4u * 8u;
		glDispatchCompute( alignToNextMultiple( m_width, pixelsPerThreadGroup ) / pixelsPerThreadGroup,
						   alignToNextMultiple( m_height, pixelsPerThreadGroup ) / pixelsPerThreadGroup,
						   1u );

		glMemoryBarrier( GL_TEXTURE_FETCH_BARRIER_BIT );
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::execute01( EncoderETC1::Etc1Quality quality )
	{
		bindTexture( 0u, m_ditheredTexture );
		bindUav( 0u, m_compressTargetRes, PFG_RG32_UINT, ResourceAccess::Write );
		bindUavBuffer( 1u, m_etc1TablesSsbo, 0u, getEtc1TablesSize() );
		if( m_etc1Error )
			bindUav( 2u, m_etc1Error, PFG_R32_FLOAT, ResourceAccess::Write );
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

		if( hasAlpha() )
		{
			// Compress Alpha too (using EAC compressor)
			bindTexture( 0u, m_srcTexture );
			bindUav( 0u, m_eacTargetRes, PFG_RG32_UINT, ResourceAccess::Write );
			bindComputePso( m_eacPso );
			glDispatchCompute( alignToNextMultiple( m_width, 4u ) / 4u,
							   alignToNextMultiple( m_height, 4u ) / 4u, 1u );
		}
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::execute02()
	{
		if( !hasAlpha() )
		{
			// This step is only relevant when doing ETC2_RGBA
			return;
		}

		glMemoryBarrier( GL_TEXTURE_FETCH_BARRIER_BIT );
		bindTexture( 0u, m_compressTargetRes );
		bindTexture( 1u, m_eacTargetRes );
		bindUav( 0u, m_stitchedTarget, PFG_RGBA32_UINT, ResourceAccess::Write );
		bindComputePso( m_stitchPso );
		glDispatchCompute( alignToNextMultiple( m_width, 32u ) / 32u,
						   alignToNextMultiple( m_height, 32u ) / 32u, 1u );
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::execute03()
	{
		// It's unclear which of these 2 barrier bits GL wants in order for glCopyImageSubData to work
		glMemoryBarrier( GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		// Copy "8x8" PFG_RG32_UINT -> 32x32 PFG_ETC1_RGB8_UNORM
		glCopyImageSubData( m_compressTargetRes, GL_TEXTURE_2D, 0, 0, 0, 0,  //
							m_dstTexture, GL_TEXTURE_2D, 0, 0, 0, 0,         //
							( GLsizei )( getBlockWidth() ), ( GLsizei )( getBlockHeight() ), 1 );

		StagingTexture stagingTex =
			createStagingTexture( getBlockWidth(), getBlockHeight(), PFG_RG32_UINT, false );
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
		m_downloadStaging = createStagingTexture( getBlockWidth(), getBlockHeight(),
												  hasAlpha() ? PFG_RGBA32_UINT : PFG_RG32_UINT, false );
		downloadStagingTexture( hasAlpha() ? m_stitchedTarget : m_compressTargetRes, m_downloadStaging );
	}
	//-------------------------------------------------------------------------
	void EncoderETC1::downloadTo( CpuImage &outImage )
	{
		glFinish();
		outImage.width = m_width;
		outImage.height = m_height;
		outImage.format = hasAlpha() ? PFG_ETC2_RGBA8_UNORM : PFG_ETC1_RGB8_UNORM;
		outImage.data = reinterpret_cast<uint8_t *>( m_downloadStaging.data );
	}
}  // namespace betsy
