#pragma once

#include <fstream>
#include <glm/glm.hpp>

#include <AABB.h>

namespace sol
{
    class state;
}

class BinaryWriter
{
public:
    static void addScriptCallbacks(sol::state& state);

    void open(const std::string& path);
    void close() { m_file.close(); }

    void writeUInt(uint32_t val)
    {
        m_file.write(reinterpret_cast<const char*>(&val), sizeof(uint32_t));
    }
    void writeVec3(const glm::vec3& v)
    {
        m_file.write(reinterpret_cast<const char*>(&v), sizeof(glm::vec3));
    }
    void writeVec2(const glm::vec2& v)
    {
        m_file.write(reinterpret_cast<const char*>(&v), sizeof(glm::vec2));
    }
    void writeMat4(const glm::mat4& m)
    {
        m_file.write(reinterpret_cast<const char*>(&m), sizeof(glm::mat4));
    }
    void writeData(const uint8_t* data, uint32_t copySize)
    {
        m_file.write(reinterpret_cast<const char*>(data), copySize);
    }
    void writeAABB(const Wolf::AABB& aabb)
    {
        m_file.write(reinterpret_cast<const char*>(&aabb), sizeof(aabb));
    }

private:
    std::ofstream m_file;
};