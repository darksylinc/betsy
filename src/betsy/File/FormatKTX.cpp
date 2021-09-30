
#include "betsy/File/FormatKTX.h"

#include "betsy/EncoderGL.h"

#include "sds/sds_fstream.h"

#include <memory.h>
#include <stdio.h>

#define KTX_ENDIAN_REF ( 0x04030201 )
#define KTX_ENDIAN_REF_REV ( 0x01020304 )

namespace betsy
{
	struct KTXHeader
	{
		uint8_t identifier[12];
		uint32_t endianness;
		uint32_t glType;
		uint32_t glTypeSize;
		uint32_t glFormat;
		uint32_t glInternalFormat;
		uint32_t glBaseInternalFormat;
		uint32_t pixelWidth;
		uint32_t pixelHeight;
		uint32_t pixelDepth;
		uint32_t numberOfArrayElements;
		uint32_t numberOfFaces;
		uint32_t numberOfMipmapLevels;
		uint32_t bytesOfKeyValueData;
	};
	//-------------------------------------------------------------------------
	void FormatKTX::save( const char *fullpath, const CpuImage &cpuImage )
	{
		sds::fstream file( fullpath, sds::fstream::OutputDiscard );

		if( !file.is_open() )
		{
			fprintf( stderr, "Could not save to '%s' Check write access and whether the disk is full\n",
					 fullpath );
			return;
		}

		KTXHeader header;
		memset( &header, 0, sizeof( header ) );
		const uint8_t c_KtxFileIdentifier[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31,
												  0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
		memcpy( header.identifier, c_KtxFileIdentifier, sizeof( c_KtxFileIdentifier ) );

		header.endianness = KTX_ENDIAN_REF;
		header.pixelWidth = cpuImage.width;
		header.pixelHeight = cpuImage.height;
		header.pixelDepth = 0u;             // KTX spec says it must be 0 for 2D
		header.numberOfArrayElements = 0u;  // KTX spec says it must be 0 for 2D
		header.numberOfFaces = 1u;
		header.numberOfMipmapLevels = 1u;

		if( !CpuImage::isCompressed( cpuImage.format ) )
		{
			header.glInternalFormat = EncoderGL::get( cpuImage.format );
			header.glBaseInternalFormat = EncoderGL::getBaseFormat( cpuImage.format );
			EncoderGL::getFormatAndType( cpuImage.format, header.glFormat, header.glType );
			header.glTypeSize = static_cast<uint32_t>( CpuImage::getBytesPerPixel( cpuImage.format ) );
		}
		else
		{
			header.glInternalFormat = EncoderGL::get( cpuImage.format );
			header.glBaseInternalFormat = EncoderGL::getBaseFormat( cpuImage.format );
			header.glFormat = 0u;
			header.glType = 0u;
			header.glTypeSize = 1u;
		}

		file.write<KTXHeader>( header );

		// Now deal with the data
		for( uint32_t level = 0u; level < header.numberOfMipmapLevels; ++level )
		{
			const size_t imageSize = CpuImage::getSizeBytes( header.pixelWidth, header.pixelHeight, 1u,
															 1u, cpuImage.format, 4u );
			file.write<uint32_t>( static_cast<uint32_t>( imageSize ) );

			for( uint32_t face = 0u; face < header.numberOfFaces; ++face )
				file.write( reinterpret_cast<const char *>( cpuImage.data ), imageSize );
		}
	}
}  // namespace betsy
