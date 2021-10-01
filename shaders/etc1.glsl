#version 430 core

#extension GL_ARB_shader_group_vote : require

// #include "/media/matias/Datos/SyntaxHighlightingMisc.h"

#include "CrossPlatformSettings_piece_all.glsl"
#include "UavCrossPlatform_piece_all.glsl"

#define cETC1ColorDeltaMin -4
#define cETC1ColorDeltaMax 3

#define cETC1IntenModifierNumBits 3
#define cETC1IntenModifierValues 8

#define FLT_MAX 340282346638528859811704183484516925440.0f

#define cLowQuality 0
#define cMediumQuality 1
#define cHighQuality 2

// 2 sets of 16 float3 (rgba8_unorm) for each ETC block
// We use rgba8_unorm encoding because it's 6kb vs 1.5kb of LDS. The former kills occupancy
shared uint g_srcPixelsBlock[16 * 2 * 4 * 4];
shared bool g_allPixelsEqual[4 * 4 * 4];

layout(location = 0) uniform float4 params;

#define p_quality params.x
#define p_scanDeltaAbsMin params.y
#define p_scanDeltaAbsMax params.z
#define p_maxRefinementTrials uint(params.w)

// srcTex MUST be in rgba8_unorm otherwise severe dataloss could happen (i.e. do NOT use sRGB)
// This is due to packing in g_srcPixelsBlock
uniform sampler2D srcTex;

layout(rg32ui, binding = 0) uniform restrict writeonly uimage2D dstTexture;

layout(std430, binding = 1) readonly restrict buffer globalBuffer
{
  // Given an ETC1 diff/inten_table/selector, and an 8-bit desired color, this table encodes the best
  // packed_color in the low byte, and the abs error in the high byte.
  float pBuff_etc1_inverse_lookup[2 * 8 * 4][256]; // [diff/inten_table/selector][desired_color]
  uint pBuff_color8_to_etc_block_config[];
};

#ifdef OUTPUT_ERROR
// Used by ETC2 to decide between this and T, H and P modes
layout(r32f, binding = 2) uniform restrict writeonly image2D dstError;
#endif

layout(local_size_x = 4, //
       local_size_y = 4, //
       local_size_z = 4) in;

const float4 g_etc1_inten_tables[cETC1IntenModifierValues] = {
    float4(-8, -2, 2, 8),     float4(-17, -5, 5, 17),   float4(-29, -9, 9, 29),     float4(-42, -13, 13, 42),
    float4(-60, -18, 18, 60), float4(-80, -24, 24, 80), float4(-106, -33, 33, 106), float4(-183, -47, 47, 183)};

struct PotentialSolution
{
  float3 rgbIntLS; // in range [0; limit]
  uint intenTable;

  // Has 8 selectors, 8-bits each.
  uint selectors[2];
  float error;
};

void storeErrorToLds(float error, uint threadId)
{
  g_srcPixelsBlock[threadId] = floatBitsToUint(error);
}
float loadErrorFromLds(uint threadId)
{
  return uintBitsToFloat(g_srcPixelsBlock[threadId]);
}

void storeBestBlockThreadIdxToLds(uint bestIdx, uint threadId)
{
  g_srcPixelsBlock[threadId] = bestIdx;
}
uint loadBestBlockThreadIdxFromLds(uint threadId)
{
  return g_srcPixelsBlock[threadId];
}

float3 getScaledColor(const float3 rgbInt, const bool bUseColor4)
{
  const float a = bUseColor4 ? 1.0f : 0.25f;
  const float b = bUseColor4 ? 16.0f : 8.0f;
  return floor(rgbInt * a) + rgbInt * b;
}

float calcError(float3 a, float3 b)
{
  float3 diff = a - b;
  return dot(diff, diff);
}

