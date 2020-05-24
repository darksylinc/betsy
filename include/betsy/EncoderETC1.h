
#pragma once

#include "betsy/EncoderGL.h"

#include "GL/glcorearb.h"

namespace betsy
{
	struct CpuImage;

	class EncoderETC1 : public EncoderGL
	{
	public:
		enum Etc1Quality
		{
			cLowQuality,
			cMediumQuality,
			cHighQuality
		};

	protected:
		uint32_t m_width;
		uint32_t m_height;

		GLuint m_srcTexture;
		GLuint m_compressTargetRes;
		GLuint m_dstTexture;

		StagingTexture m_stagingTex;

		ComputePso m_compressPso;

	public:
		EncoderETC1();
		~EncoderETC1();

		void initResources( const CpuImage &srcImage );
		void deinitResources();

		void execute01( EncoderETC1::Etc1Quality quality = cHighQuality );
		void execute02();
	};
}  // namespace betsy
