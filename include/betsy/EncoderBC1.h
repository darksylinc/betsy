
#pragma once

#include "betsy/EncoderGL.h"

#include "GL/glcorearb.h"

namespace betsy
{
	struct CpuImage;

	/**
	@brief The EncoderBC1 class
		Supports BC1 (565) and BC3 (BC1 565 RGB + Alpha encoded using BC4)
	*/
	class EncoderBC1 : public EncoderGL
	{
	protected:
		uint32_t m_width;
		uint32_t m_height;

		GLuint m_srcTexture;
		GLuint m_bc1TargetRes;
		GLuint m_bc4TargetRes;    // Only used for BC3
		GLuint m_stitchedTarget;  // Only used for BC3
		GLuint m_dstTexture;

		// Tables that contain the lowest error when all pixels have the same index
		GLuint m_bc1TablesSsbo;

		StagingTexture m_downloadStaging;

		ComputePso m_bc1Pso;
		ComputePso m_bc4Pso;
		ComputePso m_stitchPso;  // Only used for RG11, Combines EAC R11 with G11 to form RG11

		uint32_t getBlockWidth() const { return ( m_width + 3u ) >> 2u; }
		uint32_t getBlockHeight() const { return ( m_height + 3u ) >> 2u; }

	public:
		EncoderBC1();
		~EncoderBC1();

		/** Initialize resources. Must be called before execute*()
		@param srcImage
			Source Image should be RGBA8888, and should NOT be sRGB
		@param bCompressAlpha
			When true, compresses to BC3
		@param bDither
			Use Floyd-steinberg dithering. Anti-banding method.
		*/
		void initResources( const CpuImage &srcImage, const bool useBC3, const bool bDither );
		void deinitResources();

		void execute01();
		void execute02();
		void execute03();

		void startDownload();
		void downloadTo( CpuImage &outImage );
	};
}  // namespace betsy