bool evaluate_solution(const uint subblockStart, const float3 blockRgbInt, const bool bUseColor4,
                       const float3 baseColor5, const bool constrainAgainstBaseColor5,
                       inout PotentialSolution bestSolution)
{
  PotentialSolution trialSolution;

  if (constrainAgainstBaseColor5)
  {
    const float3 d = blockRgbInt - baseColor5;

    if ((min3(d.r, d.g, d.b) < cETC1ColorDeltaMin) || //
        (max3(d.r, d.g, d.b) > cETC1ColorDeltaMax))
    {
      return false;
    }
  }

  const float3 baseColor = getScaledColor(blockRgbInt, bUseColor4);

  trialSolution.error = FLT_MAX;

  for (uint inten_table = 0u; inten_table < cETC1IntenModifierValues; ++inten_table)
  {
    const float4 pIntenTable = g_etc1_inten_tables[inten_table];

    float3 blockColorsInt[4];
    for (uint s = 0u; s < 4u; s++)
    {
      const float yd = pIntenTable[s];
      blockColorsInt[s] = clamp(baseColor + yd, 0.0f, 255.0f);
    }

    float total_error = 0;
    float4 tempSelectors[2];

    for (uint c = 0u; c < 8u; ++c)
    {
      const float3 srcPixel = unpackUnorm4x8(g_srcPixelsBlock[c + subblockStart]).xyz * 255.0f;

      float best_selector_index = 0;
      float best_error = calcError(srcPixel, blockColorsInt[0]);

      float trial_error = calcError(srcPixel, blockColorsInt[1]);
      if (trial_error < best_error)
      {
        best_error = trial_error;
        best_selector_index = 1;
      }

      trial_error = calcError(srcPixel, blockColorsInt[2]);
      if (trial_error < best_error)
      {
        best_error = trial_error;
        best_selector_index = 2;
      }

      trial_error = calcError(srcPixel, blockColorsInt[3]);
      if (trial_error < best_error)
      {
        best_error = trial_error;
        best_selector_index = 3;
      }

      tempSelectors[c >> 2u][c & 0x03u] = best_selector_index;

      total_error += best_error;
    }

    if (total_error < trialSolution.error)
    {
      trialSolution.error = total_error;
      trialSolution.intenTable = inten_table;
      trialSolution.selectors[0] = packUnorm4x8(tempSelectors[0] * (1.0f / 255.0f));
      trialSolution.selectors[1] = packUnorm4x8(tempSelectors[1] * (1.0f / 255.0f));
      // trialSolution.valid = true;
    }
  }

  trialSolution.rgbIntLS = blockRgbInt;

  bool success = false;
  if (trialSolution.error < bestSolution.error)
  {
    bestSolution = trialSolution;
    success = true;
  }

  return success;
}

float getScanDelta(const float iDeltai, const float scanDeltaAbsMin, const float scanDeltaAbsMax)
{
  const float numAbsDeltas = scanDeltaAbsMax - scanDeltaAbsMin + 1.0f;

  if (iDeltai < numAbsDeltas)
    return -scanDeltaAbsMax + iDeltai;
  else
    return scanDeltaAbsMin + iDeltai + (scanDeltaAbsMin == 0.0f ? 1.0f : 0.0f) - numAbsDeltas;
}

