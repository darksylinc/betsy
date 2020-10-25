
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

		uint32_t getBlockWidth() const { return ( m_width + 3u ) >> 2u; }
		uint32_t getBlockHeight() const { return ( m_height + 3u ) >> 2u; }

	public:
		EncoderEAC();
		~EncoderEAC();

		/** Initialize resources. Must be called before execute*()
		@param srcImage
		@param rg11
			When true, compresses to two-channel RG11 instead of one-channel R11
		*/
		void initResources( const CpuImage &srcImage, const bool rg11 );
		void deinitResources();

		void execute01();
		void execute02();

		void startDownload();
		void downloadTo( CpuImage &outImage );
	};
}  // namespace betsy
