#pragma once

#include "Entity.h"

class SystemManager;

class AIChatFakeEntity : public Entity
{
public:
    AIChatFakeEntity(SystemManager* systemManager);

    void activateParams() override;
    void fillJSONForParams(std::string& outJSON) override;

    void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler, const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<DrawManager>& drawManager, const Wolf::ResourceNonOwner<EditorPhysicsManager>& editorPhysicsManager) override;

    void save() const override;

    const std::string& getName() const override { return m_name; }
    std::string computeEscapedLoadingPath() const override { return "aiChatId"; }
    bool isTransient() const override { return true; }

private:
    std::string m_name = "AIChat";
    inline static const std::string TAB = "AI Chat";
    SystemManager* m_systemManager;

    void forAllVisibleParams(const std::function<void(EditorParamInterface*, std::string& inOutString)>& callback, std::string& inOutString);
    std::string computeAssistantName();
    void computeGeminiPayload(std::string& outPayload);
    void computeOllamaPayload(std::string& outPayload);

    EditorParamChat m_chatParam = EditorParamChat("Chat", TAB, "Request");
    void onRequest();
    void handleAIRequest();
    EditorParamString m_requestParam = EditorParamString("Request", TAB, "Request");
    EditorParamButton m_sendRequestButtonParam = EditorParamButton("Send request", TAB, "Request", [this]() { onRequest(); });

    std::string m_serviceName = "local";
    std::string m_serviceURL;
    std::string m_apiKey;
    void readServiceInfo();

    std::array<EditorParamInterface*, 3> m_alwaysVisibleParams =
    {
        &m_chatParam,
        &m_requestParam,
        &m_sendRequestButtonParam
    };

    struct HistoryMessage
    {
        std::string m_role;
        std::string m_message;
        std::string m_thoughtSignature;
    };
    std::vector<HistoryMessage> m_messageHistory;
    std::thread m_requestThread;
    std::mutex m_requestMutex;
    bool m_needJoin = false;

    std::string m_requestedMessageFromSystem;
};
