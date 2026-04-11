#pragma once

namespace CacheHelper
{
    template<typename T>
    static void writeVector(std::ofstream& file, const std::vector<T>& vec)
    {
        uint32_t size = static_cast<uint32_t>(vec.size());
        file.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        if (size > 0)
        {
            file.write(reinterpret_cast<const char*>(vec.data()), size * sizeof(T));
        }
    }

    static void writeString(std::ofstream& file, const std::string& str)
    {
        uint32_t size = static_cast<uint32_t>(str.size());
        file.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        file.write(str.data(), size);
    }

    template<typename T>
    static void readVector(std::ifstream& file, std::vector<T>& vec)
    {
        uint32_t size = 0;
        file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        vec.resize(size);
        if (size > 0)
        {
            file.read(reinterpret_cast<char*>(vec.data()), size * sizeof(T));
        }
    }

    static void readString(std::ifstream& file, std::string& str)
    {
        uint32_t size = 0;
        file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        str.resize(size);
        if (size > 0)
        {
            file.read(&str[0], size);
        }
    }
}