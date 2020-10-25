
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
		GLuint m_ditheredTexture;  // m_ditheredTexture = m_srcTexture when dither is disabled
		GLuint m_compressTargetRes;
		GLuint m_etc1Error;       // Only used if bForEtc2 = true
		GLuint m_eacTargetRes;    // Only used for ETC2_RGBA
		GLuint m_stitchedTarget;  // Only used for ETC2_RGBA
		GLuint m_dstTexture;

		/// ETC1 tables (see ETC1_tables.inl) needed when either all pixels
		/// in a block or when all pixels in subblock are exactly the same.
		/// This is a quality improvement rather than a performance one.
		GLuint m_etc1TablesSsbo;

		StagingTexture m_downloadStaging;

		ComputePso m_ditherPso;
		ComputePso m_compressPso;
		ComputePso m_eacPso;     // Only used for ETC2_RGBA
		ComputePso m_stitchPso;  // Only used for ETC2_RGBA, Combines ETC1 RGB with EAC alpha

		static size_t   getEtc1TablesSize();
		static uint8_t *createFilledEtc1Tables();

		bool hasAlpha() const;

		void initResources( const CpuImage &srcImage, const bool bCompressAlpha, const bool bDither,
							const bool bForEtc2 );

		uint32_t getBlockWidth() const { return ( m_width + 3u ) >> 2u; }
		uint32_t getBlockHeight() const { return ( m_height + 3u ) >> 2u; }

	public:
		EncoderETC1();
		~EncoderETC1();

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
		void execute01( EncoderETC1::Etc1Quality quality = cHighQuality );
		void execute02();
		void execute03();

		void startDownload();
		void downloadTo( CpuImage &outImage );
	};
}  // namespace betsy
