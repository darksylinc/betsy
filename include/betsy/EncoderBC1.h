
#pragma once

#include "betsy/EncoderGL.h"

#include "GL/glcorearb.h"

namespace betsy
{
	struct CpuImage;

	class EncoderBC1 : public EncoderGL
	{
	protected:
		uint32_t m_width;
		uint32_t m_height;

		GLuint m_srcTexture;
		GLuint m_bc1TargetRes;
		GLuint m_stitchedTarget;  // Only used for RG11
		GLuint m_dstTexture;

		// Tables that contain the lowest error when all pixels have the same index
		GLuint m_bc1TablesSsbo;

		StagingTexture m_downloadStaging;

		ComputePso m_bc1Pso;
		ComputePso m_stitchPso;  // Only used for RG11, Combines EAC R11 with G11 to form RG11

	public:
		EncoderBC1();
		~EncoderBC1();

		/** Initialize resources. Must be called before execute*()
		@param srcImage
			Source Image should be RGBA8888, and should NOT be sRGB
		@param bCompressAlpha
			When true, compresses to BC3
		*/
		void initResources( const CpuImage &srcImage, const bool useBC3 );
		void deinitResources();

		void execute01();
		void execute02();
		void execute03();

		void startDownload();
		void downloadTo( CpuImage &outImage );
	};
}  // namespace betsy
