
#pragma once

#include "GL/gl3w.h"
#include "betsy/Encoder.h"

namespace betsy
{
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

		gli::format format;

		const char *debugName;

		TextureParams( uint32_t _width, uint32_t _height, gli::format _format,
		               const char *_debugName = 0, uint32_t _flags = 0u, uint32_t _depthOrSlices = 1u,
		               uint8_t _numMipmaps = 1u );
	};

	struct StagingTexture
	{
		GLuint      bufferName;
		size_t      bytesPerRow;
		uint32_t    width;
		uint32_t    height;
		gli::format pixelFormat;
		void *      data;
		size_t      sizeBytes;
		StagingTexture();
	};

	struct StagingBuffer
	{
		GLuint bufferName;
		void * data;
		size_t sizeBytes;
	};

	struct ComputePso
	{
		GLuint computeShader;
		GLuint computePso;
		ComputePso();
	};

	class EncoderGL : public Encoder
	{
	public:
		static GLenum get( gli::format format );
		static GLenum getBaseFormat( gli::format format );
		static void   getFormatAndType( gli::format pixelFormat, GLenum &format, GLenum &type );

	protected:
		GLuint createTexture( const TextureParams &params );
		void   destroyTexture( GLuint texName );

		StagingTexture createStagingTexture( uint32_t width, uint32_t height, gli::format format,
		                                     bool forUpload );
		void           uploadStagingTexture( const StagingTexture &stagingTex, GLuint dstTexture );
		void           downloadStagingTexture( GLuint srcTexture, const StagingTexture &stagingTex );
		void           destroyStagingTexture( const StagingTexture &stagingTex );

		GLuint createUavBuffer( size_t sizeBytes, void *initialData );
		void   destroyUavBuffer( GLuint bufferName );

		ComputePso createComputePso( const char *csShader );
		void       destroyPso( ComputePso &pso );
		void       bindComputePso( const ComputePso &pso );

		void bindTexture( uint32_t slot, GLuint textureSrv );
		void bindUav( uint32_t slot, GLuint textureSrv, gli::format pixelFormat,
		              ResourceAccess::ResourceAccess access );
		void bindUavBuffer( uint32_t slot, GLuint buffer, size_t offset, size_t bufferSize );
	};
}  // namespace betsy
