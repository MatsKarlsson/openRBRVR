#pragma once

#include "Util.hpp"
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <inicpp.h>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>

// Metres and quaternion XYZW in the recentered cockpit reference frame, not
// raw runtime LOCAL/room coordinates. Version the keys for future migrations.
struct HandButtonLayout {
    std::array<std::optional<M4>, 2> poses;
    static constexpr std::array<const char*, 2> keys { "handStartButtonV1", "handHelpButtonV1" };

    static std::optional<M4> parse(const std::string& text)
    {
        std::istringstream stream(text);
        stream.imbue(std::locale::classic());
        std::array<float, 7> values;
        for (auto& v : values)
            if (!(stream >> v) || !std::isfinite(v))
                return std::nullopt;
        stream >> std::ws;
        if (!stream.eof())
            return std::nullopt;
        const glm::vec3 position(values[0], values[1], values[2]);
        const glm::quat q(values[6], values[3], values[4], values[5]);
        const float norm = glm::dot(q, q);
        if (glm::length(position) > 3.0f || norm < 0.5f || norm > 1.5f)
            return std::nullopt;
        auto pose = glm::mat4_cast(glm::normalize(q));
        pose[3] = glm::vec4(position, 1);
        return pose;
    }

    static std::string serialize(const M4& pose)
    {
        const auto q = glm::normalize(glm::quat_cast(pose));
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::setprecision(9) << pose[3].x << ' ' << pose[3].y << ' ' << pose[3].z
               << ' ' << q.x << ' ' << q.y << ' ' << q.z << ' ' << q.w;
        return stream.str();
    }

    static std::optional<HandButtonLayout> load(const std::filesystem::path& path)
    {
        try {
            HandButtonLayout layout;
            if (!std::filesystem::exists(path))
                return layout;
            std::ifstream stream(path);
            if (!stream)
                return std::nullopt;
            ini::IniFile file;
            file.decode(stream);
            if (stream.bad())
                return std::nullopt;
            for (size_t i = 0; i < keys.size(); ++i)
                layout.poses[i] = parse(file["openRBRVR"][keys[i]].as<std::string>());
            return layout;
        } catch (...) {
            return std::nullopt;
        }
    }

    // Update only the locked button. Keep the other placement and all existing
    // camera/seat/other plugin values. Replace only after a complete write.
    static bool save(const std::filesystem::path& path, size_t button, const M4& pose)
    {
        if (button >= keys.size())
            return false;
        const auto text = serialize(pose);
        if (!parse(text))
            return false;
        auto temporary = path;
        temporary += ".openRBRVR.tmp";
        try {
            ini::IniFile file;
            if (std::filesystem::exists(path)) {
                std::ifstream input(path);
                if (!input)
                    return false;
                file.decode(input);
                if (input.bad())
                    return false;
            }
            file["openRBRVR"][keys[button]] = text;
            {
                std::ofstream output(temporary, std::ios::trunc);
                if (!output)
                    return false;
                file.encode(output);
                output.flush();
                if (!output) {
                    output.close();
                    std::filesystem::remove(temporary);
                    return false;
                }
                output.close();
                if (output.fail()) {
                    std::filesystem::remove(temporary);
                    return false;
                }
            }
            if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                return true;
            std::filesystem::remove(temporary);
        } catch (...) {
            std::error_code error;
            std::filesystem::remove(temporary, error);
        }
        return false;
    }
};
