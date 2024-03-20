#include "ContaminationEmitter.h"

#include "EditorParamsHelper.h"

ContaminationEmitter::ContaminationEmitter(const std::function<void(ComponentInterface*)>& requestReloadCallback)
{
	m_requestReloadCallback = requestReloadCallback;

	Wolf::CreateImageInfo createImageInfo{};
	createImageInfo.extent = { CONTAMINATION_IDS_IMAGE_SIZE, CONTAMINATION_IDS_IMAGE_SIZE, CONTAMINATION_IDS_IMAGE_SIZE };
	createImageInfo.format = VK_FORMAT_R8_UINT;
	createImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	createImageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
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
	std::vector<EditorParamInterface*> params = { &m_contaminationMaterials };
	::loadParams<ContaminationMaterial>(jsonReader, ID, params);
}

void ContaminationEmitter::activateParams()
{
	m_contaminationMaterials.activate();
}

void ContaminationEmitter::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	m_contaminationMaterials.addToJSON(outJSON, tabCount, false);
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
