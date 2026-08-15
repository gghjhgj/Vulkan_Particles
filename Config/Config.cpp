#include "Config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <type_traits>

WindowConfig Config::window;
ParticlesConfig Config::particles;

namespace
{
using Section = std::unordered_map<std::string, std::string>;
using IniData = std::unordered_map<std::string, Section>;

std::string trim(const std::string& str)
{
    const auto first =
        str.find_first_not_of(" \t\r\n");

    if (first == std::string::npos)
        return {};

    const auto last =
        str.find_last_not_of(" \t\r\n");

    return str.substr(
        first,
        last - first + 1
    );
}

IniData parseIni(const std::string& path)
{
    std::ifstream file(path);

    if (!file)
    {
        throw std::runtime_error(
            "Failed to open configuration file: " + path
        );
    }

    IniData data;

    std::string line;
    std::string currentSection;

    while (std::getline(file, line))
    {
        line = trim(line);

        if (line.empty() ||
            line[0] == '#' ||
            line[0] == ';')
        {
            continue;
        }

        if (line.front() == '[' &&
            line.back() == ']')
        {
            currentSection =
                trim(
                    line.substr(
                        1,
                        line.size() - 2
                    )
                );

            continue;
        }

        const auto separator =
            line.find('=');

        if (separator == std::string::npos)
        {
            throw std::runtime_error(
                "Invalid configuration line: " + line
            );
        }

        const std::string key =
            trim(
                line.substr(
                    0,
                    separator
                )
            );

        std::string value =
            trim(
                line.substr(
                    separator + 1
                )
            );

        const auto comment =
            value.find_first_of("#;");

        if (comment != std::string::npos)
        {
            value =
                trim(
                    value.substr(
                        0,
                        comment
                    )
                );
        }

        data[currentSection][key] = value;
    }

    return data;
}

template <typename T>
T get(
    const IniData& data,
    const std::string& section,
    const std::string& key)
{
    const auto sectionIt =
        data.find(section);

    if (sectionIt == data.end())
    {
        throw std::runtime_error(
            "Missing configuration section [" +
            section +
            "]"
        );
    }

    const auto valueIt =
        sectionIt->second.find(key);

    if (valueIt == sectionIt->second.end())
    {
        throw std::runtime_error(
            "Missing configuration value: [" +
            section +
            "] " +
            key
        );
    }

    if constexpr (std::is_same_v<T, std::string>)
    {
        return valueIt->second;
    }
    else
    {
        std::stringstream stream(
            valueIt->second
        );

        T value{};

        if constexpr (std::is_same_v<T, bool>)
        {
            stream >> std::boolalpha >> value;
        }
        else
        {
            stream >> value;
        }

        if (stream.fail())
        {
            throw std::runtime_error(
                "Invalid value for [" +
                section +
                "] " +
                key +
                ": " +
                valueIt->second
            );
        }

        return value;
    }
}

}

void Config::load(const std::string& path)
{
    const IniData ini =
        parseIni(path);

    window.width =
        get<int>(
            ini,
            "window",
            "width"
        );

    window.height =
        get<int>(
            ini,
            "window",
            "height"
        );

    particles.count =
        get<uint32_t>(
            ini,
            "particles",
            "count"
        );
    particles.trail_length =
        get<float>(
            ini,
            "particles",
            "trail_length"
        );
    particles.trail_width =
        get<float>(
            ini,
            "particles",
            "trail_width"
        );
    particles.size =
        get<float>(
            ini,
            "particles",
            "size"
        );
}