
#include "betsy/GL/EncoderBC4.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>

#include "betsy/CpuImage.h"
#include "betsy/GL/Shaders.h"

namespace betsy
{
EncoderBC4::EncoderBC4()
    : m_width(0), m_height(0), m_srcTexture(0), m_stitchedTarget(0), m_dstTexture(0), m_encodeSNorm(false)
{
  memset(m_bc4TargetRes, 0, sizeof(m_bc4TargetRes));
}
//-------------------------------------------------------------------------
EncoderBC4::~EncoderBC4()
{
  assert(!m_srcTexture && "deinitResources not called!");
}
//-------------------------------------------------------------------------
void EncoderBC4::initResources(const CpuImage &srcImage, const bool redGreen, const bool encodeSNorm)
{
  m_width = srcImage.width;
  m_height = srcImage.height;

  m_encodeSNorm = encodeSNorm;

  const auto srcFormat = srcImage.format;

  m_srcTexture = createTexture(TextureParams(m_width, m_height, srcFormat, "m_srcTexture"));

  m_bc4TargetRes[0] = createTexture(TextureParams(getBlockWidth(), getBlockHeight(), gli::FORMAT_RG32_UINT_PACK32,
                                                  "m_bc4TargetRes[0]", TextureFlags::Uav));
  if (redGreen)
  {
    m_bc4TargetRes[1] = createTexture(TextureParams(getBlockWidth(), getBlockHeight(), gli::FORMAT_RG32_UINT_PACK32,
                                                    "m_bc4TargetRes[1]", TextureFlags::Uav));
  }

  m_bc4Pso = createComputePso(bc4_glsl);

  if (redGreen)
  {
    m_stitchedTarget = createTexture(TextureParams(getBlockWidth(), getBlockHeight(), gli::FORMAT_RGBA32_UINT_PACK32,
                                                   "m_stitchedTarget", TextureFlags::Uav));
    m_stitchPso = createComputePso(etc2_rgba_stitch_glsl);

    m_dstTexture = createTexture(TextureParams(
        m_width, m_height, m_encodeSNorm ? gli::FORMAT_RG_ATI2N_SNORM_BLOCK16 : gli::FORMAT_RG_ATI2N_UNORM_BLOCK16,
        "m_dstTexture"));
  }
  else
  {
    m_dstTexture = createTexture(TextureParams(
        m_width, m_height, m_encodeSNorm ? gli::FORMAT_R_ATI1N_SNORM_BLOCK8 : gli::FORMAT_R_ATI1N_UNORM_BLOCK8,
        "m_dstTexture"));
  }

  StagingTexture stagingTex = createStagingTexture(m_width, m_height, srcImage.format, true);
  memcpy(stagingTex.data, srcImage.data, stagingTex.sizeBytes);
  uploadStagingTexture(stagingTex, m_srcTexture);
  destroyStagingTexture(stagingTex);
}
//-------------------------------------------------------------------------
void EncoderBC4::deinitResources()
{
  if (m_dstTexture)
  {
    destroyTexture(m_dstTexture);
    m_dstTexture = 0;
  }
  if (m_stitchedTarget)
  {
    destroyTexture(m_stitchedTarget);
    m_stitchedTarget = 0;
  }
  for (size_t i = 0u; i < 2u; ++i)
  {
    if (m_bc4TargetRes[i])
    {
      destroyTexture(m_bc4TargetRes[i]);
      m_bc4TargetRes[i] = 0;
    }
  }
  destroyTexture(m_srcTexture);
  m_srcTexture = 0;

  if (m_downloadStaging.bufferName)
    destroyStagingTexture(m_downloadStaging);

  destroyPso(m_bc4Pso);
  if (m_stitchPso.computePso)
    destroyPso(m_stitchPso);
}
//-------------------------------------------------------------------------
void EncoderBC4::execute01()
{
  bindTexture(0u, m_srcTexture);
  bindComputePso(m_bc4Pso);

  const size_t numChannels = m_bc4TargetRes[1] ? 2u : 1u;
  for (size_t i = 0u; i < numChannels; ++i)
  {
    bindUav(0u, m_bc4TargetRes[i], gli::FORMAT_RG32_UINT_PACK32, ResourceAccess::Write);

    // p_channelIdx, p_useSNorm
    glUniform2f(0, i == 0u ? 0.0f : 1.0f, m_encodeSNorm ? 1.0f : 0.0f);

    glDispatchCompute(1u, //
                      alignToNextMultiple(m_width, 16u) / 16u, alignToNextMultiple(m_height, 16u) / 16u);
  }
}
//-------------------------------------------------------------------------
void EncoderBC4::execute02()
{
  if (!m_bc4TargetRes[1])
  {
    // This step is only relevant when doing BC5
    return;
  }

  glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
  bindTexture(0u, m_bc4TargetRes[1]);
  bindTexture(1u, m_bc4TargetRes[0]);
  bindUav(0u, m_stitchedTarget, gli::FORMAT_RGBA32_UINT_PACK32, ResourceAccess::Write);
  bindComputePso(m_stitchPso);
  glDispatchCompute(alignToNextMultiple(m_width, 32u) / 32u, alignToNextMultiple(m_height, 32u) / 32u, 1u);
}
//-------------------------------------------------------------------------
void EncoderBC4::execute03()
{
  // It's unclear which of these 2 barrier bits GL wants in order for glCopyImageSubData to work
  glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  // Copy "8x8" PFG_RG32_UINT   -> 32x32 PFG_BC4_UNORM/SNORM
  // Copy "8x8" PFG_RGBA32_UINT -> 32x32 PFG_BC5_UNORM/SNORM
  glCopyImageSubData(m_bc4TargetRes[1] ? m_stitchedTarget : m_bc4TargetRes[0], //
                     GL_TEXTURE_2D, 0, 0, 0, 0,                                //
                     m_dstTexture, GL_TEXTURE_2D, 0, 0, 0, 0,                  //
                     (GLsizei)(getBlockWidth()), (GLsizei)(getBlockHeight()), 1);
}
//-------------------------------------------------------------------------
void EncoderBC4::startDownload()
{
  glMemoryBarrier(GL_PIXEL_BUFFER_BARRIER_BIT);

  if (m_downloadStaging.bufferName)
    destroyStagingTexture(m_downloadStaging);
  m_downloadStaging =
      createStagingTexture(getBlockWidth(), getBlockHeight(),
                           m_bc4TargetRes[1] ? gli::FORMAT_RGBA32_UINT_PACK32 : gli::FORMAT_RG32_UINT_PACK32, false);
  downloadStagingTexture(m_bc4TargetRes[1] ? m_stitchedTarget : m_bc4TargetRes[0], m_downloadStaging);
}
//-------------------------------------------------------------------------
void EncoderBC4::downloadTo(CpuImage &outImage)
{
  glFinish();
  outImage.width = m_width;
  outImage.height = m_height;
  outImage.format = m_bc4TargetRes[1] ? gli::FORMAT_RG_ATI2N_UNORM_BLOCK16 : gli::FORMAT_R_ATI1N_UNORM_BLOCK8;
  outImage.data = reinterpret_cast<uint8_t *>(m_downloadStaging.data);
}
} // namespace betsy
