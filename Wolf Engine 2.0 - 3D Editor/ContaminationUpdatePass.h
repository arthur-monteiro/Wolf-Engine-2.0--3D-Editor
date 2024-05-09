#pragma once

#include <mutex>

#include <CommandRecordBase.h>
#include <Pipeline.h>
#include <ResourceUniqueOwner.h>
#include <ShaderParser.h>

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
	void unregisterEmitter(ContaminationEmitter* componentEmitter);
	void swapShootRequests(std::vector<ShootRequest>& shootRequests);

private:
	void createPipeline();

	Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_computeShaderParser;
	Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_computePipeline;

	std::vector<ContaminationEmitter*> m_contaminationEmitters;
	std::mutex m_contaminationEmitterLock;
	std::vector<ShootRequest> m_shootRequests;
};

