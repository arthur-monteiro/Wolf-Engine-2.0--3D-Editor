#pragma once

#include <mutex>

#include <CommandRecordBase.h>
#include <Pipeline.h>
#include <ResourceUniqueOwner.h>
#include <ShaderParser.h>
#include <UniformBuffer.h>

#include <DescriptorSetLayoutGenerator.h>

#include "ShootInfo.h"

class ContaminationEmitter;

class ContaminationUpdatePass : public Wolf::CommandRecordBase
{
public:
	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	void registerEmitter(ContaminationEmitter* componentEmitter);
	void unregisterEmitter(const ContaminationEmitter* componentEmitter);
	void swapShootRequests(std::vector<ShootRequest>& shootRequests);

	bool wasEnabledThisFrame() const { return m_wasEnabledThisFrame; }

private:
	void createPipeline();

	Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_computeShaderParser;
	Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_computePipeline;

	Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;

	class PerEmitterInfo
	{
	public:
		PerEmitterInfo(ContaminationEmitter* contaminationEmitter, const Wolf::DescriptorSetLayoutGenerator& descriptorSetLayoutGenerator, const Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& descriptorSetLayout,
			const Wolf::ResourceUniqueOwner<Wolf::UniformBuffer>& shootRequestsBuffer);

		bool isSame(const ContaminationEmitter* contaminationEmitter) const { return m_componentPtr == contaminationEmitter; }
		Wolf::ResourceUniqueOwner<Wolf::DescriptorSet>& getUpdateDescriptorSet() { return m_updateDescriptorSet; }
		ContaminationEmitter* getContaminationEmitter() const { return m_componentPtr; }

	private:
		ContaminationEmitter* m_componentPtr;

		Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_updateDescriptorSet;
	};
	std::vector<PerEmitterInfo> m_contaminationEmitters;
	std::mutex m_contaminationEmitterLock;
	std::vector<ShootRequest> m_newShootRequests;
	std::vector<ShootRequest> m_shootRequests;
	std::mutex m_shootRequestsLock;

	struct ShootRequestGPUInfo
	{
		uint32_t shootRequestCount;
		glm::vec3 padding;

		static constexpr uint32_t MAX_SHOOT_REQUEST = 16;
		glm::vec4 startPointAndLength[MAX_SHOOT_REQUEST];
		glm::vec4 directionAndRadius[MAX_SHOOT_REQUEST];
		glm::vec4 startPointOffsetAndMaterialsCleanedFlags[MAX_SHOOT_REQUEST]; // zw unused

	};
	Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_shootRequestBuffer;
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_stagingShootRequestBuffer;

	bool m_wasEnabledThisFrame = false;
};

