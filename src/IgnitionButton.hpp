#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <span>

// Verify the ignition argument/result use, but allow a plugin to redirect the
// relative CALL. Its live target is the input path we must preserve.
inline uintptr_t resolve_ignition_call(std::span<const uint8_t> argument,
    std::span<const uint8_t> call, uintptr_t address)
{
    constexpr uint8_t result[] { 0x85, 0xc0, 0x74, 0x04, 0x83, 0x4e, 0x04, 0x01, 0x6a, 0x07 };
    if (argument.size() < 2 || argument[0] != 0x6a || argument[1] != 12
        || call.size() < 5 + sizeof(result) || call[0] != 0xe8
        || memcmp(call.data() + 5, result, sizeof(result)))
        return 0;
    int32_t relative;
    memcpy(&relative, call.data() + 1, sizeof(relative));
    return address + 5 + relative;
}

// A held virtual button, renewed by each valid hand frame. The lease releases
// input if VR frames stop arriving, even before lifecycle cleanup can run.
class IgnitionButton {
public:
    static constexpr int ignition_axis = 12;
    static constexpr uint64_t timeout_ms = 500;
    void set_down(bool down, uint64_t now) { deadline_ = down ? now + timeout_ms : 0; }
    void release() { deadline_ = 0; }
    bool pending() const { return deadline_.load() != 0; }
    bool is_down(int axis, uint64_t now) const
    {
        const auto deadline = deadline_.load();
        return axis == ignition_axis && deadline != 0 && now < deadline;
    }

private:
    std::atomic<uint64_t> deadline_ { 0 };
};
