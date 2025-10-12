#include <glm/glm.hpp>

#include <CommandRecordBase.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <Pipeline.h>
#include <Sampler.h>
#include <ShaderParser.h>

#include "CascadedShadowMapsPass.h"
#include "EditorParams.h"
#include "ShadowMaskPassInterface.h"

class PreDepthPass;
class SharedGPUResources;

class ShadowMaskPassCascadedShadowMapping : public Wolf::CommandRecordBase, public ShadowMaskPassInterface
{
public:
	ShadowMaskPassCascadedShadowMapping(EditorParams* editorParams, const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::ResourceNonOwner<CascadedShadowMapsPass>& csmPass);

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	void addComputeShadowsShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const override;

	Wolf::Image* getOutput() override { return m_outputMask.get(); }
	Wolf::Semaphore* getSemaphore() const override { return Wolf::CommandRecordBase::getSemaphore(0 /* as this is never used as last semaphore it should be ok */); }
	void getConditionalBlocksToEnableWhenReadingMask(std::vector<std::string>& conditionalBlocks) const override {}
	Wolf::Image* getDenoisingPatternImage() override { return nullptr; }

	bool wasEnabledThisFrame() const override { return m_csmPass->wasEnabledThisFrame(); }

private:
	void createOutputImages(uint32_t width, uint32_t height);
	void createPipeline();
	void updateDescriptorSet() const;

	static float jitter();

	EditorParams* m_editorParams;

	/* Pipeline */
	std::unique_ptr<Wolf::ShaderParser> m_computeShaderParser;
	std::unique_ptr<Wolf::Pipeline> m_pipeline;
	std::unique_ptr<Wolf::Image> m_outputMask;

	/* Resources */
	Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
	std::unique_ptr<Wolf::DescriptorSet> m_descriptorSet;
	std::array<float, 16> m_noiseRotations;
	Wolf::ResourceNonOwner<PreDepthPass> m_preDepthPass;
	Wolf::ResourceNonOwner<CascadedShadowMapsPass> m_csmPass;
	struct ShadowUBData
	{
		glm::uvec2 viewportOffset;
		glm::uvec2 viewportSize;

		std::array<glm::mat4, CascadedShadowMapsPass::CASCADE_COUNT> cascadeMatrices;

		glm::vec4 cascadeSplits;

		std::array<glm::vec4, CascadedShadowMapsPass::CASCADE_COUNT / 2> cascadeScales;

		glm::uvec4 cascadeTextureSize;

		glm::float_t noiseRotation;
		glm::vec3 padding;
	};
	std::unique_ptr<Wolf::UniformBuffer> m_uniformBuffer;
	std::unique_ptr<Wolf::Sampler> m_shadowMapsSampler;

	// Noise
	static constexpr uint32_t NOISE_TEXTURE_SIZE_PER_SIDE = 128;
	static constexpr uint32_t NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE = 4;
	std::unique_ptr<Wolf::Image> m_noiseImage;
	std::unique_ptr<Wolf::Sampler> m_noiseSampler;
};