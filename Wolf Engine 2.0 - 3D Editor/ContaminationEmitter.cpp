#include "ContaminationEmitter.h"

#include "EditorParamsHelper.h"

ContaminationEmitter::ContaminationEmitter(const std::function<void(ComponentInterface*)>& requestReloadCallback)
{
	m_requestReloadCallback = requestReloadCallback;
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
