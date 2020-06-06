
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
		GLuint m_eacTargetRes;    // Only used for ETC2_RGBA
		GLuint m_stitchedTarget;  // Only used for ETC2_RGBA
		GLuint m_dstTexture;

		StagingTexture m_downloadStaging;

		ComputePso m_compressPso;
		ComputePso m_eacPso;     // Only used for ETC2_RGBA
		ComputePso m_stitchPso;  // Only used for ETC2_RGBA, Combines ETC1 RGB with EAC alpha

	public:
		EncoderETC1();
		~EncoderETC1();

		/** Initialize resources. Must be called before execute*()
		@param srcImage
		@param bCompressAlpha
			When true, compresses to ETC2_RGBA
		*/
		void initResources( const CpuImage &srcImage, bool bCompressAlpha );
		void deinitResources();

		void execute01( EncoderETC1::Etc1Quality quality = cHighQuality );
		void execute02();
		void execute03();

		void startDownload();
		void downloadTo( CpuImage &outImage );
	};
}  // namespace betsy
