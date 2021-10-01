
#pragma once
#include <stdint.h>
#include "Encoder.h"

namespace betsy
{
	struct CpuImage
	{
		uint8_t *data;
		uint32_t width;
		uint32_t height;

		gli::format format;

		CpuImage();
		CpuImage( const char *fullpath );
		~CpuImage();

		static size_t getBytesPerPixel( gli::format format );

		static size_t getSizeBytes( uint32_t width, uint32_t height, uint32_t depth, uint32_t slices,
									gli::format format, uint32_t rowAlignment = 4u );

	protected:
		void RGB8toRGBA8( uint8_t const *__restrict srcData, size_t bytesPerRow );
		void R32toRGBA32( uint32_t const *__restrict srcData, size_t bytesPerRow );
		void RGB32toRGBA32( uint32_t const *__restrict srcData, size_t bytesPerRow );
		template <typename T>
		void BGRANtoRGBAN( T const *__restrict srcData, size_t bytesPerRow );
	};
}  // namespace betsy
