#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ExternalSceneLoader.h"

struct ModelLoadingInfo;
class ModelLoader;
struct ModelData;

class DAEImporter
{
public:
	DAEImporter(ExternalSceneLoader::OutputData& outputData, const ExternalSceneLoader::SceneLoadingInfo& sceneLoadingInfo, AssetManager* assetManager);

private:
	static void extractFloatValues(const std::string& input, std::vector<float>& outValues);

	struct InternalInfoPerBone
	{
		std::string name;
		glm::mat4 offsetMatrix;
		struct Pose
		{
			float time;
			glm::mat4 transform;
		};
		std::vector<Pose> poses;
	};
	std::vector<InternalInfoPerBone> m_bonesInfo;
	const InternalInfoPerBone* findBoneInfoByName(const std::string& name, uint32_t& outIdx) const;

	struct Node
	{
		std::string name;
		std::vector<std::pair<std::string, std::string>> properties;
		std::string body;

		std::vector<std::unique_ptr<Node>> children;
		Node* parent;

		std::string getProperty(const std::string& propertyName);
		Node* getFirstChildByName(const std::string& childName);
		Node* getChildByName(const std::string& childName, uint32_t idx);
	};
	Node* createNode(Node* currentNode);
	void findNodesInHierarchy(Node* currentNode, std::vector<AnimationData::Bone>& currentBoneArray);

	std::vector<std::unique_ptr<Node>> m_rootNodes;

	ExternalSceneLoader::MeshData* m_meshData = nullptr;
};
