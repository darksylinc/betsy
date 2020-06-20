
#pragma once

#include "betsy/EncoderGL.h"

#include "GL/glcorearb.h"

namespace betsy
{
	struct CpuImage;

	/**
	@brief The EncoderBC4 class
		Handles BC4 and BC5 compression.
		Note that BC5 is just two BC4 blocks together.
	*/
	class EncoderBC4 : public EncoderGL
	{
	protected:
		uint32_t m_width;
		uint32_t m_height;

		GLuint m_srcTexture;
		GLuint m_bc4TargetRes[2];
		GLuint m_stitchedTarget;  // Only used for BC5
		GLuint m_dstTexture;

		StagingTexture m_downloadStaging;

		ComputePso m_bc4Pso;
		ComputePso m_stitchPso;  // Only used for BC5, Combines BC4 R with G to form BC5

	public:
		EncoderBC4();
		~EncoderBC4();

		/** Initialize resources. Must be called before execute*()
		@param srcImage
		@param redGreen
			When false, compresses to one-channel BC4
			When true, compresses to two-channel BC5
		*/
		void initResources( const CpuImage &srcImage, const bool redGreen );
		void deinitResources();

		void execute01();
		void execute02();
		void execute03();

		void startDownload();
		void downloadTo( CpuImage &outImage );
	};
}  // namespace betsy
