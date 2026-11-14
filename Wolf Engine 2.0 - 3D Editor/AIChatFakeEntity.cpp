#include "AIChatFakeEntity.h"

#include <curl/curl.h>

#include "SystemManager.h"

AIChatFakeEntity::AIChatFakeEntity(SystemManager* systemManager)
: Entity("", [](Entity*){}, [](Entity*){}, [](const std::string&) { return Wolf::NullableResourceNonOwner<Entity>(); }),
  m_systemManager(systemManager)
{
    readServiceInfo();
}

void AIChatFakeEntity::activateParams()
{
    std::string unused;
    forAllVisibleParams([](EditorParamInterface* e, std::string&) { e->activate(); }, unused);
}

void AIChatFakeEntity::fillJSONForParams(std::string& outJSON)
{
    outJSON += "{\n";
    outJSON += "\t" R"("params": [)" "\n";

    forAllVisibleParams([this](EditorParamInterface* e, std::string& inOutJSON) mutable
    {
        e->addToJSON(inOutJSON, 1, false);
    }, outJSON);

    if (const size_t commaPos = outJSON.substr(outJSON.size() - 3).find(','); commaPos != std::string::npos)
    {
        outJSON.erase(commaPos + outJSON.size() - 3);
    }
    outJSON += "\t]\n";
    outJSON += "}";
}

void AIChatFakeEntity::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler, const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<DrawManager>& drawManager,
    const Wolf::ResourceNonOwner<EditorPhysicsManager>& editorPhysicsManager)
{
    m_requestMutex.lock();
    if (m_needJoin)
    {
        m_requestMutex.unlock();

        m_requestThread.join();
        m_needJoin = false;

        if (!m_requestedMessageFromSystem.empty())
        {
            m_requestParam = m_requestedMessageFromSystem;
            onRequest();
        }
    }
    else
    {
        m_requestMutex.unlock();
    }
}

void AIChatFakeEntity::save() const
{
    // Nothing to do
}

void AIChatFakeEntity::forAllVisibleParams(const std::function<void(EditorParamInterface*, std::string& inOutString)>& callback, std::string& inOutString)
{
    for (EditorParamInterface* editorParam : m_alwaysVisibleParams)
    {
        callback(editorParam, inOutString);
    }
}

std::string AIChatFakeEntity::computeAssistantName()
{
    if (m_serviceName == "gemini")
    {
        return "Gemini";
    }
    else if (m_serviceName == "local")
    {
        return "Qwen";
    }
    return "Assistant";
}

void AIChatFakeEntity::computeGeminiPayload(std::string& outPayload)
{
    Wolf::Debug::sendCriticalError("Not updated, see computeOllamaPayload");

    std::string contentsJson = "\"contents\": [\n";
    for (size_t i = 0; i < m_messageHistory.size(); ++i)
    {
        if (i > 0) contentsJson += ",\n";

        contentsJson += "    {\n";
        contentsJson += "        \"role\": \"" + m_messageHistory[i].m_role + "\",\n";
        contentsJson += "        \"parts\": [\n";
        contentsJson += "            {\n";
        contentsJson += "                \"text\": \"" + m_messageHistory[i].m_message + "\"";

        if (m_messageHistory[i].m_role == "model" && !m_messageHistory[i].m_thoughtSignature.empty())
        {
            contentsJson += ",\n                \"thoughtSignature\": \"" + m_messageHistory[i].m_thoughtSignature + "\"";
        }

        contentsJson += "\n            }\n";
        contentsJson += "        ]\n";
        contentsJson += "    }";
    }
    contentsJson += "\n]";

    std::string systemInstructionJson = R"({
        "parts": [
            {
                "text": "You are the AI assistant for a 3D editor. "
                        "You have access to the following asset catalog: "
                        "- saves/assets/sphere.json: A unit sphere centered at origin. "
                        "- saves/assets/square.json: A 2D flat plane. "
                        "- saves/assets/cube.json: A unit cube with bounds [-0.5, 0.5] on all axes. "
                        "Always use these exact assetIds when issuing addExternalScene commands."
            }
        ]
    })";

    outPayload = R"({
        "system_instruction": )" + systemInstructionJson + R"(,
        )" + contentsJson + R"(,
        "generationConfig": {
            "responseMimeType": "application/json",
            "responseSchema": {
                "type": "OBJECT",
                "properties": {
                    "message": {
                        "type": "STRING",
                        "description": "A summary of the output to be displayed to the user."
                    },
                    "commands": {
                        "type": "ARRAY",
                        "items": {
                            "type": "OBJECT",
                            "properties": {
                                "type": {
                                    "type": "STRING",
                                    "enum": ["addExternalScene"]
                                },
                                "assetId": {
                                    "type": "STRING",
                                    "enum": ["saves/assets/sphere.json", "saves/assets/square.json", "saves/assets/cube.json"]
                                },
                                "transform": {
                                    "type": "ARRAY",
                                    "description": "A 4x4 transformation matrix represented as 16 numbers in column-major order.",
                                    "items": {
                                        "type": "NUMBER"
                                    },
                                    "minItems": 16,
                                    "maxItems": 16
                                }
                            },
                            "required": ["type", "assetId", "transform"]
                        }
                    }
                },
                "required": ["message", "commands"]
            }
        }
    })";
}

