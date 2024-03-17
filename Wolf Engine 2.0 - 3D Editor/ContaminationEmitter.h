#pragma once

#include "ComponentInterface.h"
#include "EditorParamArray.h"
#include "MaterialEditor.h"
#include "Notifier.h"

class ContaminationEmitter : public ComponentInterface
{
public:
	static inline std::string ID = "contaminationEmitter";
	std::string getId() const override { return ID; }

	ContaminationEmitter(const std::function<void(ComponentInterface*)>& requestReloadCallback);

	void loadParams(Wolf::JSONReader& jsonReader) override;

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

private:
	inline static const std::string TAB = "Contamination emitter";
	std::function<void(ComponentInterface*)> m_requestReloadCallback;

	class ContaminationMaterial : public ParameterGroupInterface, public Notifier
	{
	public:
		ContaminationMaterial();
		ContaminationMaterial(const ContaminationMaterial&) = default;
		
		std::span<EditorParamInterface*> getAllParams() override;
		std::span<EditorParamInterface* const> getAllConstParams() const override;
		bool hasDefaultName() const override;

	private:
		inline static const std::string DEFAULT_NAME = "New material";
		MaterialEditor m_material;
	};

	void onMaterialAdded();
	EditorParamArray<ContaminationMaterial> m_contaminationMaterials = EditorParamArray<ContaminationMaterial>("Contamination materials", TAB, "Materials", 8, [this] { onMaterialAdded(); });
};