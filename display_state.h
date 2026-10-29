// SPDX-License-Identifier: MIT
// Shared, platform-independent input/display timing logic.
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace keydisplay {
constexpr std::uint32_t kDefaultDurationMs = 1200;
constexpr std::uint32_t kMinDurationMs = 1;
constexpr std::uint32_t kMaxDurationMs = 600000;

inline bool ParseDuration(const std::wstring& text, std::uint32_t& result) {
    if (text.empty() || text.size() > 6) return false;
    std::uint32_t value = 0;
    for (wchar_t ch : text) {
        if (ch < L'0' || ch > L'9') return false;
        value = value * 10 + static_cast<std::uint32_t>(ch - L'0');
    }
    if (value < kMinDurationMs || value > kMaxDurationMs) return false;
    result = value;
    return true;
}

struct PressedKey {
    std::uint32_t id;
    std::wstring name;
    bool modifier;
};

struct DisplayState {
    std::uint32_t durationMs = kDefaultDurationMs;
    std::uint64_t lastEventMs = 0;
    std::wstring text;
    std::vector<PressedKey> pressed;

    std::wstring Combo() const {
        std::wstring value;
        for (bool modifiers : {true, false}) {
            for (const auto& key : pressed) {
                if (key.modifier != modifiers) continue;
                if (!value.empty()) value += L" + ";
                value += key.name;
            }
        }
        return value;
    }
    void KeyDown(std::uint32_t id, const std::wstring& name,
                 bool modifier, std::uint64_t now) {
        const auto it = std::find_if(pressed.begin(), pressed.end(),
                                    [id](const PressedKey& key) { return key.id == id; });
        if (it == pressed.end()) pressed.push_back({id, name, modifier});
        // Each repeat is a new event; never defer behind the display duration.
        text = Combo();
        lastEventMs = now;
    }
    void KeyUp(std::uint32_t id) {
        pressed.erase(std::remove_if(pressed.begin(), pressed.end(),
                      [id](const PressedKey& key) { return key.id == id; }), pressed.end());
        // Release updates the held-key state, NOT the last-event bubble.
        // Otherwise a quick key down/up could disappear before a screen refresh.
    }
    void Mouse(const std::wstring& name, std::uint64_t now) {
        text.clear();
        for (const auto& key : pressed) {
            if (!key.modifier) continue;
            if (!text.empty()) text += L" + ";
            text += key.name;
        }
        if (!text.empty()) text += L" + ";
        text += name;
        lastEventMs = now;
    }
    bool Visible(std::uint64_t now) const {
        return !text.empty() && now - lastEventMs < durationMs;
    }
    std::uint32_t Remaining(std::uint64_t now) const {
        if (!Visible(now)) return 0;
        return durationMs - static_cast<std::uint32_t>(now - lastEventMs);
    }
    void Clear() {
        pressed.clear();
        text.clear();
    }
};
}  // namespace keydisplay
