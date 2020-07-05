
#include "betsy/CpuImage.h"

//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"

#define FREEIMAGE_LIB
#include "FreeImage.h"

#include <malloc.h>
#include <memory.h>
#include <stdio.h>

namespace betsy
{
	CpuImage::CpuImage() : data( 0 ), width( 0 ), height( 0 ), format( PFG_RGBA16_FLOAT ) {}
	//-------------------------------------------------------------------------
	CpuImage::CpuImage( const char *fullpath ) :
		data( 0 ),
		width( 0 ),
		height( 0 ),
		format( PFG_RGBA16_FLOAT )
	{
		const FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename( fullpath );

		FIBITMAP *fiBitmap = FreeImage_Load( fif, fullpath );
		if( !fiBitmap )
		{
			fprintf( stderr, "Failed to load %s\n", fullpath );
			return;
		}

		// Must derive format first, this may perform conversions
		const FREE_IMAGE_TYPE imageType = FreeImage_GetImageType( fiBitmap );
		const FREE_IMAGE_COLOR_TYPE colourType = FreeImage_GetColorType( fiBitmap );
		unsigned bpp = FreeImage_GetBPP( fiBitmap );

		switch( imageType )
		{
		case FIT_UNKNOWN:
		case FIT_COMPLEX:
		case FIT_DOUBLE:
			printf( "Unknown or unsupported image format %s\n", fullpath );
			return;
		case FIT_BITMAP:
			// Standard image type
			// Perform any colour conversions for greyscale
			if( colourType == FIC_MINISWHITE || colourType == FIC_MINISBLACK )
			{
				FIBITMAP *newBitmap = FreeImage_ConvertToGreyscale( fiBitmap );
				// free old bitmap and replace
				FreeImage_Unload( fiBitmap );
				fiBitmap = newBitmap;
				// get new formats
				bpp = FreeImage_GetBPP( fiBitmap );
			}
			// Perform any colour conversions for RGB
			else if( bpp < 8 || colourType == FIC_PALETTE || colourType == FIC_CMYK )
			{
				FIBITMAP *newBitmap = NULL;
				if( FreeImage_IsTransparent( fiBitmap ) )
				{
					// convert to 32 bit to preserve the transparency
					// (the alpha byte will be 0 if pixel is transparent)
					newBitmap = FreeImage_ConvertTo32Bits( fiBitmap );
				}
				else
				{
					// no transparency - only 3 bytes are needed
					newBitmap = FreeImage_ConvertTo24Bits( fiBitmap );
				}

				// free old bitmap and replace
				FreeImage_Unload( fiBitmap );
				fiBitmap = newBitmap;
				// get new formats
				bpp = FreeImage_GetBPP( fiBitmap );
			}

			// by this stage, 8-bit is greyscale, 16/24/32 bit are RGB[A]
			switch( bpp )
			{
			case 8:
				printf( "8 bpp Monochrome textures not supported %s\n", fullpath );
				return;
			case 16:
				printf( "16 bpp textures not supported %s\n", fullpath );
				return;
			case 24:
			case 32:
				format = PFG_RGBA8_UNORM_SRGB;
				break;
			}
			break;
		case FIT_UINT16:
		case FIT_INT16:
			// 16-bit greyscale
			printf( "16 bpp Monochrome textures not supported %s\n", fullpath );
			return;
		case FIT_UINT32:
		case FIT_INT32:
			printf( "32 bpp Monochrome textures not supported %s\n", fullpath );
			break;
		case FIT_FLOAT:
			// Single-component floating point data
			format = PFG_RGBA32_FLOAT;
			break;
		case FIT_RGB16:
		case FIT_RGBA16:
			printf( "Texture format not supported %s\n", fullpath );
			return;
		case FIT_RGBF:
			format = PFG_RGBA32_FLOAT;
			break;
		case FIT_RGBAF:
			format = PFG_RGBA32_FLOAT;
			break;
		}

		width = FreeImage_GetWidth( fiBitmap );
		height = FreeImage_GetHeight( fiBitmap );

		const size_t neededBytes = getSizeBytes( width, height, 1u, 1u, format );
		data = reinterpret_cast<uint8_t *>( malloc( neededBytes ) );

		// Convert data inverting scanlines
		const size_t bytesPerRow = FreeImage_GetPitch( fiBitmap );
		const uint8_t *srcData = FreeImage_GetBits( fiBitmap );

		switch( imageType )
		{
		case FIT_BITMAP:
			if( bpp == 24 )
				RGB8toRGBA8( srcData, bytesPerRow );
			else if( bpp == 32 )
				BGRANtoRGBAN( srcData, bytesPerRow );
			break;
		case FIT_FLOAT:
			R32toRGBA32( reinterpret_cast<const uint32_t *>( srcData ), bytesPerRow );
			break;
		case FIT_RGBF:
			RGB32toRGBA32( reinterpret_cast<const uint32_t *>( srcData ), bytesPerRow );
			break;
		case FIT_RGBAF:
			BGRANtoRGBAN<uint32_t>( reinterpret_cast<const uint32_t *>( srcData ), bytesPerRow );
			break;
		default:
			break;
		}

		FreeImage_Unload( fiBitmap );
	}
	//-------------------------------------------------------------------------
	CpuImage::~CpuImage()
	{
		if( data )
		{
			free( data );
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
				const uint8_t b = *srcData++;
				const uint8_t g = *srcData++;
				const uint8_t r = *srcData++;
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
