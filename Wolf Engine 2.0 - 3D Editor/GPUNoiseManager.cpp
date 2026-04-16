#include "GPUNoiseManager.h"

#include <random>

#include <Image.h>
#include <Sampler.h>

GPUNoiseManager::GPUNoiseManager()
{
    initVogelDiskNoise();
}

void GPUNoiseManager::initVogelDiskNoise()
{
    Wolf::CreateImageInfo noiseImageCreateInfo;
    noiseImageCreateInfo.extent = { NOISE_TEXTURE_SIZE_PER_SIDE, NOISE_TEXTURE_SIZE_PER_SIDE, NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE * NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE };
    noiseImageCreateInfo.format = Wolf::Format::R32G32_SFLOAT;
    noiseImageCreateInfo.mipLevelCount = 1;
    noiseImageCreateInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST | Wolf::ImageUsageFlagBits::SAMPLED;
    m_vogelDiskNoiseImage.reset(Wolf::Image::createImage(noiseImageCreateInfo));

    std::vector<float> noiseData(static_cast<size_t>(NOISE_TEXTURE_SIZE_PER_SIDE) * NOISE_TEXTURE_SIZE_PER_SIDE * NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE * NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE * 2u);
    for (uint32_t v = 0; v < 4; ++v)
    {
        for (uint32_t u = 0; u < 4; u++)
        {
            std::default_random_engine generator(u + v * 4);
            std::uniform_real_distribution distrib(-0.5f, 0.5f);
            auto jitter = [&]()
            {
                return distrib(generator);
            };

            for (uint32_t texY = 0; texY < NOISE_TEXTURE_SIZE_PER_SIDE; ++texY)
            {
                for (uint32_t texX = 0; texX < NOISE_TEXTURE_SIZE_PER_SIDE; ++texX)
                {
                    float x = (static_cast<float>(u) + 0.5f + jitter()) / static_cast<float>(NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE);
                    float y = (static_cast<float>(v) + 0.5f + jitter()) / static_cast<float>(NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE);

                    const uint32_t patternIdx = u + v * NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE;
                    const uint32_t idx = texX + texY * NOISE_TEXTURE_SIZE_PER_SIDE + patternIdx * (NOISE_TEXTURE_SIZE_PER_SIDE * NOISE_TEXTURE_SIZE_PER_SIDE);

                    constexpr float PI = 3.14159265358979323846264338327950288f;
                    noiseData[2 * idx] = sqrtf(y) * cosf(2.0f * PI * x);
                    noiseData[2 * idx + 1] = sqrtf(y) * sinf(2.0f * PI * x);
                }
            }
        }
    }
    m_vogelDiskNoiseImage->copyCPUBuffer(reinterpret_cast<unsigned char*>(noiseData.data()), Wolf::Image::SampledInFragmentShader());

    m_vogelDiskNoiseSampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1.0f, VK_FILTER_NEAREST));
}