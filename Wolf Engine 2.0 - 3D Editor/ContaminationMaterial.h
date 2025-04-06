#pragma once

#include <ResourceNonOwner.h>

#include "EditorConfiguration.h"
#include "EditorTypes.h"
#include "Entity.h"
#include "ParameterGroupInterface.h"

class ContaminationMaterial : public ComponentInterface
{
public:
	static inline std::string ID = "contaminationMaterial";
	std::string getId() const override { return ID; }

	ContaminationMaterial(const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback);
	ContaminationMaterial(const ContaminationMaterial&) = delete;

	void loadParams(Wolf::JSONReader& jsonReader) override;

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void saveCustom() const override {}

	void setContaminationEmitterIdx(uint32_t idx);
	uint32_t getMaterialIdx() const;
	glm::vec3 getColor() const { return m_color; }
	uint32_t getIdxInContaminationEmitter() const { return m_contaminationEmitterIdx; }

private:
	inline static const std::string TAB = "Contamination material";
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

	std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_materialEntity;
	void onMaterialEntityChanged();
	bool m_materialNotificationRegistered = false;

	EditorParamUInt m_contaminationEmitterIdx = EditorParamUInt("Contamination emitter index", TAB, "General", 0, 255, EditorParamUInt::ParamUIntType::NUMBER, false, true);

	EditorParamString m_materialEntityParam = EditorParamString("Material entity", TAB, "Visual", [this]() { onMaterialEntityChanged(); }, EditorParamString::ParamStringType::ENTITY);
	EditorParamVector3 m_color = EditorParamVector3("Color", TAB, "Visual", 0.0f, 1.0f);

	std::array<EditorParamInterface*, 3> m_editorParams =
	{
		&m_contaminationEmitterIdx,

		&m_materialEntityParam,
		&m_color
	};
};

// To be used by components that have an array of contamination materials: contamination emitter, gas cylinder...
template <const std::string& TAB>
class ContaminationMaterialArrayItem : public ParameterGroupInterface, public Notifier
{
public:
	ContaminationMaterialArrayItem();
	ContaminationMaterialArrayItem(const ContaminationMaterialArrayItem&) = delete;

	void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);

	void setGetEntityFromLoadingPathCallback(const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback);
	void setIndex(uint32_t idx);

	void getAllParams(std::vector<EditorParamInterface*>& out) const override;
	void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override;
	bool hasDefaultName() const override;
	uint32_t getMaterialIdx() const;
	glm::vec3 getColor() const;
	uint32_t getIdxInContaminationEmitter() const;

private:
	inline static const std::string DEFAULT_NAME = "New material";
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

	std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_materialEntity;
	uint32_t m_index = -1;
	void onMaterialEntityChanged();
	bool m_materialNotificationRegistered = false;

	EditorParamString m_materialEntityParam = EditorParamString("Material entity", TAB, "Material", [this]() { onMaterialEntityChanged(); }, EditorParamString::ParamStringType::ENTITY);
	std::array<EditorParamInterface*, 1> m_editorParams =
	{
		&m_materialEntityParam,
	};
};

template <const std::string& TAB>
ContaminationMaterialArrayItem<TAB>::ContaminationMaterialArrayItem() : ParameterGroupInterface(TAB)
{
	m_name = DEFAULT_NAME;
}

template <const std::string& TAB>
void ContaminationMaterialArrayItem<TAB>::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration)
{
	if (m_materialEntity && !m_materialNotificationRegistered)
	{
		if (const Wolf::NullableResourceNonOwner<ContaminationMaterial> materialComponent = (*m_materialEntity)->getComponent<ContaminationMaterial>())
		{
			materialComponent->subscribe(this, [this](Flags) { notifySubscribers(); });
			m_materialNotificationRegistered = true;

			notifySubscribers();
		}
	}
}

template <const std::string& TAB>
void ContaminationMaterialArrayItem<TAB>::setGetEntityFromLoadingPathCallback(const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
{
	m_getEntityFromLoadingPathCallback = getEntityFromLoadingPathCallback;
}

template <const std::string& TAB>
void ContaminationMaterialArrayItem<TAB>::setIndex(uint32_t idx)
{
	m_index = idx;
}

template <const std::string& TAB>
void ContaminationMaterialArrayItem<TAB>::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_editorParams.data(), &m_editorParams.back() + 1, std::back_inserter(out));
}

template <const std::string& TAB>
void ContaminationMaterialArrayItem<TAB>::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_editorParams.data(), &m_editorParams.back() + 1, std::back_inserter(out));
}

template <const std::string& TAB>
bool ContaminationMaterialArrayItem<TAB>::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}

template <const std::string& TAB>
uint32_t ContaminationMaterialArrayItem<TAB>::getMaterialIdx() const
{
	if (m_materialEntity)
	{
		if (const Wolf::NullableResourceNonOwner<ContaminationMaterial> materialComponent = (*m_materialEntity)->getComponent<ContaminationMaterial>())
		{
			return materialComponent->getMaterialIdx();
		}
	}

	return 0;
}

template <const std::string& TAB>
glm::vec3 ContaminationMaterialArrayItem<TAB>::getColor() const
{
	if (m_materialEntity)
	{
		if (const Wolf::NullableResourceNonOwner<ContaminationMaterial> materialComponent = (*m_materialEntity)->getComponent<ContaminationMaterial>())
		{
			return materialComponent->getColor();
		}
	}

	return glm::vec3(0.0f);
}

template <const std::string& TAB>
uint32_t ContaminationMaterialArrayItem<TAB>::getIdxInContaminationEmitter() const
{
	if (m_materialEntity)
	{
		if (const Wolf::NullableResourceNonOwner<ContaminationMaterial> materialComponent = (*m_materialEntity)->getComponent<ContaminationMaterial>())
		{
			return materialComponent->getIdxInContaminationEmitter();
		}
	}

	return 0;
}

template <const std::string& TAB>
void ContaminationMaterialArrayItem<TAB>::onMaterialEntityChanged()
{
	if (static_cast<std::string>(m_materialEntityParam).empty())
	{
		m_materialEntity.reset(nullptr);
		return;
	}

	m_materialEntity.reset(new Wolf::ResourceNonOwner<Entity>(m_getEntityFromLoadingPathCallback(m_materialEntityParam)));

	if (m_index != static_cast<uint32_t>(-1))
	{
		if (const Wolf::NullableResourceNonOwner<ContaminationMaterial> materialComponent = (*m_materialEntity)->getComponent<ContaminationMaterial>())
		{
			materialComponent->setContaminationEmitterIdx(m_index);
		}
	}

	m_materialNotificationRegistered = false;

	notifySubscribers();
}
