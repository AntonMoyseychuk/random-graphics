#ifndef REGISTERS_COMMON_H
#define REGISTERS_COMMON_H

#include <texture_incl.fx>


DECLARE_SRV_TEXTURE(sampler2D, GBUFFER_ALBEDO_TEX, 0, TEXTURE_FORMAT_RGBA8, COMMON_SMP_CLAMP_LINEAR_IDX);
DECLARE_SRV_TEXTURE(sampler2D, GBUFFER_NORMAL_TEX, 1, TEXTURE_FORMAT_RGBA8, COMMON_SMP_CLAMP_LINEAR_IDX);
DECLARE_SRV_TEXTURE(sampler2D, GBUFFER_SPECULAR_TEX, 2, TEXTURE_FORMAT_RGBA8, COMMON_SMP_CLAMP_LINEAR_IDX);

DECLARE_SRV_TEXTURE(sampler2D, COMMON_DEPTH_TEX, 3, TEXTURE_FORMAT_R32F, COMMON_SMP_CLAMP_LINEAR_IDX);


DECLARE_SRV_TEXTURE(sampler2D, TEST_TEXTURE, 4, TEXTURE_FORMAT_RGBA8, COMMON_SMP_REPEAT_NEAREST_IDX);

DECLARE_SRV_VARIABLE(float, COMMON_ELAPSED_TIME, 0, 0.f);

#endif