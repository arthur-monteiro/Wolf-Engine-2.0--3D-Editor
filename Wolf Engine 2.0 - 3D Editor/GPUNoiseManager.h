#pragma once

#include <cstdint>

#include "ResourceUniqueOwner.h"

namespace Wolf
{
    class Sampler;
    class Image;
}

class GPUNoiseManager
{
public:
    GPUNoiseManager();

    Wolf::ResourceNonOwner<Wolf::Image> getVogelDiskImage() const { return m_vogelDiskNoiseImage.createNonOwnerResource(); }
    Wolf::ResourceNonOwner<Wolf::Sampler> getVogelDiskSampler() const { return m_vogelDiskNoiseSampler.createNonOwnerResource(); }

private:
    void initVogelDiskNoise();
    static constexpr uint32_t NOISE_TEXTURE_SIZE_PER_SIDE = 128;
    static constexpr uint32_t NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE = 4;
    Wolf::ResourceUniqueOwner<Wolf::Image> m_vogelDiskNoiseImage;
    Wolf::ResourceUniqueOwner<Wolf::Sampler> m_vogelDiskNoiseSampler;
};
