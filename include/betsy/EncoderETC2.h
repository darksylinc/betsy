
#pragma once

#include "betsy/EncoderETC1.h"

#include "GL/glcorearb.h"

namespace betsy
{
	struct CpuImage;

	class EncoderETC2 : public EncoderETC1
	{
	protected:
		EncoderETC1 mEncoderETC1;

		GLuint m_thModesTargetRes;
		GLuint m_thModesError;
		GLuint m_thModesC0C1;
		GLuint m_pModeTargetRes;
		GLuint m_pModeError;

		ComputePso m_thModesPso;
		ComputePso m_thModesFindBestC0C1;
		ComputePso m_pModePso;

	public:
		EncoderETC2();
		~EncoderETC2();

		/** Initialize resources. Must be called before execute*()
		@param srcImage
			Source Image should be RGBA8888, and should NOT be sRGB
		@param bCompressAlpha
			When true, compresses to ETC2_RGBA
		@param bDither
			Use Floyd-steinberg dithering. Anti-banding method.
		*/
		void initResources( const CpuImage &srcImage, const bool bCompressAlpha, const bool bDither );
		void deinitResources();

		void execute00();
		void execute01( EncoderETC2::Etc1Quality quality = cHighQuality );
		void execute02();
		void execute03();

		void startDownload();
	};
}  // namespace betsy
