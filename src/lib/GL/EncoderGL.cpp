
#include "betsy/GL/EncoderGL.h"

#include "betsy/CpuImage.h"

#include <assert.h>
#include <string.h>

//#define DEBUG_LINK_ERRORS

#if defined(DEBUG_LINK_ERRORS)
#include <stdio.h>
#endif

namespace betsy
{
extern bool g_hasDebugObjectLabel;
bool g_hasDebugObjectLabel = false;
void setObjectLabel(GLenum identifier, GLuint name, const GLchar *label)
{
  if (g_hasDebugObjectLabel)
    glObjectLabel(identifier, name, (GLsizei)strlen(label), label);
}
//-------------------------------------------------------------------------
TextureParams::TextureParams(uint32_t _width, uint32_t _height, gli::format _format, const char *_debugName,
                             uint32_t _flags, uint32_t _depthOrSlices, uint8_t _numMipmaps)
    : width(_width), height(_height), depthOrSlices(_depthOrSlices), numMipmaps(_numMipmaps), flags(_flags),
      format(_format), debugName(_debugName)
{
}
//-------------------------------------------------------------------------
StagingTexture::StagingTexture()
    : bufferName(0), bytesPerRow(0), width(0), height(0), pixelFormat(gli::FORMAT_RGBA16_SFLOAT_PACK16), data(0),
      sizeBytes(0)
{
}
//-------------------------------------------------------------------------
ComputePso::ComputePso() : computeShader(0), computePso(0)
{
}
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
GLenum EncoderGL::get(gli::format pixFmt)
{
  gli::gl GL(gli::gl::PROFILE_GL33);
  gli::gl::format const glFormat = GL.translate(pixFmt, gli::swizzles());
  return glFormat.Internal;
}

//-----------------------------------------------------------------------------------
GLenum EncoderGL::getBaseFormat(gli::format pixFmt)
{
  GLenum baseFormat = GL_NONE;

  switch (pixFmt)
  {
  case gli::FORMAT_RGBA_ETC2_UNORM_BLOCK8:
  case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16:
  case gli::FORMAT_RGBA8_UNORM_PACK8:
  case gli::FORMAT_RGBA16_SFLOAT_PACK16:
  case gli::FORMAT_RGBA32_UINT_PACK32:
    baseFormat = GL_RGBA;
    break;
  case gli::FORMAT_RGB_ETC2_UNORM_BLOCK8:
  case gli::FORMAT_RGB_BP_UFLOAT_BLOCK16:
  case gli::FORMAT_RGB_ETC_UNORM_BLOCK8:
  case gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8:
    baseFormat = GL_RGB;
    break;
  case gli::FORMAT_RG_EAC_UNORM_BLOCK16:
  case gli::FORMAT_RG_ATI2N_UNORM_BLOCK16:
    baseFormat = GL_RG;
    break;
  case gli::FORMAT_R_EAC_UNORM_BLOCK8:
  case gli::FORMAT_R_ATI1N_UNORM_BLOCK8:
    baseFormat = GL_RED;
    break;
  default:
    printf("Missing: %i\n", pixFmt);
  }

  return baseFormat;
}

//-----------------------------------------------------------------------------------
void EncoderGL::getFormatAndType(gli::format pixFmt, GLenum &format, GLenum &type)
{
  gli::gl GL(gli::gl::PROFILE_GL33);
  gli::gl::format const glFormat = GL.translate(pixFmt, gli::swizzles());
  format = glFormat.External;
  type = glFormat.Type;
}

//-------------------------------------------------------------------------
GLuint EncoderGL::createTexture(const TextureParams &params)
{
  const GLenum format = EncoderGL::get(params.format);

  const GLenum textureTarget = params.depthOrSlices > 1u ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

  GLuint textureName = 0;
  glGenTextures(1u, &textureName);

  glBindTexture(textureTarget, textureName);
  glTexParameteri(textureTarget, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(textureTarget, GL_TEXTURE_MAX_LEVEL, 0);
  glTexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(textureTarget, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(textureTarget, GL_TEXTURE_MAX_LEVEL, params.numMipmaps - 1u);

  if (params.depthOrSlices > 1u)
  {
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, GLsizei(params.numMipmaps), format, //
                   GLsizei(params.width), GLsizei(params.height), GLsizei(params.depthOrSlices));
  }
  else
  {
    glTexStorage2D(GL_TEXTURE_2D, GLsizei(params.numMipmaps), format, //
                   GLsizei(params.width), GLsizei(params.height));
  }

  if (params.debugName)
    setObjectLabel(GL_TEXTURE, textureName, params.debugName);

  return textureName;
}
//-------------------------------------------------------------------------
void EncoderGL::destroyTexture(GLuint texName)
{
  glDeleteTextures(1u, &texName);
}
//-------------------------------------------------------------------------
StagingTexture EncoderGL::createStagingTexture(uint32_t width, uint32_t height, gli::format pixFmt, bool forUpload)
{
  const size_t sizeBytes = CpuImage::getSizeBytes(width, height, 1u, 1u, pixFmt);

  StagingTexture retVal;
  glGenBuffers(1u, &retVal.bufferName);
  glBindBuffer(GL_COPY_WRITE_BUFFER, retVal.bufferName);

  GLbitfield flags = GL_MAP_PERSISTENT_BIT;
  if (forUpload)
    flags |= GL_MAP_WRITE_BIT;
  else
    flags |= GL_MAP_READ_BIT;

  glBufferStorage(GL_COPY_WRITE_BUFFER, (GLsizeiptr)sizeBytes, 0, flags);

  GLbitfield mapFlags = GL_MAP_PERSISTENT_BIT;

  if (forUpload)
    mapFlags |= GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
  else
    mapFlags |= GL_MAP_READ_BIT;

  retVal.pixelFormat = pixFmt;
  retVal.bytesPerRow = CpuImage::getSizeBytes(width, 1u, 1u, 1u, pixFmt);
  retVal.width = width;
  retVal.height = height;
  retVal.sizeBytes = sizeBytes;
  retVal.data = glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, (GLsizeiptr)sizeBytes, mapFlags);

  glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

  return retVal;
}
//-------------------------------------------------------------------------
void EncoderGL::uploadStagingTexture(const StagingTexture &stagingTex, GLuint dstTexture)
{
  const size_t bytesPerPixel = CpuImage::getBytesPerPixel(stagingTex.pixelFormat);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(stagingTex.bytesPerRow / bytesPerPixel));
  glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, (GLint)stagingTex.height);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, stagingTex.bufferName);
  glBindTexture(GL_TEXTURE_2D, dstTexture);

  glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, (GLsizeiptr)stagingTex.sizeBytes);

  GLenum format, type;
  EncoderGL::getFormatAndType(stagingTex.pixelFormat, format, type);

  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)stagingTex.width, (GLsizei)stagingTex.height, format, type, 0);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
}
//-------------------------------------------------------------------------
void EncoderGL::downloadStagingTexture(GLuint srcTexture, const StagingTexture &stagingTex)
{
  const size_t bytesPerPixel = CpuImage::getBytesPerPixel(stagingTex.pixelFormat);

  const GLint rowLength = bytesPerPixel > 0 ? GLint(stagingTex.bytesPerRow / bytesPerPixel) : 0;
  const GLint imageHeight = (stagingTex.bytesPerRow > 0) ? GLint(stagingTex.height) : 0;

  glPixelStorei(GL_PACK_ALIGNMENT, 4);
  glPixelStorei(GL_PACK_ROW_LENGTH, rowLength);
  glPixelStorei(GL_PACK_IMAGE_HEIGHT, imageHeight);

  const GLint mipLevel = 0;

  glBindTexture(GL_TEXTURE_2D, srcTexture);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, stagingTex.bufferName);

  // We can use glGetTexImage & glGetCompressedTexImage (cubemaps need a special path)
  if (!gli::is_compressed(stagingTex.pixelFormat))
  {
    GLenum format, type;
    EncoderGL::getFormatAndType(stagingTex.pixelFormat, format, type);
    glGetTexImage(GL_TEXTURE_2D, mipLevel, format, type, 0);
  }
  else
  {
    glGetCompressedTexImage(GL_TEXTURE_2D, mipLevel, 0);
  }
}
//-------------------------------------------------------------------------
void EncoderGL::destroyStagingTexture(const StagingTexture &stagingTex)
{
  glBindBuffer(GL_COPY_WRITE_BUFFER, stagingTex.bufferName);
  glUnmapBuffer(GL_COPY_WRITE_BUFFER);
  glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
  glDeleteBuffers(1u, &stagingTex.bufferName);
}
//-------------------------------------------------------------------------
GLuint EncoderGL::createUavBuffer(size_t sizeBytes, void *initialData)
{
  GLuint bufferName;
  glGenBuffers(1u, &bufferName);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferName);
  glBufferStorage(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)sizeBytes, initialData, 0u);
  return bufferName;
}
//-------------------------------------------------------------------------
void EncoderGL::destroyUavBuffer(GLuint bufferName)
{
  glDeleteBuffers(1u, &bufferName);
}
//-------------------------------------------------------------------------
ComputePso EncoderGL::createComputePso(const char *csShader)
{
  ComputePso retVal;

  retVal.computeShader = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(retVal.computeShader, 1, &csShader, 0);
  glCompileShader(retVal.computeShader);

#ifdef DEBUG_LINK_ERRORS
  {
    GLsizei logLength = 0;
    char infoLog[1024];
    glGetShaderiv(retVal.computeShader, GL_INFO_LOG_LENGTH, &logLength);
    glGetShaderInfoLog(retVal.computeShader, sizeof(infoLog), &logLength, infoLog);
    if (logLength > 0)
      printf("%s\n", infoLog);
  }
#endif

  retVal.computePso = glCreateProgram();
  glAttachShader(retVal.computePso, retVal.computeShader);
  glLinkProgram(retVal.computePso);

#ifdef DEBUG_LINK_ERRORS
  {
    GLsizei logLength = 0;
    char infoLog[1024];
    glGetProgramInfoLog(retVal.computePso, sizeof(infoLog), &logLength, infoLog);
    if (logLength > 0)
      printf("%s\n", infoLog);
  }
#endif

  return retVal;
}
//-------------------------------------------------------------------------
void EncoderGL::destroyPso(ComputePso &pso)
{
  glDeleteProgram(pso.computePso);
  glDeleteShader(pso.computeShader);
  pso.computePso = 0;
  pso.computeShader = 0;
}
//-------------------------------------------------------------------------
void EncoderGL::bindComputePso(const ComputePso &pso)
{
  glUseProgram(pso.computePso);
}
//-------------------------------------------------------------------------
void EncoderGL::bindTexture(uint32_t slot, GLuint textureSrv)
{
  glActiveTexture(static_cast<uint32_t>(GL_TEXTURE0 + slot));
  glBindTexture(GL_TEXTURE_2D, textureSrv);
  glActiveTexture(GL_TEXTURE0);
}
//-------------------------------------------------------------------------
void EncoderGL::bindUav(uint32_t slot, GLuint textureSrv, gli::format pixFmt, ResourceAccess::ResourceAccess access)
{
  const GLenum format = EncoderGL::get(pixFmt);
  GLenum accessGl;
  switch (access)
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
    assert(false);
    accessGl = GL_READ_WRITE;
    break;
  }

  glBindImageTexture(slot, textureSrv, 0, GL_TRUE, 0, accessGl, format);
}
//-------------------------------------------------------------------------
void EncoderGL::bindUavBuffer(uint32_t slot, GLuint buffer, size_t offset, size_t bufferSize)
{
  glBindBufferRange(GL_SHADER_STORAGE_BUFFER, slot, buffer, (GLintptr)offset, (GLintptr)bufferSize);
}
} // namespace betsy