/**
@brief etc1_optimizer_compute
@param scanDeltaAbsMin
@param scanDeltaAbsMax
@param avgColour
@param avgColourLS
    Same as avgColour but in limit space, i.e. range [0; limit]
@param bUseColor4
@param baseColor5
@param constrainAgainstBaseColor5
@param bestSolution
@return
    True if we found a solution
    False if we failed to find any
*/
void etc1_optimizer_compute(const float scanDeltaAbsMin, const float scanDeltaAbsMax, const uint subblockStart,
                            const float3 avgColour, const float3 avgColourLS, const bool bUseColor4,
                            const float3 baseColor5, const bool constrainAgainstBaseColor5,
                            inout PotentialSolution bestSolution)
{
  // const float numSrcPixels = 8;  // Subblock of 2x4 or 4x2
  const float limit = bUseColor4 ? 15 : 31;

  const float scanDeltaSize = (scanDeltaAbsMax - scanDeltaAbsMin) * 2.0f + (scanDeltaAbsMin == 0.0f ? 1.0f : 2.0f);

  for (float di = 0; di < scanDeltaSize * scanDeltaSize * scanDeltaSize; ++di)
  {
    float3 xyzdi = float3(mod(di, scanDeltaSize),                        //
                          mod(floor(di / scanDeltaSize), scanDeltaSize), //
                          floor(di / (scanDeltaSize * scanDeltaSize)));
    float3 blockRgbInt = avgColourLS;
    blockRgbInt.x = getScanDelta(xyzdi.x, scanDeltaAbsMin, scanDeltaAbsMax);
    blockRgbInt.y = getScanDelta(xyzdi.y, scanDeltaAbsMin, scanDeltaAbsMax);
    blockRgbInt.z = getScanDelta(xyzdi.z, scanDeltaAbsMin, scanDeltaAbsMax);
    const uint maxRefinementTrials =
        (blockRgbInt.x == 0.0f && blockRgbInt.y == 0.0f && blockRgbInt.z == 0.0f) ? p_maxRefinementTrials : 2u;
    blockRgbInt += avgColourLS;

    if (blockRgbInt.x >= 0.0f && blockRgbInt.x <= limit && //
        blockRgbInt.y >= 0.0f && blockRgbInt.y <= limit && //
        blockRgbInt.z >= 0.0f && blockRgbInt.z <= limit)
    {
      bool bSuccess = evaluate_solution(subblockStart, blockRgbInt, bUseColor4, baseColor5, constrainAgainstBaseColor5,
                                        bestSolution);

      if (bSuccess)
      {
        // Now we have the input block, the avg. color of the input pixels, a set of trial
        // selector indices, and the block color+intensity index. Now, for each component,
        // attempt to refine the current solution by solving a simple linear equation.
        // So what this means: optimal_block_color = avg_input - avg_inten_delta
        // So the optimal block color can be computed by taking the average block color and
        // subtracting the current average of the intensity delta.
        // Unfortunately, optimal_block_color must then be quantized to 555 or 444 so it's
        // not always possible to improve matters using this formula. Also, the formula is
        // for unclamped intensity deltas. The actual implementation takes into account
        // clamping.

        float4 pSelectors[2];
        pSelectors[0] = unpackUnorm4x8(bestSolution.selectors[0]) * 255.0f;
        pSelectors[1] = unpackUnorm4x8(bestSolution.selectors[1]) * 255.0f;

        for (uint refinementTrial = 0u; refinementTrial < maxRefinementTrials && bSuccess; ++refinementTrial)
        {
          const float4 pIntenTable = g_etc1_inten_tables[bestSolution.intenTable];

          float3 deltaSum = float3(0.0f, 0.0f, 0.0f);
          float3 base_color = getScaledColor(bestSolution.rgbIntLS, bUseColor4);

          for (uint r = 0u; r < 8u; ++r)
          {
            const uint s = uint(pSelectors[r >> 2u][r & 0x03u] * 255.0f);
            const float yd = pIntenTable[s];
            // Compute actual delta being applied to each pixel,
            // taking into account clamping.
            deltaSum += clamp(base_color + yd, 0.0, 255.0) - base_color;
          }

          const float3 avgDelta = deltaSum * 0.125f;
          const float3 blockRgbInt1 = clamp(floor((avgColour - avgDelta) * limit * (1.0f / 255.0f) + 0.5f), 0, limit);

          if ((deltaSum.r == 0.0f && deltaSum.g == 0.0f && deltaSum.b == 0.0f) ||
              (blockRgbInt.r == blockRgbInt1.r && //
               blockRgbInt.g == blockRgbInt1.g && //
               blockRgbInt.b == blockRgbInt1.b) ||
              (bestSolution.rgbIntLS.r == blockRgbInt1.r && //
               bestSolution.rgbIntLS.g == blockRgbInt1.g && //
               bestSolution.rgbIntLS.b == blockRgbInt1.b) ||
              (avgColourLS.r == blockRgbInt1.r && //
               avgColourLS.g == blockRgbInt1.g && //
               avgColourLS.b == blockRgbInt1.b))
          {
            // Skip refinement
            bSuccess = false;
          }
          else
          {
            bSuccess = evaluate_solution(subblockStart, blockRgbInt1, bUseColor4, baseColor5,
                                         constrainAgainstBaseColor5, bestSolution);
          }
        } // refinementTrial
      }
    }
  }
}

/// Replaces g_selector_index_to_etc1[cETC1SelectorValues] from original code
/// const uint8 g_selector_index_to_etc1[cETC1SelectorValues] = { 3, 2, 0, 1 };
///
/// Input is in range [0; 4)
/// Return value is in range [0; 4)
float selector_index_to_etc1(float idx)
{
  return idx < 2.0f ? (3.0f - idx) : (idx - 2.0f);
}

