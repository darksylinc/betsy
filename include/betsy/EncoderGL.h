
#pragma once

#include "betsy/Encoder.h"

#include "GL/gl3w.h"

namespace betsy
{
	enum PixelFormat
	{
		PFG_RGBA32_UINT,
		PFG_RGBA32_FLOAT,
		PFG_RGBA16_FLOAT,
		PFG_RG32_UINT,
		PFG_RGBA8_UNORM,
		PFG_RGBA8_UNORM_SRGB,
		/// BC6H format (unsigned 16 bit float)
		PFG_BC6H_UF16,
		/// ETC1 (Ericsson Texture Compression)
		PFG_ETC1_RGB8_UNORM
	};

	namespace ResourceAccess
	{
		/// Enum identifying the texture access privilege
		enum ResourceAccess
		{
			Undefined = 0x00,
			Read = 0x01,
			Write = 0x02,
			ReadWrite = Read | Write
		};
	}  // namespace ResourceAccess

	namespace TextureFlags
	{
		enum TextureFlags
		{
			/// Texture can be used as an RTT (FBO in GL terms)
			RenderToTexture = 1u << 1u,
			/// Texture can be used as an UAV
			Uav = 1u << 2u
		};
	}

	struct TextureParams
	{
		uint32_t width;
		uint32_t height;
		uint32_t depthOrSlices;
		uint8_t  numMipmaps;  /// numMipmaps = 0 is invalid
		uint32_t flags;       /// See TextureFlags::TextureFlags

		PixelFormat format;

		const char *debugName;

		TextureParams( uint32_t _width, uint32_t _height, PixelFormat _format,
					   const char *_debugName = 0, uint32_t _flags = 0u, uint32_t _depthOrSlices = 1u,
					   uint8_t _numMipmaps = 1u );
	};

	struct StagingTexture
	{
		GLuint      bufferName;
		size_t      bytesPerRow;
		uint32_t    width;
		uint32_t    height;
		PixelFormat pixelFormat;
		void *      data;
		size_t      sizeBytes;
	};

	struct ComputePso
	{
		GLuint computeShader;
		GLuint computePso;
		ComputePso();
	};

	class EncoderGL : public Encoder
	{
	protected:
		static GLenum get( PixelFormat format );
		static void   getFormatAndType( PixelFormat pixelFormat, GLenum &format, GLenum &type );

		GLuint createTexture( const TextureParams &params );
		void   destroyTexture( GLuint texName );

		StagingTexture createStagingTexture( uint32_t width, uint32_t height, PixelFormat format,
											 bool forUpload );
		void           uploadStagingTexture( const StagingTexture &stagingTex, GLuint dstTexture );
		void           downloadStagingTexture( GLuint srcTexture, const StagingTexture &stagingTex );
		void           destroyStagingTexture( const StagingTexture &stagingTex );

		ComputePso createComputePsoFromFile( const char *shaderFilename, const char *relativePath );
		ComputePso createComputePso( const char *csShader );
		void       destroyPso( ComputePso &pso );
		void       bindComputePso( const ComputePso &pso );

		void bindTexture( uint32_t slot, GLuint textureSrv );
		void bindUav( uint32_t slot, GLuint textureSrv, PixelFormat pixelFormat,
					  ResourceAccess::ResourceAccess access );
	};
}  // namespace betsy
