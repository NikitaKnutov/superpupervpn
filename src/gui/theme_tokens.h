#pragma once

namespace rl::theme {
struct Token {
    const char *name;
    const char *light;
    const char *dark;
};

inline constexpr Token kTokens[] = {
    {"bg", "#F2F4F1", "#0B0D0F"},
    {"surface", "#FFFFFF", "#15181C"},
    {"surface2", "#E9ECE8", "#1E2227"},
    {"text", "#0F1411", "#F1F4F2"},
    {"muted", "#545C66", "#8E97A4"},
    {"accent", "#B6E43C", "#C6F046"},
    {"accentText", "#0F1411", "#0B0D0F"},
    {"accentInk", "#3B6A0A", "#C6F046"},
    {"connected", "#2B7612", "#C6F046"},
    {"connecting", "#8F5205", "#F5B83D"},
    {"error", "#B42323", "#FF6B6B"},
};

inline constexpr const char *kTextTokens[] = {"text", "muted", "accentInk", "connected", "connecting", "error"};
inline constexpr const char *kBackgroundTokens[] = {"bg", "surface", "surface2"};

}