std::string escapeJsonString(const std::string& input)
{
    std::string output;
    for (char c : input) {
        switch (c) {
        case '\"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 32)
            {
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u00%02x", c);
                output += buf;
            }
            else
            {
                output += c;
            }
        }
    }
    return output;
}

void AIChatFakeEntity::computeOllamaPayload(std::string& outPayload)
{
    std::string messagesJson = "\"messages\": [\n";

    messagesJson += "    {\n";
    messagesJson += "        \"role\": \"system\",\n";
    messagesJson += "        \"content\": \"You are the AI assistant for a 3D editor. Up axis is Y. You have access to the following asset catalog: - saves/assets/sphere.json: A unit sphere centered at origin."
                    " - saves/assets/square.json: A 2D centered flat plane with normal (0, 0, 1). When creating roads, floors, or ground surfaces using square.json, you MUST rotate it (e.g., 90 degrees around the X-axis, like [90.0, 0.0, 0.0]) so it lies flat horizontally on the ground. "
                    " - saves/assets/cube.json: A unit cube with bounds [-0.5, 0.5] on all axes. CRITICAL RULE: Whenever you want to place an asset, "
                    "you MUST first issue an 'addEntity' command with a unique name, and then issue an 'addExternalScene' command using that exact entityName. Entity names must be globally unique across the"
                    " entire session. Don't add empty entities to see something you need to link an 'externalScene' component by issuing 'addExternalScene'\"\n";
    messagesJson += "    }";

    for (size_t i = 0; i < m_messageHistory.size(); ++i)
    {
        messagesJson += ",\n";
        std::string ollamaRole = (m_messageHistory[i].m_role == "model") ? "assistant" : "user";

        messagesJson += "    {\n";
        messagesJson += "        \"role\": \"" + ollamaRole + "\",\n";
        messagesJson += "        \"content\": \"" + escapeJsonString(m_messageHistory[i].m_message) + "\"\n";
        messagesJson += "    }";
    }
    messagesJson += "\n]";

    std::string formatSchemaJson = R"({
        "type": "object",
        "properties": {
            "message": {
                "type": "string",
                "description": "A summary of the output to be displayed to the user."
            },
            "commands": {
                "type": "array",
                "items": {
                    "type": "object",
                    "oneOf": [
                        {
                            "properties": {
                                "type": { "type": "string", "enum": ["addEntity"] },
                                "name": { "type": "string", "description": "A unique name for the new entity." }
                            },
                            "required": ["type", "name"]
                        },
                        {
                            "properties": {
                                "type": { "type": "string", "enum": ["addExternalScene"] },
                                "entityName": { "type": "string", "description": "The unique name of the entity this scene is attached to." },
                                "assetId": {
                                    "type": "string",
                                    "enum": ["saves/assets/sphere.json", "saves/assets/square.json", "saves/assets/cube.json"]
                                },
                                "transform": {
                                    "type": "object",
                                    "description": "The local transformation for the asset.",
                                    "properties": {
                                        "position": {
                                            "type": "array",
                                            "description": "Translation vector [x, y, z].",
                                            "items": { "type": "number" }
                                        },
                                        "scale": {
                                            "type": "array",
                                            "description": "Scale vector [x, y, z].",
                                            "items": { "type": "number" }
                                        },
                                        "rotation": {
                                            "type": "array",
                                            "description": "Rotation angles in degrees around X, Y, and Z axes [pitch, yaw, roll].",
                                            "items": { "type": "number" }
                                        }
                                    },
                                    "required": ["position", "scale", "rotation"]
                                }
                            },
                            "required": ["type", "entityName", "assetId", "transform"]
                        }
                    ]
                }
            }
        },
        "required": ["message", "commands"]
    })";

    outPayload = R"({
        "model": "qwen2.5:7b",
        )" + messagesJson + R"(,
        "format": )" + formatSchemaJson + R"(,
        "stream": false
    })";
}

