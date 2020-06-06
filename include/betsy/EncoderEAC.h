
#pragma once

#include "betsy/EncoderGL.h"

#include "GL/glcorearb.h"

namespace betsy
{
	struct CpuImage;

	class EncoderEAC : public EncoderGL
	{
	protected:
		uint32_t m_width;
		uint32_t m_height;

		GLuint m_srcTexture;
		GLuint m_eacTargetRes[2];
		GLuint m_stitchedTarget;  // Only used for RG11

		StagingTexture m_downloadStaging;

		ComputePso m_eacPso;
		ComputePso m_stitchPso;  // Only used for RG11, Combines EAC R11 with G11 to form RG11

	public:
		EncoderEAC();
		~EncoderEAC();

		/** Initialize resources. Must be called before execute*()
		@param srcImage
		@param bCompressAlpha
			When true, compresses to ETC2_RGBA
		*/
		void initResources( const CpuImage &srcImage, const bool rg11 );
		void deinitResources();

		void execute01();
		void execute02();

		void startDownload();
		void downloadTo( CpuImage &outImage );
	};
}  // namespace betsy
