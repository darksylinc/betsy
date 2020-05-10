
#include "betsy/EncoderGL.h"

#include "betsy/CpuImage.h"
#include "betsy/IncludeParser.h"

#include <assert.h>
#include <string.h>

namespace betsy
{
	extern bool g_hasDebugObjectLabel;
	bool g_hasDebugObjectLabel = false;
	void ogreGlObjectLabel( GLenum identifier, GLuint name, const GLchar *label )
	{
		if( g_hasDebugObjectLabel )
			glObjectLabel( identifier, name, (GLsizei)strlen( label ), label );
	}
	//-------------------------------------------------------------------------
	TextureParams::TextureParams( uint32_t _width, uint32_t _height, PixelFormat _format,
								  const char *_debugName, uint32_t _flags, uint32_t _depthOrSlices,
								  uint8_t _numMipmaps ) :
		width( _width ),
		height( _height ),
		depthOrSlices( _depthOrSlices ),
		numMipmaps( _numMipmaps ),
		flags( _flags ),
		format( _format ),
		debugName( _debugName )
	{
	}
	//-------------------------------------------------------------------------
	ComputePso::ComputePso() : computeShader( 0 ), computePso( 0 ) {}
	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------
	GLenum EncoderGL::get( PixelFormat format )
	{
		switch( format )
		{
		case PFG_RGBA32_UINT:
			return GL_RGBA32UI;
		case PFG_RGBA16_FLOAT:
			return GL_RGBA16F;
		case PFG_RGBA8_UNORM_SRGB:
			return GL_SRGB8_ALPHA8;
		case PFG_BC6H_UF16:
			return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB;
		}
		return GL_NONE;
	}
	//-----------------------------------------------------------------------------------
	void EncoderGL::getFormatAndType( PixelFormat pixelFormat, GLenum &format, GLenum &type )
	{
		switch( pixelFormat )
		{
		case PFG_RGBA32_UINT:
			format = GL_RGBA_INTEGER;
			break;
		case PFG_RGBA16_FLOAT:
		case PFG_RGBA8_UNORM_SRGB:
			format = GL_RGBA;
			break;
		case PFG_BC6H_UF16:
			format = GL_NONE;
			assert( false &&
					"This should never happen. Compressed formats must use "
					"EncoderGL::get instead" );
			break;
		}

		switch( pixelFormat )
		{
		case PFG_RGBA16_FLOAT:
			type = GL_HALF_FLOAT;
			break;
		case PFG_RGBA32_UINT:
			type = GL_UNSIGNED_INT;
			break;
		case PFG_RGBA8_UNORM_SRGB:
			type = GL_UNSIGNED_INT_8_8_8_8_REV;
			break;
		case PFG_BC6H_UF16:
			format = GL_NONE;
			assert( false &&
					"This should never happen. Compressed formats must use "
					"EncoderGL::get instead" );
			break;
		}
	}
	//-------------------------------------------------------------------------
	GLuint EncoderGL::createTexture( const TextureParams &params )
	{
		const GLenum format = EncoderGL::get( params.format );

		const GLenum textureTarget = GL_TEXTURE_2D;

		GLuint textureName = 0;
		glGenTextures( 1u, &textureName );

		glBindTexture( textureTarget, textureName );
		glTexParameteri( textureTarget, GL_TEXTURE_BASE_LEVEL, 0 );
		glTexParameteri( textureTarget, GL_TEXTURE_MAX_LEVEL, 0 );
		glTexParameteri( textureTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( textureTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( textureTarget, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		glTexParameteri( textureTarget, GL_TEXTURE_MAX_LEVEL, params.numMipmaps - 1u );

		glTexStorage2D( GL_TEXTURE_2D, GLsizei( params.numMipmaps ), format,  //
						GLsizei( params.width ), GLsizei( params.height ) );

		if( params.debugName )
			ogreGlObjectLabel( GL_TEXTURE, textureName, params.debugName );

		return textureName;
	}
	//-------------------------------------------------------------------------
	void EncoderGL::destroyTexture( GLuint texName ) { glDeleteTextures( 1u, &texName ); }
	//-------------------------------------------------------------------------
	StagingTexture EncoderGL::createStagingTexture( uint32_t width, uint32_t height, PixelFormat format,
													bool forUpload )
	{
		const size_t sizeBytes = CpuImage::getSizeBytes( width, height, 1u, 1u, format );

		StagingTexture retVal;
		glGenBuffers( 1u, &retVal.bufferName );
		glBindBuffer( GL_COPY_WRITE_BUFFER, retVal.bufferName );

		GLbitfield flags = GL_MAP_PERSISTENT_BIT;
		if( forUpload )
			flags |= GL_MAP_WRITE_BIT;
		else
			flags |= GL_MAP_READ_BIT;

		glBufferStorage( GL_COPY_WRITE_BUFFER, (GLsizeiptr)sizeBytes, 0, flags );

		retVal.pixelFormat = format;
		retVal.rowLength = CpuImage::getSizeBytes( width, 1u, 1u, 1u, format );
		retVal.width = width;
		retVal.height = height;
		retVal.sizeBytes = sizeBytes;
		retVal.data = glMapBufferRange( GL_COPY_WRITE_BUFFER, 0, (GLsizeiptr)sizeBytes,
										GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT |
											GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_PERSISTENT_BIT );

		glBindBuffer( GL_COPY_WRITE_BUFFER, 0 );

		return retVal;
	}
	//-------------------------------------------------------------------------
	void EncoderGL::uploadStagingTexture( const StagingTexture &stagingTex, GLuint dstTexture )
	{
		const size_t bytesPerPixel = CpuImage::getBytesPerPixel( stagingTex.pixelFormat );

		glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, ( GLint )( stagingTex.rowLength / bytesPerPixel ) );
		glPixelStorei( GL_UNPACK_IMAGE_HEIGHT, (GLint)stagingTex.height );

		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, stagingTex.bufferName );
		glBindTexture( GL_TEXTURE_2D, dstTexture );

		glFlushMappedBufferRange( GL_PIXEL_UNPACK_BUFFER, 0, (GLsizeiptr)stagingTex.sizeBytes );

		GLenum format, type;
		EncoderGL::getFormatAndType( stagingTex.pixelFormat, format, type );

		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, (GLsizei)stagingTex.width, (GLsizei)stagingTex.height,
						 format, type, 0 );

		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
	//-------------------------------------------------------------------------
	void EncoderGL::destroyStagingTexture( const StagingTexture &stagingTex )
	{
		glBindBuffer( GL_COPY_WRITE_BUFFER, stagingTex.bufferName );
		glUnmapBuffer( GL_COPY_WRITE_BUFFER );
		glBindBuffer( GL_COPY_WRITE_BUFFER, 0 );
		glDeleteBuffers( 1u, &stagingTex.bufferName );
	}
	//-------------------------------------------------------------------------
	ComputePso EncoderGL::createComputePsoFromFile( const char *shaderFilename,
													const char *relativePath )
	{
		IncludeParser parser;
		parser.loadFromFile( shaderFilename, relativePath );

		ComputePso retVal = createComputePso( parser.getFinalSource() );
		return retVal;
	}
	//-------------------------------------------------------------------------
	ComputePso EncoderGL::createComputePso( const char *csShader )
	{
		ComputePso retVal;

		retVal.computeShader = glCreateShader( GL_COMPUTE_SHADER );
		glShaderSource( retVal.computeShader, 1, &csShader, 0 );
		glCompileShader( retVal.computeShader );

		retVal.computePso = glCreateProgram();
		glAttachShader( retVal.computePso, retVal.computeShader );
		glLinkProgram( retVal.computePso );

		return retVal;
	}
	//-------------------------------------------------------------------------
	void EncoderGL::destroyPso( ComputePso &pso )
	{
		glDeleteProgram( pso.computePso );
		glDeleteShader( pso.computeShader );
		pso.computePso = 0;
		pso.computeShader = 0;
	}
	//-------------------------------------------------------------------------
	void EncoderGL::bindComputePso( const ComputePso &pso ) { glUseProgram( pso.computePso ); }
	//-------------------------------------------------------------------------
	void EncoderGL::bindTexture( uint32_t slot, GLuint textureSrv )
	{
		glActiveTexture( static_cast<uint32_t>( GL_TEXTURE0 + slot ) );
		glBindTexture( GL_TEXTURE_2D, textureSrv );
		glActiveTexture( GL_TEXTURE0 );
	}
	//-------------------------------------------------------------------------
	void EncoderGL::bindUav( uint32_t slot, GLuint textureSrv, PixelFormat pixelFormat,
							 ResourceAccess::ResourceAccess access )
	{
		const GLenum format = EncoderGL::get( pixelFormat );
		GLenum accessGl;
		switch( access )
		{
		case ResourceAccess::Read:
			accessGl = GL_READ_ONLY;
			break;
		case ResourceAccess::Write:
			accessGl = GL_WRITE_ONLY;
			break;
		case ResourceAccess::ReadWrite:
			accessGl = GL_READ_WRITE;
			break;
		default:
			assert( false );
			accessGl = GL_READ_WRITE;
			break;
		}

		glBindImageTexture( slot, textureSrv, 0, GL_TRUE, 0, accessGl, format );
	}
}  // namespace betsy
