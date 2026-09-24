// dlssnr-amd.ini: the ReShade add-on's settings. Re-read while the game runs.
#include "nr_pe_config.hpp"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace nr::pe {
namespace {

std::string trim(std::string s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::optional<bool> parse_bool(const std::string& v) {
    const std::string s = lower(v);
    if (s == "1" || s == "true" || s == "on" || s == "yes") return true;
    if (s == "0" || s == "false" || s == "off" || s == "no") return false;
    return std::nullopt;
}

float number(const std::string& v, float lo, float hi) {
    return std::clamp(std::strtof(v.c_str(), nullptr), lo, hi);
}

uint64_t stamp_of(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) return 0;
    return (uint64_t(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
}

// One pass field, named as OptiScaler's Pass<N><Field> (Pass2Intensity ...).
bool set_pass_field(PassOverride& o, const std::string& field, const std::string& value) {
    if (field == "style") { const int v = std::atoi(value.c_str()); if (v >= 0 && v <= 2) o.style = v; }
    else if (field == "intensity") o.intensity = number(value, 0, 2);
    else if (field == "localstructure" || field == "local_structure") o.local_structure = number(value, 0, 2);
    else if (field == "localtone" || field == "local_tone") o.local_tone = number(value, 0, 2);
    else if (field == "skinstructure" || field == "skin_structure") o.skin_structure = number(value, -1, 2);
    else if (field == "automask" || field == "automatic_mask") { if (auto b = parse_bool(value)) o.automatic_mask = *b; }
    else return false;
    return true;
}

// Parse one file into `c`. Returns false if it could not be opened. `legacy`
// is set when the file uses the old lowercase keys (before 2026-09-23).
bool parse(const std::string& path, Config& c, bool& legacy) {
    FILE* file = std::fopen(path.c_str(), "r");
    if (!file) return false;
    int passes = c.controls.passes;
    bool section = false;
    char line[512];
    while (std::fgets(line, sizeof line, file)) {
        std::string text = line;
        const auto comment = text.find_first_of(";#");
        if (comment != std::string::npos) text = text.substr(0, comment);
        text = trim(text);
        if (text.empty()) continue;
        if (text.front() == '[') { section = true; continue; }
        const auto equals = text.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = lower(trim(text.substr(0, equals)));
        const std::string value = trim(text.substr(equals + 1));
        if (value.empty()) continue;
        auto& k = c.controls;

        if (key == "enabled") { if (auto b = parse_bool(value)) k.enabled = *b; }
        else if (key == "applymodel" || key == "apply") { if (auto b = parse_bool(value)) k.apply_model = *b; }
        else if (key == "passes") passes = std::atoi(value.c_str());
        else if (key == "unlockpasses") { if (auto b = parse_bool(value)) c.unlock_passes = *b; }
        else if (key == "workingscale" || key == "model_scale") c.model_scale = number(value, 0.25f, 1.0f);
        else if (key == "style") { const int v = std::atoi(value.c_str()); if (v >= 0 && v <= 2) k.style = v; }
        else if (key == "intensity") k.intensity = number(value, 0, 2);
        else if (key == "localstructure" || key == "local_structure") k.local_structure = number(value, 0, 2);
        else if (key == "localtone" || key == "local_tone") k.local_tone = number(value, 0, 2);
        else if (key == "skinstructure" || key == "skin_structure") k.skin_structure = number(value, -1, 2);
        else if (key == "automask" || key == "automatic_mask") { if (auto b = parse_bool(value)) k.automatic_mask = *b; }
        else if (key == "transferstrength") k.detail_strength = number(value, 0, 2);
        else if (key == "colourstrength" || key == "colour" || key == "color") k.colour_strength = number(value, 0, 4);
        else if (key == "maxratio") k.max_ratio = number(value, 1, 8);
        else if (key == "history") c.history = number(value, 0, 1);
        else if (key == "whitepoint" || key == "white_point") c.white_point = number(value, 0.01f, 100.0f);
        else if (key == "verbose") { if (auto b = parse_bool(value)) c.verbose = *b; }
        else if (key.rfind("pass", 0) == 0 && key.size() > 5 && std::isdigit(static_cast<unsigned char>(key[4]))) {
            // Pass<N><Field>, or the old pass<N>_<field>.
            size_t end = 4;
            while (end < key.size() && std::isdigit(static_cast<unsigned char>(key[end]))) ++end;
            const int n = std::atoi(key.substr(4, end - 4).c_str());
            std::string field = key.substr(end);
            if (!field.empty() && field.front() == '_') field.erase(0, 1);
            if (n >= 2 && n <= kMaxPasses) set_pass_field(c.pass[size_t(n) - 2], field, value);
        }
        // Anything else (the old module's placement/overlay/hooks/intercept/
        // menu_key/toggle_key/spoof_nvidia) is ignored and dropped on save.
    }
    std::fclose(file);
    legacy = !section;
    if (legacy && passes > 2) c.unlock_passes = true;   // the old file allowed up to 16
    c.controls.passes = std::clamp(passes, 1, c.pass_limit());
    c.resolve();
    return true;
}

}  // namespace

void Config::resolve() {
    auto& k = controls;
    k.per_pass.assign(size_t(kMaxPasses) - 1, PassControls{});
    for (size_t i = 0; i < k.per_pass.size(); ++i) {
        const PassOverride& o = pass[i];
        PassControls& p = k.per_pass[i];
        p.used = true;
        p.style = o.style.value_or(k.style);
        p.intensity = o.intensity.value_or(k.intensity);
        p.local_structure = o.local_structure.value_or(k.local_structure);
        p.local_tone = o.local_tone.value_or(0.0f);
        p.skin_structure = o.skin_structure.value_or(k.skin_structure);
        p.automatic_mask = o.automatic_mask.value_or(k.automatic_mask);
    }
}

void Config::load(const std::string& path) {
    Config next{};
    bool legacy = false;
    if (!parse(path, next, legacy)) {
        *this = Config{};
        resolve();
        save(path);
        return;
    }
    *this = next;
    stamp_ = stamp_of(path);
    if (legacy) save(path);
}

bool Config::reload(const std::string& path) {
    const uint64_t stamp = stamp_of(path);
    if (!stamp || stamp == stamp_) return false;
    Config next{};
    bool legacy = false;
    if (!parse(path, next, legacy)) return false;
    *this = next;
    stamp_ = stamp;
    return true;
}

void Config::save(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return;
    auto flag = [](bool b) { return b ? "true" : "false"; };
    const auto& k = controls;
    std::fprintf(f,
        "; DLSSNR-AMD-Vulkan ReShade add-on settings; edits take effect when saved, also in game.\n"
        "; [DlssNr] keys, ranges and defaults are those of OptiScaler DLSS-NR (OptiScaler.ini).\n"
        "[DlssNr]\n"
        "Enabled=%s\n"
        "ApplyModel=%s\n"
        "; 1..2; 1..10 with UnlockPasses=true\n"
        "Passes=%d\n"
        "UnlockPasses=%s\n"
        "; Model resolution, 0.25..1\n"
        "WorkingScale=%.3f\n"
        "; 0 Standard, 1 Natural, 2 Cinematic\n"
        "Style=%d\n"
        "Intensity=%.3f\n"
        "LocalStructure=%.3f\n"
        "LocalTone=%.3f\n"
        "; -1 follows LocalStructure\n"
        "SkinStructure=%.3f\n"
        "AutoMask=%s\n"
        "; Apply edit: Detail strength 0..2, Colour strength 0..4 (above 1 adds saturation),\n"
        "; Highlight guard 1..8\n"
        "TransferStrength=%.3f\n"
        "ColourStrength=%.3f\n"
        "MaxRatio=%.3f\n"
        "; From the 2nd pass on, each pass can be set on its own: Pass2Style, Pass2Intensity, Pass2LocalStructure,\n"
        "; Pass2LocalTone, Pass2SkinStructure, Pass2AutoMask, and likewise Pass3...\n"
        "; Keys left out inherit the 1st pass, except LocalTone, which defaults to 0.\n",
        flag(k.enabled), flag(k.apply_model), k.passes, flag(unlock_passes), model_scale, k.style,
        k.intensity, k.local_structure, k.local_tone, k.skin_structure, flag(k.automatic_mask),
        k.detail_strength, k.colour_strength, k.max_ratio);
    for (int n = 2; n <= kMaxPasses; ++n) {
        const PassOverride& o = pass[size_t(n) - 2];
        if (o.style) std::fprintf(f, "Pass%dStyle=%d\n", n, *o.style);
        if (o.intensity) std::fprintf(f, "Pass%dIntensity=%.3f\n", n, *o.intensity);
        if (o.local_structure) std::fprintf(f, "Pass%dLocalStructure=%.3f\n", n, *o.local_structure);
        if (o.local_tone) std::fprintf(f, "Pass%dLocalTone=%.3f\n", n, *o.local_tone);
        if (o.skin_structure) std::fprintf(f, "Pass%dSkinStructure=%.3f\n", n, *o.skin_structure);
        if (o.automatic_mask) std::fprintf(f, "Pass%dAutoMask=%s\n", n, flag(*o.automatic_mask));
    }
    std::fprintf(f,
        "\n; This project's own settings\n"
        "; How strongly the previous frame's result is blended into this one, 0..1\n"
        "History=%.3f\n"
        "; White point of linear-light input\n"
        "WhitePoint=%.3f\n"
        "; Log a line for every skipped frame\n"
        "Verbose=%s\n",
        history, white_point, flag(verbose));
    std::fclose(f);
    stamp_ = stamp_of(path);
}

}  // namespace nr::pe
