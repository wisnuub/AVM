#include "avm/core/gapps.h"
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace avm {

// ---------------------------------------------------------------------------
// Built-in certified device fingerprints
// Source: https://source.android.com/docs/compatibility/cts/downloads
// These are public build fingerprints from Google's CTS verified devices.
// ---------------------------------------------------------------------------
static const DeviceFingerprint BUILTIN_PROFILES[] = {
    {
        .brand        = "google",
        .device       = "husky",
        .manufacturer = "Google",
        .model        = "Pixel 8 Pro",
        .name         = "husky",
        .fingerprint  = "google/husky/husky:14/AD1A.240905.004/12117344:user/release-keys",
        .description  = "husky-user 14 AD1A.240905.004 12117344 release-keys",
        .version_release = "14",
        .version_sdk  = 34,
    },
    {
        .brand        = "google",
        .device       = "cheetah",
        .manufacturer = "Google",
        .model        = "Pixel 7 Pro",
        .name         = "cheetah",
        .fingerprint  = "google/cheetah/cheetah:14/AP1A.240905.005/12117347:user/release-keys",
        .description  = "cheetah-user 14 AP1A.240905.005 12117347 release-keys",
        .version_release = "14",
        .version_sdk  = 34,
    },
    {
        .brand        = "google",
        .device       = "felix",
        .manufacturer = "Google",
        .model        = "Pixel Fold",
        .name         = "felix",
        .fingerprint  = "google/felix/felix:14/AD1A.240905.004/12117344:user/release-keys",
        .description  = "felix-user 14 AD1A.240905.004 12117344 release-keys",
        .version_release = "14",
        .version_sdk  = 34,
    },
};

DeviceFingerprint get_builtin_fingerprint(const std::string& profile) {
    if (profile == "pixel8pro" || profile == "husky") return BUILTIN_PROFILES[0];
    if (profile == "pixel7pro" || profile == "cheetah") return BUILTIN_PROFILES[1];
    if (profile == "pixelfold" || profile == "felix") return BUILTIN_PROFILES[2];
    // Default fallback
    if (profile == "pixel7") return BUILTIN_PROFILES[1];
    throw std::invalid_argument("Unknown spoof profile: " + profile);
}

DeviceFingerprint load_fingerprint_file(const std::filesystem::path& path) {
    // Minimal JSON parser for the fingerprint schema.
    // In a real build this would use nlohmann/json or simdjson.
    // For now we do a simple line-by-line key extraction.
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open fingerprint file: " + path.string());
    }

    DeviceFingerprint fp;
    std::string line;
    auto extract = [](const std::string& l, const std::string& key) -> std::string {
        auto pos = l.find('"' + key + '"');
        if (pos == std::string::npos) return {};
        auto colon = l.find(':', pos);
        if (colon == std::string::npos) return {};
        auto q1 = l.find('"', colon + 1);
        if (q1 == std::string::npos) return {};
        auto q2 = l.find('"', q1 + 1);
        if (q2 == std::string::npos) return {};
        return l.substr(q1 + 1, q2 - q1 - 1);
    };

    while (std::getline(f, line)) {
        if (auto v = extract(line, "brand");        !v.empty()) fp.brand = v;
        if (auto v = extract(line, "device");       !v.empty()) fp.device = v;
        if (auto v = extract(line, "manufacturer"); !v.empty()) fp.manufacturer = v;
        if (auto v = extract(line, "model");        !v.empty()) fp.model = v;
        if (auto v = extract(line, "name");         !v.empty()) fp.name = v;
        if (auto v = extract(line, "fingerprint");  !v.empty()) fp.fingerprint = v;
        if (auto v = extract(line, "description");  !v.empty()) fp.description = v;
        if (auto v = extract(line, "version_release"); !v.empty()) fp.version_release = v;
        // version_sdk is an int; handle separately
        auto sdk_pos = line.find("\"version_sdk\"");
        if (sdk_pos != std::string::npos) {
            auto colon = line.find(':', sdk_pos);
            if (colon != std::string::npos) {
                try { fp.version_sdk = std::stoi(line.substr(colon + 1)); } catch (...) {}
            }
        }
    }
    return fp;
}

void build_gapps_qemu_props(
    const DeviceFingerprint& fp,
    std::vector<std::pair<std::string, std::string>>& out
) {
    // These -prop values are passed to QEMU at startup.
    // They override the system image's build.prop at the kernel cmdline level,
    // ensuring GMS Core sees release-keys before it even starts.
    out.emplace_back("ro.product.brand",       fp.brand);
    out.emplace_back("ro.product.device",      fp.device);
    out.emplace_back("ro.product.manufacturer",fp.manufacturer);
    out.emplace_back("ro.product.model",       fp.model);
    out.emplace_back("ro.product.name",        fp.name);
    out.emplace_back("ro.build.fingerprint",   fp.fingerprint);
    out.emplace_back("ro.build.description",   fp.description);
    out.emplace_back("ro.build.version.release",fp.version_release);
    out.emplace_back("ro.build.version.sdk",   std::to_string(fp.version_sdk));
    out.emplace_back("ro.build.tags",          "release-keys");
    out.emplace_back("ro.build.type",          "user");
    out.emplace_back("ro.debuggable",          "0");
    out.emplace_back("ro.secure",              "1");
    out.emplace_back("ro.adb.secure",          "1");
    // Ensure GMS doesn't see test-keys in the OTA cert list
    out.emplace_back("ro.build.keys",          "release-keys");
}

bool verify_gapps_image(const std::filesystem::path& image_path) {
    // Lightweight check: a GApps-injected image should be at least 3 GB
    // (vanilla AOSP arm64 is ~2 GB; pico GApps adds ~0.5-1 GB).
    // A more thorough check would mount and inspect priv-app.
    std::error_code ec;
    auto sz = std::filesystem::file_size(image_path, ec);
    if (ec) return false;
    // 2.8 GB threshold heuristic
    constexpr uintmax_t GAPPS_SIZE_THRESHOLD = 2'800'000'000ULL;
    return sz >= GAPPS_SIZE_THRESHOLD;
}

} // namespace avm
