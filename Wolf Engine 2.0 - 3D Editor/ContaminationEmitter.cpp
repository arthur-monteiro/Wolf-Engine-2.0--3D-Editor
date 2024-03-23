#include "ContaminationEmitter.h"

#include "EditorParamsHelper.h"

ContaminationEmitter::ContaminationEmitter(const std::function<void(ComponentInterface*)>& requestReloadCallback)
{
	m_requestReloadCallback = requestReloadCallback;

	Wolf::CreateImageInfo createImageInfo{};
	createImageInfo.extent = { CONTAMINATION_IDS_IMAGE_SIZE, CONTAMINATION_IDS_IMAGE_SIZE, CONTAMINATION_IDS_IMAGE_SIZE };
	createImageInfo.format = VK_FORMAT_R8_UINT;
	createImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	createImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	createImageInfo.mipLevelCount = 1;
	m_contaminationIdsImage.reset(new Wolf::Image(createImageInfo));
	m_contaminationIdsImage->setImageLayout(Wolf::Image::SampledInFragmentShader());

	m_descriptorSetLayoutGenerator.addCombinedImageSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	m_descriptorSetLayout.reset(new Wolf::DescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

	Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());
	m_sampler.reset(new Wolf::Sampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_NEAREST));
	descriptorSetGenerator.setCombinedImageSampler(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_contaminationIdsImage->getDefaultImageView(), *m_sampler);

	m_descriptorSet.reset(new Wolf::DescriptorSet(m_descriptorSetLayout->getDescriptorSetLayout(), Wolf::UpdateRate::NEVER));
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void ContaminationEmitter::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams<ContaminationMaterial>(jsonReader, ID, m_editorParams);
}

void ContaminationEmitter::activateParams()
{
	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void ContaminationEmitter::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

ContaminationEmitter::ContaminationMaterial::ContaminationMaterial() : ParameterGroupInterface(ContaminationEmitter::TAB), m_material(ContaminationEmitter::TAB, "")
{
	m_name = DEFAULT_NAME;
}

std::span<EditorParamInterface*> ContaminationEmitter::ContaminationMaterial::getAllParams()
{
	return m_material.getAllParams();
}

std::span<EditorParamInterface* const> ContaminationEmitter::ContaminationMaterial::getAllConstParams() const
{
	return m_material.getAllConstParams();
}

bool ContaminationEmitter::ContaminationMaterial::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}

void ContaminationEmitter::onMaterialAdded()
{
	m_contaminationMaterials.back().subscribe(this, [this]() { /* nothing to do */ });
	m_requestReloadCallback(this);
}

void ContaminationEmitter::onFillSceneWithValueChanged() const
{
	if (m_fillSceneWithValue.isEnabled())
	{
		const uint8_t value = static_cast<uint8_t>(m_fillSceneWithValue);
		const std::vector<uint8_t> bufferWithValue(static_cast<size_t>(CONTAMINATION_IDS_IMAGE_SIZE * CONTAMINATION_IDS_IMAGE_SIZE * CONTAMINATION_IDS_IMAGE_SIZE), value);

		m_contaminationIdsImage->copyCPUBuffer(bufferWithValue.data(), Wolf::Image::SampledInFragmentShader());
	}
}