// Packs solid color blocks efficiently using a set of small precomputed tables.
// For random 888 inputs, MSE results are better than Erricson's ETC1 packer in "slow" mode ~9.5% of
// the time, is slightly worse only ~.01% of the time, and is equal the rest of the time.
void pack_etc1_block_solid_color(const float3 srcPixel)
{
  float bestError = FLT_MAX, bestPacked_c1 = 0, bestPacked_c2 = 0;
  uint best_x = 0u, best_i = 0u;

  // For each possible 8-bit value, there is a precomputed list of diff/inten/selector
  // configurations that allow that 8-bit value to be encoded with no error.
  for (uint i = 0u; i < 3u && bestError > 0; ++i)
  {
    const float c0 = srcPixel[i];
    const uint c1 = uint(i == 0u ? srcPixel.y : (i == 1u ? srcPixel.z : srcPixel.x));
    const uint c2 = uint(i == 0u ? srcPixel.z : (i == 1u ? srcPixel.x : srcPixel.y));

    const float deltaRange = 1.0f;
    for (float delta = -deltaRange; delta <= deltaRange && bestError > 0; ++delta)
    {
      const float c_plus_delta = clamp(c0 + delta, 0.0f, 255.0f);

      uint tableOffset;
      if (c_plus_delta == 0.0f)
        tableOffset = 0u;
      else if (c_plus_delta == 255.0f)
        tableOffset = 33u;
      else
        tableOffset = uint(66.0f + (c_plus_delta - 1.0f) * 11.0f);

      const uint numTableEntries = pBuff_color8_to_etc_block_config[tableOffset];
      for (uint j = 0u; j < numTableEntries && bestError > 0; ++j)
      {
        const uint x = pBuff_color8_to_etc_block_config[tableOffset + j + 1u];

        float p1 = pBuff_etc1_inverse_lookup[x & 0xFFu][c1];
        float p2 = pBuff_etc1_inverse_lookup[x & 0xFFu][c2];

        const float3 diffVal = float3(c_plus_delta - c0,           //
                                      floor(p1 * (1.0f / 256.0f)), //
                                      floor(p2 * (1.0f / 256.0f)));
        const float trialError = dot(diffVal, diffVal);
        if (trialError < bestError)
        {
          bestError = trialError;
          best_x = x;
          bestPacked_c1 = mod(p1, 256.0f);
          bestPacked_c2 = mod(p2, 256.0f);
          best_i = i;
        }
      }
    }
  }

  const float diff = float(best_x & 1u);
  const float inten = float((best_x >> 1u) & 7u);

  float4 bytes;
  uint2 outputBytes;

  bytes.w = (inten + (inten * 8.0f)) * 4.0f + diff * 2.0f;

  const float bestPacked_c0 = float((best_x >> 8) & 255u);
  float3 bestPacked = float3(bestPacked_c0, bestPacked_c1, bestPacked_c2);
  if (diff != 0.0f)
    bestPacked = bestPacked * 8.0f;
  else
    bestPacked = bestPacked + bestPacked * 16.0f;

  bytes.xyz = best_i == 0u ? bestPacked.xyz : (best_i == 1u ? bestPacked.zxy : bestPacked.yzx);

  const float etc1Selector = selector_index_to_etc1(float((best_x >> 4u) & 3u));
  outputBytes.x = packUnorm4x8(bytes * (1.0f / 255.0f));
  outputBytes.y =
      (etc1Selector >= 2u ? 0xFFFFu : 0u) | ((etc1Selector == 1.0f || etc1Selector == 3.0f) ? 0xFFFF0000u : 0u);

  uint2 dstUV = gl_GlobalInvocationID.yz;
  imageStore(dstTexture, int2(dstUV), uint4(outputBytes.xy, 0u, 0u));

#ifdef OUTPUT_ERROR
  imageStore(dstError, int2(dstUV), float4(bestError, 0.0f, 0.0f, 0.0f));
#endif
}

void pack_etc1_block_solid_color_constrained(const float3 srcPixel, const bool bUseDiff, const float3 baseColor5,
                                             inout PotentialSolution bestSolution)
{
  float bestError = FLT_MAX, bestPacked_c1 = 0, bestPacked_c2 = 0;
  uint best_x = 0u, best_i = 0u;

  // For each possible 8-bit value, there is a precomputed list of diff/inten/selector
  // configurations that allow that 8-bit value to be encoded with no error.
  for (uint i = 0u; i < 3u && bestError > 0; ++i)
  {
    const float c0 = srcPixel[i];
    const uint c1 = uint(i == 0u ? srcPixel.y : (i == 1u ? srcPixel.z : srcPixel.x));
    const uint c2 = uint(i == 0u ? srcPixel.z : (i == 1u ? srcPixel.x : srcPixel.y));

    const float deltaRange = 1.0f;
    for (float delta = -deltaRange; delta <= deltaRange && bestError > 0; ++delta)
    {
      const float c_plus_delta = clamp(c0 + delta, 0.0f, 255.0f);

      uint tableOffset;
      if (c_plus_delta == 0.0f)
        tableOffset = 0u;
      else if (c_plus_delta == 255.0f)
        tableOffset = 33u;
      else
        tableOffset = uint(66.0f + (c_plus_delta - 1.0f) * 11.0f);

      const uint numTableEntries = pBuff_color8_to_etc_block_config[tableOffset];
      for (uint j = 0u; j < numTableEntries && bestError > 0; ++j)
      {
        const uint x = pBuff_color8_to_etc_block_config[tableOffset + j + 1u];
        const bool bCandidateUsesDiff = (x & 1u) != 0u;

        if (bUseDiff == bCandidateUsesDiff)
        {
          const float p1 = pBuff_etc1_inverse_lookup[x & 0xFFu][c1];
          const float p2 = pBuff_etc1_inverse_lookup[x & 0xFFu][c2];

          bool bIsValid = true;

          if (bUseDiff)
          {
            const float p0 = float((x >> 8u) & 255u);
            const float3 blockRgbInt = float3(p0, mod(p1, 256.0f), mod(p2, 256.0f));
            const float3 d = blockRgbInt - baseColor5;

            if ((min3(d.r, d.g, d.b) < cETC1ColorDeltaMin) || //
                (max3(d.r, d.g, d.b) > cETC1ColorDeltaMax))
            {
              bIsValid = false;
            }
          }

          const float3 diffVal = float3(c_plus_delta - c0,           //
                                        floor(p1 * (1.0f / 256.0f)), //
                                        floor(p2 * (1.0f / 256.0f)));
          const float trialError = dot(diffVal, diffVal);
          if (trialError < bestError && bIsValid)
          {
            bestError = trialError;
            best_x = x;
            bestPacked_c1 = mod(p1, 256.0f);
            bestPacked_c2 = mod(p2, 256.0f);
            best_i = i;
          }
        }
      }
    }
  }

  bestError *= 8.0f;

  if (bestError < bestSolution.error)
  {
    const float bestPacked_c0 = float((best_x >> 8) & 255u);
    float3 bestPacked = float3(bestPacked_c0, bestPacked_c1, bestPacked_c2);
    bestSolution.rgbIntLS = best_i == 0u ? bestPacked.xyz : (best_i == 1u ? bestPacked.zxy : bestPacked.yzx);

    bestSolution.selectors[0] = packUnorm4x8(float4(((best_x >> 4u) & 3u) * (1.0f / 255.0f)));
    bestSolution.selectors[1] = bestSolution.selectors[0];
    bestSolution.intenTable = (best_x >> 1u) & 7u;
  }
}

