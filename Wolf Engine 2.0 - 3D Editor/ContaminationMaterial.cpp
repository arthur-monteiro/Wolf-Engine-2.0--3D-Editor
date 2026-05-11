#include "ContaminationMaterial.h"

#include "EditorParamsHelper.h"
#include "MaterialEditor.h"

ContaminationMaterial::ContaminationMaterial(const std::function<Wolf::NullableResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
	: m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback)
{
	m_contaminationEmitterIdx = 255;
}

void ContaminationMaterial::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader.getRoot()->getPropertyObject(ID), ID, m_editorParams);
}

void ContaminationMaterial::activateParams()
{
	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void ContaminationMaterial::addParamsToJSON(std::string & outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void ContaminationMaterial::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	if (m_materialEntity && !m_materialNotificationRegistered)
	{
		if (const Wolf::NullableResourceNonOwner<MaterialEditor> materialComponent = (*m_materialEntity)->getComponent<MaterialEditor>())
		{
			materialComponent->subscribe(this, [this](Flags) { notifySubscribers(); });
			m_materialNotificationRegistered = true;

			notifySubscribers();
		}
	}
}

void ContaminationMaterial::setContaminationEmitterIdx(uint32_t idx)
{
	m_contaminationEmitterIdx = idx;
}

uint32_t ContaminationMaterial::getMaterialIdx() const
{
	if (m_materialEntity)
	{
		if (const Wolf::NullableResourceNonOwner<MaterialEditor> materialComponent = (*m_materialEntity)->getComponent<MaterialEditor>())
		{
			return materialComponent->getMaterialGPUIdx();
		}
	}

	return 0;
}

void ContaminationMaterial::onMaterialEntityChanged()
{
	if (static_cast<std::string>(m_materialEntityParam).empty())
	{
		m_materialEntity.reset(nullptr);
		return;
	}

	m_materialEntity.reset(new Wolf::ResourceNonOwner<Entity>(m_getEntityFromLoadingPathCallback(m_materialEntityParam)));
	m_materialNotificationRegistered = false;
}