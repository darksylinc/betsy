
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

		StagingTexture m_stagingTex;

		ComputePso m_compressPso;

	public:
		EncoderBC6H();
		~EncoderBC6H();

		void initResources( const CpuImage &srcImage );
		void deinitResources();

		void execute01();
		void execute02();
	};
}  // namespace betsy
