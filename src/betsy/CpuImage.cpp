
#include "betsy/CpuImage.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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
		int x, y, n;
		data = stbi_load( fullpath, &x, &y, &n, 4 );
		if( data )
		{
			width = static_cast<uint32_t>( x );
			height = static_cast<uint32_t>( y );
			format = PFG_RGBA8_UNORM_SRGB;
		}
		else
		{
			printf( "Failed to load %s\n", fullpath );
		}
		// ... process data if not NULL ...
		// ... x = width, y = height, n = # 8-bit components per pixel ...
		// ... replace '0' with '1'..'4' to force that many components per pixel
		// ... but 'n' will always be the number that it would have been if you said 0
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
			return 4u * 4u;
		case PFG_RGBA16_FLOAT:
			return 2u * 4u;
		case PFG_RGBA8_UNORM_SRGB:
			return 1u * 4u;
		case PFG_BC6H_UF16:
			return 0u;
		}
	}
	//-------------------------------------------------------------------------
	size_t CpuImage::getSizeBytes( uint32_t width, uint32_t height, uint32_t depth, uint32_t slices,
								   PixelFormat format, uint32_t rowAlignment )
	{
		size_t retVal = width * getBytesPerPixel( format );
		retVal = alignToNextMultiple( retVal, (size_t)rowAlignment );

		retVal *= height * depth * slices;

		return retVal;
	}
}  // namespace betsy
