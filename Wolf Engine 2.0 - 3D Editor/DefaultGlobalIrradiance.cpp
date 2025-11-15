#include "DefaultGlobalIrradiance.h"

#include <fstream>

void DefaultGlobalIrradiance::addShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const
{
    std::ifstream inFile("Shaders/defaultGlobalIrradiance/readGlobalIrradiance.glsl");
    std::string line;
    while (std::getline(inFile, line))
    {
        inOutShaderCodeToAdd.codeString += line + '\n';
    }
}
