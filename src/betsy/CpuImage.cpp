
#include "betsy/CpuImage.h"

//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"

#define FREEIMAGE_LIB
#include <malloc.h>
#include <memory.h>
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace betsy
{
	CpuImage::CpuImage() : data( 0 ), width( 0 ), height( 0 ), format( PFG_RGBA16_FLOAT ) {}
	//-------------------------------------------------------------------------
	CpuImage::CpuImage( const char *fullpath ) :
	    data( 0 ), width( 0 ), height( 0 ), format( PFG_RGBA16_FLOAT )
	{
		bool hdr = stbi_is_hdr( fullpath );
		int comp = 0;
		if( hdr )
		{
			format = PFG_RGBA32_FLOAT;
			data = (uint8_t *)stbi_loadf( fullpath, (int *)&width, (int *)&height, &comp, 0 );
		}
		else
		{
			format = PFG_RGBA8_UNORM_SRGB;
			auto tmpData = stbi_load( fullpath, (int *)&width, (int *)&height, &comp, 0 );

			stbi_write_tga("test2.tga",width,height,comp,tmpData);

			if(comp == 3)
			{
				const size_t neededBytes = getSizeBytes( width, height, 1u, 1u, format );
				data = reinterpret_cast<uint8_t *>( malloc( neededBytes ) );
				RGB8toRGBA8( tmpData, width * 3 );
				comp = 4;
				stbi_image_free( tmpData );
			}
			else
				data = tmpData;
		}
		if( comp != 4 )
		{
			printf( "Did not get correct amount of components!\n" );
			return;
		}

		stbi_write_tga("test.tga",width,height,comp,data);
	}
	//-------------------------------------------------------------------------
	CpuImage::~CpuImage()
	{
		if( data )
		{
			stbi_image_free( data );
			data = 0;
		}
	}
	//-------------------------------------------------------------------------
	size_t CpuImage::getBytesPerPixel( PixelFormat format )
	{
		switch( format )
		{
		case PFG_RGBA32_UINT:
		case PFG_RGBA32_FLOAT:
			return 4u * 4u;
		case PFG_RGBA16_FLOAT:
			return 2u * 4u;
		case PFG_R32_FLOAT:
			return 4u;
		case PFG_RG32_UINT:
			return 4u * 2u;
		case PFG_RGBA8_UNORM:
		case PFG_RGBA8_UNORM_SRGB:
			return 1u * 4u;
		case PFG_RG8_UINT:
			return 1u * 2u;
		case PFG_ETC1_RGB8_UNORM:
		case PFG_ETC2_RGBA8_UNORM:
		case PFG_EAC_R11_UNORM:
		case PFG_EAC_RG11_UNORM:
		case PFG_BC1_UNORM:
		case PFG_BC3_UNORM:
		case PFG_BC4_UNORM:
		case PFG_BC4_SNORM:
		case PFG_BC5_UNORM:
		case PFG_BC5_SNORM:
		case PFG_BC6H_UF16:
			return 0u;
		}

		return 0u;
	}
	//-----------------------------------------------------------------------------------
	bool CpuImage::isCompressed( PixelFormat pixelFormat )
	{
		switch( pixelFormat )
		{
		case PFG_BC1_UNORM:
		case PFG_BC3_UNORM:
		case PFG_BC4_UNORM:
		case PFG_BC4_SNORM:
		case PFG_BC5_UNORM:
		case PFG_BC5_SNORM:
		case PFG_BC6H_UF16:
		case PFG_ETC1_RGB8_UNORM:
		case PFG_ETC2_RGBA8_UNORM:
		case PFG_EAC_R11_UNORM:
		case PFG_EAC_RG11_UNORM:
			return true;
		default:
			return false;
		}
	}
	//-------------------------------------------------------------------------
	size_t CpuImage::getSizeBytes( uint32_t width, uint32_t height, uint32_t depth, uint32_t slices,
	                               PixelFormat format, uint32_t rowAlignment )
	{
		size_t retVal;

		if( isCompressed( format ) )
		{
			switch( format )
			{
			case PFG_BC1_UNORM:
			case PFG_BC4_UNORM:
			case PFG_BC4_SNORM:
			case PFG_ETC1_RGB8_UNORM:
			case PFG_EAC_R11_UNORM:
				retVal = ( ( width + 3u ) / 4u ) * ( ( height + 3u ) / 4u ) * 8u * depth * slices;
				break;
			case PFG_BC3_UNORM:
			case PFG_BC5_UNORM:
			case PFG_BC5_SNORM:
			case PFG_ETC2_RGBA8_UNORM:
			case PFG_EAC_RG11_UNORM:
			case PFG_BC6H_UF16:
				retVal = ( ( width + 3u ) / 4u ) * ( ( height + 3u ) / 4u ) * 16u * depth * slices;
				break;
			default:
				retVal = 0u;
			}
		}
		else
		{
			retVal = width * getBytesPerPixel( format );
			retVal = alignToNextMultiple( retVal, (size_t)rowAlignment );

			retVal *= height * depth * slices;
		}

		return retVal;
	}
	//-------------------------------------------------------------------------
	void CpuImage::RGB8toRGBA8( uint8_t const *__restrict srcData, size_t bytesPerRow )
	{
		const size_t imgHeight = height;
		const size_t imgWidth = width;
		for( size_t y = 0u; y < imgHeight; ++y )
		{
			uint8_t *__restrict dstData =
			    reinterpret_cast<uint8_t *__restrict>( data ) + ( imgHeight - y - 1u ) * imgWidth * 4u;

			for( size_t x = 0u; x < imgWidth; ++x )
			{
				const uint8_t r = *srcData++;
				const uint8_t g = *srcData++;
				const uint8_t b = *srcData++;
				*dstData++ = r;
				*dstData++ = g;
				*dstData++ = b;
				*dstData++ = 0xff;
			}

			// dstData's bytePerRow is already multiple of 4 (our natural alignment)
			srcData += bytesPerRow - imgWidth * 3u;
		}
	}
	//-------------------------------------------------------------------------
	void CpuImage::R32toRGBA32( uint32_t const *__restrict srcData, size_t bytesPerRow )
	{
		const size_t imgWidth = width;
		const size_t imgHeight = height;
		for( size_t y = 0u; y < imgHeight; ++y )
		{
			uint32_t *__restrict dstData =
			    reinterpret_cast<uint32_t *__restrict>( data ) + ( imgHeight - y - 1u ) * imgWidth * 4u;
			for( size_t x = 0u; x < ( bytesPerRow >> 2u ); ++x )
			{
				const uint32_t value = *srcData++;
				*dstData++ = value;
				*dstData++ = value;
				*dstData++ = value;
				*dstData++ = 0x3f800000;  // 1.0f
			}
		}
	}
	//-------------------------------------------------------------------------
	void CpuImage::RGB32toRGBA32( uint32_t const *__restrict srcData, size_t bytesPerRow )
	{
		const size_t imgWidth = width;
		const size_t imgHeight = height;
		for( size_t y = 0u; y < imgHeight; ++y )
		{
			uint32_t *__restrict dstData =
			    reinterpret_cast<uint32_t *__restrict>( data ) + ( imgHeight - y - 1u ) * imgWidth * 4u;
			for( size_t x = 0u; x < ( bytesPerRow >> 2u ) / 3u; ++x )
			{
				const uint32_t b = *srcData++;
				const uint32_t g = *srcData++;
				const uint32_t r = *srcData++;
				*dstData++ = r;
				*dstData++ = g;
				*dstData++ = b;
				*dstData++ = 0x3f800000;  // 1.0f
			}
		}
	}
	//-------------------------------------------------------------------------
	template <typename T>
	void CpuImage::BGRANtoRGBAN( T const *__restrict srcData, size_t bytesPerRow )
	{
		const size_t imgWidth = width;
		const size_t imgHeight = height;
		for( size_t y = 0u; y < imgHeight; ++y )
		{
			T *__restrict dstData =
			    reinterpret_cast<T *__restrict>( data ) + ( imgHeight - y - 1u ) * imgWidth * 4u;

			for( size_t x = 0u; x < imgWidth; ++x )
			{
				const T b = *srcData++;
				const T g = *srcData++;
				const T r = *srcData++;
				const T a = *srcData++;
				*dstData++ = r;
				*dstData++ = g;
				*dstData++ = b;
				*dstData++ = a;
			}

			// dstData's bytePerRow is already multiple of 4 (our natural alignment)
			srcData += bytesPerRow - imgWidth * 4u * sizeof( T );
		}
	}
}  // namespace betsy
