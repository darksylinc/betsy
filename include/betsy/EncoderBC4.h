
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

		bool m_encodeSNorm;

		StagingTexture m_downloadStaging;

		ComputePso m_bc4Pso;
		ComputePso m_stitchPso;  // Only used for BC5, Combines BC4 R with G to form BC5

		uint32_t getBlockWidth() const { return ( m_width + 3u ) >> 2u; }
		uint32_t getBlockHeight() const { return ( m_height + 3u ) >> 2u; }

	public:
		EncoderBC4();
		~EncoderBC4();

		/** Initialize resources. Must be called before execute*()
		@param srcImage
		@param redGreen
			When false, compresses to one-channel BC4
			When true, compresses to two-channel BC5
		@param encodeSNorm
			When true, it encodes using BCn_SNORM variant instead of BCn_UNORM one.
			Source is still assumed to be UNORM though i.e. in range [0; 1].
			The main use case is normal maps.
		*/
		void initResources( const CpuImage &srcImage, const bool redGreen, const bool encodeSNorm );
		void deinitResources();

		void execute01();
		void execute02();
		void execute03();

		void startDownload();
		void downloadTo( CpuImage &outImage );
	};
}  // namespace betsy