size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
    size_t totalSize = size * nmemb;
    userp->append((char*)contents, totalSize);
    return totalSize;
}

void AIChatFakeEntity::onRequest()
{
    m_requestThread = std::thread(&AIChatFakeEntity::handleAIRequest, this);
}

void AIChatFakeEntity::handleAIRequest()
{
    m_chatParam.addMessage(m_requestParam, m_requestedMessageFromSystem.empty() ? "User" : "Wolf System");
    m_requestedMessageFromSystem.clear();

    m_chatParam.showTypingIndicator(computeAssistantName());

    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    std::string outputAnswer = "No output";

    m_messageHistory.push_back({.m_role = "user", .m_message = m_requestParam, .m_thoughtSignature = ""});

    std::string payloadStr;
    if (m_serviceName == "gemini")
    {
        computeGeminiPayload(payloadStr);
    }
    else if (m_serviceName == "local")
    {
        computeOllamaPayload(payloadStr);
    }

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (m_serviceName == "gemini")
    {
        std::string authHeader = "x-goog-api-key: " + m_apiKey;
        headers = curl_slist_append(headers, authHeader.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, m_serviceURL.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    res = curl_easy_perform(curl);

    if(res != CURLE_OK)
    {
        Wolf::Debug::sendCriticalError("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));
    }

    outputAnswer = readBuffer;

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    curl_global_cleanup();

    std::string modelReply;
    bool hasError = false;

    {
        Wolf::JSONReader::StringReadInfo jsonReadInfo;
        jsonReadInfo.jsonData = outputAnswer;
        Wolf::JSONReader jsonReader(jsonReadInfo);

        Wolf::JSONReader::JSONObjectInterface* root = jsonReader.getRoot();

        if (root->hasProperty("error"))
        {
            Wolf::JSONReader::JSONObjectInterface* errorObj = root->getPropertyObject("error");
            float errorCode = errorObj->getPropertyFloat("code");
            std::string errorMessage = errorObj->getPropertyString("message");

            modelReply = "Error: " + std::to_string(static_cast<int>(errorCode)) + " - " + errorMessage;
            hasError = true;
        }
        else
        {
            std::string thoughtSignature;
            if (m_serviceName == "gemini")
            {
                Wolf::JSONReader::JSONObjectInterface* contentObj = root->getArrayObjectItem("candidates", 0)->getPropertyObject("content");
                Wolf::JSONReader::JSONObjectInterface* partsObj = contentObj->getArrayObjectItem("parts", 0);

                modelReply = partsObj->getPropertyString("text");
                if (partsObj->hasProperty("thoughtSignature"))
                {
                    thoughtSignature = partsObj->getPropertyString("thoughtSignature");
                }
            }
            else if (m_serviceName == "local")
            {
                Wolf::JSONReader::JSONObjectInterface* messageObj = root->getPropertyObject("message");
                modelReply = messageObj->getPropertyString("content");
            }

            HistoryMessage& historyMessage = m_messageHistory.emplace_back();
            historyMessage.m_thoughtSignature = thoughtSignature;
            historyMessage.m_message = modelReply;
            historyMessage.m_role = "model";
        }
    }

    std::string messageForUser;
    if (!hasError)
    {
        Wolf::JSONReader::StringReadInfo jsonReadInfo;

        auto replaceAll= [](std::string& str, const std::string& from, const std::string& to) {
            if (from.empty()) return;
            size_t start_pos = 0;
            while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length();
            }
        };

        replaceAll(modelReply, "\\n", "");
        replaceAll(modelReply, "\\r", "");

        jsonReadInfo.jsonData = modelReply;
        Wolf::JSONReader jsonReader(jsonReadInfo);
        Wolf::JSONReader::JSONObjectInterface* root = jsonReader.getRoot();

        messageForUser = root->getPropertyString("message");

        uint32_t commandCount = root->getArraySize("commands");
        Wolf::Debug::sendInfo("Processing " + std::to_string(commandCount) + " commands...");

        std::string conflictStr;
        for (size_t i = 0; i < commandCount; ++i)
        {
            Wolf::JSONReader::JSONObjectInterface* cmdObj = root->getArrayObjectItem("commands", i);
            std::string cmdType = cmdObj->getPropertyString("type");

            if (cmdType == "addExternalScene")
            {
                std::string entityName = cmdObj->getPropertyString("entityName");
                Wolf::NullableResourceNonOwner<Entity> entity = m_systemManager->getEntityFromLoadingPath(entityName);

                if (entity && entity->hasMeshComponent())
                {
                    conflictStr += "Entity '" + entityName + "' already exists and already has an external scene component. Use an entity without external scene component or create a new one.\n";
                }
            }
        }

        if (!conflictStr.empty())
        {
            Wolf::Debug::sendInfo("Conflicts found, asking AI to fix");

            conflictStr = "One or more issues has been found in the output: \n" + conflictStr;
            conflictStr += "Please re-do without these errors";

            m_chatParam.hideTypingIndicator();
            m_chatParam.addMessage(messageForUser, computeAssistantName());

            m_requestedMessageFromSystem = conflictStr;

            m_needJoin = true;
            return;
        }

        for (size_t i = 0; i < commandCount; ++i)
        {
            m_requestMutex.lock();

            Wolf::JSONReader::JSONObjectInterface* cmdObj = root->getArrayObjectItem("commands", i);
            std::string cmdType = cmdObj->getPropertyString("type");

            Wolf::Debug::sendInfo("Command type is " + cmdType);

            if (cmdType == "addEntity")
            {
                std::string entityName = cmdObj->getPropertyString("name");
                Wolf::Debug::sendInfo("Adding entity with name: " + entityName);

                Entity* entity = m_systemManager->addEntity(entityName, "", true);
                entity->setName(entityName);
            }
            else if (cmdType == "addExternalScene")
            {
                std::string entityName = cmdObj->getPropertyString("entityName");
                std::string assetId = cmdObj->getPropertyString("assetId");

                Wolf::Debug::sendInfo("Asset id is " + assetId);

                Wolf::JSONReader::JSONObjectInterface* transformObj = cmdObj->getPropertyObject("transform");

                const std::vector<float>& posArray = transformObj->getPropertyFloatArray("position");
                glm::vec3 translation(posArray[0], posArray[1], posArray[2]);

                const std::vector<float>& scaleArray = transformObj->getPropertyFloatArray("scale");
                glm::vec3 scale(scaleArray[0], scaleArray[1], scaleArray[2]);
                if (scale.x == 0.0f || scale.y == 0.0f || scale.z == 0.0f)
                    Wolf::Debug::sendCriticalError("Scale is wrong");

                const std::vector<float>& rotArray = transformObj->getPropertyFloatArray("rotation");
                glm::vec3 eulerAnglesDegrees(rotArray[0], rotArray[1], rotArray[2]);
                glm::vec3 eulerAnglesRadians = glm::radians(eulerAnglesDegrees);

                m_systemManager->getAssetManager()->addExternalScene(assetId);

                Wolf::NullableResourceNonOwner<Entity> entity = m_systemManager->getEntityFromLoadingPath(entityName);
                if (!entity)
                {
                    Wolf::Debug::sendInfo("Entity not found, creating it...");

                    Entity* createdEntity = m_systemManager->addEntity(entityName, "", true);
                    createdEntity->setName(entityName);
                    entity = m_systemManager->getEntityFromLoadingPath(entityName);
                }

                ExternalSceneComponent* component = static_cast<ExternalSceneComponent*>(m_systemManager->getComponentInstancier()->instanciateComponent(ExternalSceneComponent::ID));
                component->setAssetId(assetId);
                component->setPosition(translation);
                component->setRotationEuler(eulerAnglesRadians);
                component->setScale(scale);
                entity->addComponent(component);
            }
            else
            {
                Wolf::Debug::sendCriticalError("Unhandled");
            }

            m_requestMutex.unlock();
        }
    }
    else
    {
        messageForUser = modelReply;
    }

    m_chatParam.hideTypingIndicator();
    m_chatParam.addMessage(messageForUser.empty() ? "No message" : messageForUser, computeAssistantName());

    m_needJoin = true;
}

void AIChatFakeEntity::readServiceInfo()
{
    Wolf::JSONReader::FileReadInfo readInfo;
    readInfo.filename = "config/aiConfig.json";
    Wolf::JSONReader jsonReader(readInfo);

    uint32_t serviceCount = jsonReader.getRoot()->getArraySize("services");

    bool serviceFound = false;
    for (uint32_t i = 0; i < serviceCount; ++i)
    {
        Wolf::JSONReader::JSONObjectInterface* serviceObj = jsonReader.getRoot()->getArrayObjectItem("services", i);
        std::string serviceName = serviceObj->getPropertyString("serviceName");

        if (serviceName == m_serviceName)
        {
            serviceFound = true;

            m_serviceURL = serviceObj->getPropertyString("URL");
            if (serviceObj->hasProperty("apiKey"))
            {
                m_apiKey = serviceObj->getPropertyString("apiKey");
            }
            break;
        }
    }

    if (!serviceFound)
    {
        Wolf::Debug::sendCriticalError("Service not found");
    }
}
