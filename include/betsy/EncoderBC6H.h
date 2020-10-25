
#pragma once

#include "betsy/EncoderGL.h"

#include "GL/glcorearb.h"

namespace betsy
{
	struct CpuImage;

	class EncoderBC6H : public EncoderGL
	{
		uint32_t m_width;
		uint32_t m_height;

		GLuint m_srcTexture;
		GLuint m_compressTargetRes;
		GLuint m_dstTexture;

		StagingTexture m_downloadStaging;

		ComputePso m_compressPso;

		uint32_t getBlockWidth() const { return ( m_width + 3u ) >> 2u; }
		uint32_t getBlockHeight() const { return ( m_height + 3u ) >> 2u; }

	public:
		EncoderBC6H();
		~EncoderBC6H();

		void initResources( const CpuImage &srcImage );
		void deinitResources();

		void execute01();
		void execute02();

		void startDownload();
		void downloadTo( CpuImage &outImage );
	};
}  // namespace betsy
