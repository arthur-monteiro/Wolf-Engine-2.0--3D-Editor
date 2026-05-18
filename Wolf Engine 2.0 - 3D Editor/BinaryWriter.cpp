#include "BinaryWriter.h"

#include <sol/sol.hpp>

#include "Debug.h"

void BinaryWriter::addScriptCallbacks(sol::state& state)
{
    state.new_usertype<BinaryWriter>("BinaryWriter",
        "open", &BinaryWriter::open,
        "close", &BinaryWriter::close,
        "writeUInt", &BinaryWriter::writeUInt,
        "writeVec2", &BinaryWriter::writeVec2,
        "writeVec3", &BinaryWriter::writeVec3,
        "writeMat4", &BinaryWriter::writeMat4,
        "writeAABB", &BinaryWriter::writeAABB
    );
}

void BinaryWriter::open(const std::string& path)
{
    m_file.open(path, std::ios::binary);
    if (!m_file.good())
    {
        Wolf::Debug::sendCriticalError("Cannot open path");
    }
}