void main()
{
  // 4 threads per ETC block. Each pair of threads tries a different mode
  // combination (flip & useColor4); but we do not split subgroups into 2 threads because
  // when bUseColor4 = false, subblock1 depends on subblock0
  //
  // In Rich Geldrich's implementation the algorithm would stop if the 1st subgroup error
  // is already higher the combined error of the best pair of candidates.
  //
  // We can't do that because:
  //	1. There is no "best candidate" because they're all processed in parallel
  //	2. Even if there were, GPUs would very likely loop due to branch divergence
  //	   from different ETC blocks being processed by the same threadgroup
  const uint blockThreadId = gl_LocalInvocationID.x;

  const bool bFlip = (blockThreadId & 0x01u) != 0u;
  const bool bUseColor4 = (blockThreadId & 0x02u) != 0u;

  const uint2 pixelsToLoadBase = gl_GlobalInvocationID.yz << 2u;

  // We need to load a block of 4x4 pixels from src (16 pixels), and we have 4 threads per block
  // Thus each thread gets to load 4 pixels:
  //	4x1
  //	4x1
  //	4x1
  //	4x1
  // and distribute it into shared memory
  uint2 pixelsToLoad = pixelsToLoadBase;
  pixelsToLoad.y += blockThreadId;

  const float3 srcPixels0 = OGRE_Load2D(srcTex, int2(pixelsToLoad), 0).xyz;
  const float3 srcPixels1 = OGRE_Load2D(srcTex, int2(pixelsToLoad + uint2(1u, 0u)), 0).xyz;
  const float3 srcPixels2 = OGRE_Load2D(srcTex, int2(pixelsToLoad + uint2(2u, 0u)), 0).xyz;
  const float3 srcPixels3 = OGRE_Load2D(srcTex, int2(pixelsToLoad + uint2(3u, 0u)), 0).xyz;

  const uint blockStart = (gl_LocalInvocationID.y + gl_LocalInvocationID.z * 4u) * 32u;
  // const uint blockStart = ( gl_LocalInvocationIndex & 0x60u ) << 3u;
  // Linear (horizontal, aka flip = on)
  const uint subblockStartH = blockStart + 16u + (blockThreadId << 2u);
  g_srcPixelsBlock[subblockStartH + 0u] = packUnorm4x8(float4(srcPixels0, 1.0f));
  g_srcPixelsBlock[subblockStartH + 1u] = packUnorm4x8(float4(srcPixels1, 1.0f));
  g_srcPixelsBlock[subblockStartH + 2u] = packUnorm4x8(float4(srcPixels2, 1.0f));
  g_srcPixelsBlock[subblockStartH + 3u] = packUnorm4x8(float4(srcPixels3, 1.0f));

  // Non-linear (vertical, aka flip = off)
  const uint subblockOffset0 = 0u; // subblockIdx = 0
  const uint subblockStart0 = blockStart + subblockOffset0 + blockThreadId;
  g_srcPixelsBlock[subblockStart0 + 0u] = packUnorm4x8(float4(srcPixels0, 1.0f));
  g_srcPixelsBlock[subblockStart0 + 4u] = packUnorm4x8(float4(srcPixels1, 1.0f));

  const uint subblockOffset1 = 8u; // subblockIdx = 1
  const uint subblockStart1 = blockStart + subblockOffset1 + blockThreadId;
  g_srcPixelsBlock[subblockStart1 + 0u] = packUnorm4x8(float4(srcPixels2, 1.0f));
  g_srcPixelsBlock[subblockStart1 + 4u] = packUnorm4x8(float4(srcPixels3, 1.0f));

  const uint pixelsEqualBlockStart = (gl_LocalInvocationID.y + gl_LocalInvocationID.z * 4u) * 4u;
  bool bAllPixelsInThreadEqual = srcPixels0 == srcPixels1 && srcPixels0 == srcPixels2 && srcPixels0 == srcPixels3;
  g_allPixelsEqual[pixelsEqualBlockStart + blockThreadId] = bAllPixelsInThreadEqual;

  __sharedOnlyBarrier;

  if (blockThreadId == 0u)
  {
    // Thread 0 finishes checking if all pixels are equal
    for (uint i = 1u; i < 4u; ++i)
    {
      bAllPixelsInThreadEqual = bAllPixelsInThreadEqual && g_allPixelsEqual[pixelsEqualBlockStart + i];

      // All 4 pixels in thread 'i' could be equal. But they may be different from *our* pixels
      const float3 otherPixels = unpackUnorm4x8(g_srcPixelsBlock[blockStart + i]).xyz;
      bAllPixelsInThreadEqual = bAllPixelsInThreadEqual && srcPixels0 == otherPixels;
    }
    g_allPixelsEqual[pixelsEqualBlockStart] = bAllPixelsInThreadEqual;
  }

  __sharedOnlyBarrier;

  PotentialSolution results[2];
  for (uint i = 0u; i < 2u; ++i)
  {
    results[i].error = FLT_MAX;
    results[i].rgbIntLS = float3(0, 0, 0);
    results[i].intenTable = 0u;
    results[i].selectors[0] = 0u;
    results[i].selectors[1] = 0u;
  }

  // Check if all colours are equal. If so we can improve quality
  const bool bAllColoursEqual = g_allPixelsEqual[pixelsEqualBlockStart];
  if (bAllColoursEqual)
  {
    if (blockThreadId == 0u)
      pack_etc1_block_solid_color(srcPixels0 * 255.0f);
  }

  barrier(); // Ensure all threads are done reading from g_allPixelsEqual

  if (!bAllColoursEqual)
  {
    // Now check if the subblock has all pixels equal.
    // There are 4 subblocks to check (2 flipped and 2 non-flipped)
    // so we put all 4 threads to work on each.
    const uint subblockIdx = (blockThreadId & 0x1u);
    const bool bTmpFlip = blockThreadId >= 2u;

    const uint subblockStart = blockStart + (bTmpFlip ? 16u : 0u) + (subblockIdx == 0u ? 0u : 8u);

    bool allPixelsInSubblockEqual = true;

    const float3 srcPixel0 = unpackUnorm4x8(g_srcPixelsBlock[subblockStart]).xyz;
    for (uint i = 1u; i < 8u; ++i)
    {
      const float3 srcPixel = unpackUnorm4x8(g_srcPixelsBlock[i + subblockStart]).xyz;
      allPixelsInSubblockEqual = allPixelsInSubblockEqual && srcPixel0 == srcPixel;
    }

    // g_allPixelsEqual[start + 0] => subblockIdx = 0 + bFlip = false
    // g_allPixelsEqual[start + 1] => subblockIdx = 1 + bFlip = false
    // g_allPixelsEqual[start + 2] => subblockIdx = 0 + bFlip = true
    // g_allPixelsEqual[start + 3] => subblockIdx = 1 + bFlip = true
    g_allPixelsEqual[pixelsEqualBlockStart + blockThreadId] = allPixelsInSubblockEqual;
  }

  __sharedOnlyBarrier;

  // Unfortunately subblockIdx = 1 depends on subblockIdx = 0 when bUseColor4 = false
  for (uint subblockIdx = 0u; subblockIdx < 2u && !bAllColoursEqual; ++subblockIdx)
  {
    const bool constrainAgainstBaseColor5 = !bUseColor4 && subblockIdx != 0u;
    const float3 baseColor5 = results[0].rgbIntLS;

    const uint subblockStart = blockStart + (bFlip ? 16u : 0u) + (subblockIdx == 0u ? 0u : 8u);

    // Have all threads compute average.
    //
    // We don't do parallel reduction because it'd consume too
    // much LDS and we don't have access to __shfl_down
    float3 avgColour = float3(0, 0, 0);
    for (uint i = 0; i < 8u; ++i)
    {
      const float3 srcPixel = unpackUnorm4x8(g_srcPixelsBlock[i + subblockStart]).xyz;
      avgColour += srcPixel;
    }
    avgColour *= 1.0f / 8.0f;

    if (p_quality >= cMediumQuality && (subblockIdx > 0u || bUseColor4) &&
        g_allPixelsEqual[pixelsEqualBlockStart + subblockIdx + (bFlip ? 2u : 0u)])
    {
      pack_etc1_block_solid_color_constrained(unpackUnorm4x8(g_srcPixelsBlock[subblockStart]).xyz * 255.0f, bUseColor4,
                                              baseColor5, results[subblockIdx]);
    }

    const float limit = bUseColor4 ? 15 : 31;
    const float3 avgColourLS = clamp(floor(avgColour * limit + 0.5f), 0, limit);
    avgColour = avgColour * 255.0f;

    etc1_optimizer_compute(p_scanDeltaAbsMin, p_scanDeltaAbsMax, subblockStart, avgColour, avgColourLS, bUseColor4,
                           baseColor5, constrainAgainstBaseColor5, results[subblockIdx]);

    if (p_quality >= cMediumQuality)
    {
      const float refinement_error_thresh0 = 3000;
      const float refinement_error_thresh1 = 6000;
      if (results[subblockIdx].error > refinement_error_thresh0)
      {
        float scanDeltaAbsMin, scanDeltaAbsMax;
        if (p_quality == cMediumQuality)
        {
          scanDeltaAbsMin = 2;
          scanDeltaAbsMax = 3;
        }
        else
        {
          scanDeltaAbsMin = 5;
          if (results[subblockIdx].error > refinement_error_thresh1)
            scanDeltaAbsMax = 8;
          else
            scanDeltaAbsMax = 5;
        }

        etc1_optimizer_compute(scanDeltaAbsMin, scanDeltaAbsMax, subblockStart, avgColour, avgColourLS, bUseColor4,
                               baseColor5, constrainAgainstBaseColor5, results[subblockIdx]);
      }
    }
  }

  //
  // IMPORTANT: From now on g_srcPixelsBlock will be used for sync and
  // selecting the best result thus its old contents will become garbage
  //

  // Ensure execution reaches here; otherwise we could
  // overwrite g_srcPixelsBlock while it's in use
  barrier();

  // Save everyone's error to LDS
  storeErrorToLds(results[0].error + results[1].error, gl_LocalInvocationIndex);
  __sharedOnlyBarrier;

  if (blockThreadId == 0u)
  {
    float bestError = results[0].error + results[1].error;
    uint bestErrorThread = 0u;

    // Find the best result
    for (uint i = 1u; i < 4u; ++i)
    {
      float otherError = loadErrorFromLds(gl_LocalInvocationIndex + i);
      if (otherError < bestError)
      {
        bestError = otherError;
        bestErrorThread = i;
      }
    }

    // Write down which thread has the best error in our private LDS region
    storeBestBlockThreadIdxToLds(bestErrorThread, gl_LocalInvocationIndex);
  }

  __sharedOnlyBarrier;

  // Load threadIdx with best error which was written down by thread blockThreadId == 0u
  const uint blockThreadWithBestError = loadBestBlockThreadIdxFromLds(gl_LocalInvocationIndex & ~0x03u);

  if (blockThreadWithBestError == blockThreadId && !bAllColoursEqual)
  {
    // This thread was selected as the one with the best result.
    // Thus this one (the winner) will write to output
    float4 bytes;
    if (bUseColor4)
    {
      bytes.xyz = results[1].rgbIntLS + results[0].rgbIntLS * 16.0f;
    }
    else
    {
      float3 delta = results[1].rgbIntLS - results[0].rgbIntLS;
      if (delta.x < 0.0f)
        delta.x += 8.0f;
      if (delta.y < 0.0f)
        delta.y += 8.0f;
      if (delta.z < 0.0f)
        delta.z += 8.0f;

      bytes.xyz = delta + results[0].rgbIntLS * 8.0f;
    }

    bytes.w = (results[1].intenTable * 4.0f) + (results[0].intenTable * 32.0f) + (bUseColor4 ? 0.0f : 2.0f) +
              (bFlip ? 1.0f : 0.0f);

    uint2 outputBytes;
    outputBytes.x = packUnorm4x8(bytes * (1.0f / 255.0f));

    float selector0 = 0, selector1 = 0;
    if (bFlip)
    {
      // flipped:
      // { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 },
      // { 0, 1 }, { 1, 1 }, { 2, 1 }, { 3, 1 }
      //
      // { 0, 2 }, { 1, 2 }, { 2, 2 }, { 3, 2 },
      // { 0, 3 }, { 1, 3 }, { 2, 3 }, { 3, 3 }
      float4 pSelectors00 = unpackUnorm4x8(results[0].selectors[0]) * 255.0f;
      float4 pSelectors01 = unpackUnorm4x8(results[0].selectors[1]) * 255.0f;
      float4 pSelectors10 = unpackUnorm4x8(results[1].selectors[0]) * 255.0f;
      float4 pSelectors11 = unpackUnorm4x8(results[1].selectors[1]) * 255.0f;
      for (uint x = 4u; x-- > 0u;)
      {
        float b, bHalf;
        b = selector_index_to_etc1(pSelectors11[x]);
        bHalf = floor(b * 0.5f);
        selector0 = (selector0 * 2.0f) + (b - bHalf * 2.0f);
        selector1 = (selector1 * 2.0f) + (bHalf);

        b = selector_index_to_etc1(pSelectors10[x]);
        bHalf = floor(b * 0.5f);
        selector0 = (selector0 * 2.0f) + (b - bHalf * 2.0f);
        selector1 = (selector1 * 2.0f) + (bHalf);

        b = selector_index_to_etc1(pSelectors01[x]);
        bHalf = floor(b * 0.5f);
        selector0 = (selector0 * 2.0f) + (b - bHalf * 2.0f);
        selector1 = (selector1 * 2.0f) + (bHalf);

        b = selector_index_to_etc1(pSelectors00[x]);
        bHalf = floor(b * 0.5f);
        selector0 = (selector0 * 2.0f) + (b - bHalf * 2.0f);
        selector1 = (selector1 * 2.0f) + (bHalf);
      }
    }
    else
    {
      // non-flipped:
      // { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 },
      // { 1, 0 }, { 1, 1 }, { 1, 2 }, { 1, 3 }
      //
      // { 2, 0 }, { 2, 1 }, { 2, 2 }, { 2, 3 },
      // { 3, 0 }, { 3, 1 }, { 3, 2 }, { 3, 3 }
      for (uint subblockIdx = 2u; subblockIdx-- > 0u;)
      {
        for (uint i = 2u; i-- > 0u;)
        {
          float4 pSelectors = unpackUnorm4x8(results[subblockIdx].selectors[i]) * 255.0f;

          float b, bHalf;
          b = selector_index_to_etc1(pSelectors[3]);
          bHalf = floor(b * 0.5f);
          selector0 = (selector0 * 2.0f) + (b - bHalf * 2.0f);
          selector1 = (selector1 * 2.0f) + (bHalf);

          b = selector_index_to_etc1(pSelectors[2]);
          bHalf = floor(b * 0.5f);
          selector0 = (selector0 * 2.0f) + (b - bHalf * 2.0f);
          selector1 = (selector1 * 2.0f) + (bHalf);

          b = selector_index_to_etc1(pSelectors[1]);
          bHalf = floor(b * 0.5f);
          selector0 = (selector0 * 2.0f) + (b - bHalf * 2.0f);
          selector1 = (selector1 * 2.0f) + (bHalf);

          b = selector_index_to_etc1(pSelectors[0]);
          bHalf = floor(b * 0.5f);
          selector0 = (selector0 * 2.0f) + (b - bHalf * 2.0f);
          selector1 = (selector1 * 2.0f) + (bHalf);
        }
      }
    }

#ifdef CHECKING_ORDER
    bytes.x = floor(selector1 / 256.0f);
    bytes.y = selector1 - floor(selector1 / 256.0f) * 256.0f;
    bytes.z = floor(selector0 / 256.0f);
    bytes.w = selector0 - floor(selector0 / 256.0f) * 256.0f;
    outputBytes.y = packUnorm4x8(bytes * (1.0f / 255.0f));
#else
    outputBytes.y = packUnorm2x16(float2(selector1, selector0) * (1.0f / 65535.0f));
    outputBytes.y = packUnorm4x8(unpackUnorm4x8(outputBytes.y).yxwz); // Byteswap
#endif

    uint2 dstUV = gl_GlobalInvocationID.yz;
    imageStore(dstTexture, int2(dstUV), uint4(outputBytes.xy, 0u, 0u));
#ifdef OUTPUT_ERROR
    imageStore(dstError, int2(dstUV), float4(results[0].error + results[1].error, 0.0f, 0.0f, 0.0f));
#endif
  }
}
