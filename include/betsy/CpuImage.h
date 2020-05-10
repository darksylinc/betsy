
#pragma once

#include "betsy/EncoderGL.h"

namespace betsy
{
	struct CpuImage
	{
		uint8_t *data;
		uint32_t width;
		uint32_t height;

		PixelFormat format;

		CpuImage();
		CpuImage( const char *fullpath );
		~CpuImage();

		static size_t getBytesPerPixel( PixelFormat format );

		static size_t getSizeBytes( uint32_t width, uint32_t height, uint32_t depth, uint32_t slices,
									PixelFormat format, uint32_t rowAlignment = 4u );
	};
}  // namespace betsy
