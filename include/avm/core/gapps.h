#pragma once
#include <filesystem>
#include <string>

namespace avm {

/**
 * GAppsConfig — runtime configuration for Google Play services support.
 *
 * When gapps_enabled is true, AVM will:
 *   1. Verify the system image contains the expected priv-app GApps directories.
 *   2. Pass additional QEMU -prop flags to ensure ro.build.tags=release-keys
 *      and other CTS-passing properties are set at boot time.
 *   3. Expose /dev/binder and /dev/ashmem with the permissions GMS Core expects.
 *   4. Reserve extra RAM headroom (gapps needs ~512 MB above base Android).
 */
struct GAppsConfig {
    bool gapps_enabled = false;

    // Path to a JSON fingerprint file for Play Integrity spoofing.
    // If empty, uses the built-in Pixel 8 Pro fingerprint.
    std::filesystem::path fingerprint_file;

    // Whether to apply SafetyNet property overrides at VM boot via -prop.
    bool spoof_fingerprint = true;

    // Which safety profile to spoof.
    // "pixel8pro" = Google Pixel 8 Pro (AD1A.240905.004) — default
    // "pixel7"    = Google Pixel 7  (TP1A.221005.002)
    std::string spoof_profile = "pixel8pro";

    // Extra RAM to reserve for GMS Core (MB). Added on top of profile base RAM.
    int extra_ram_mb = 512;
};

/**
 * Fingerprint entry for a specific certified device profile.
 */
struct DeviceFingerprint {
    std::string brand;
    std::string device;
    std::string manufacturer;
    std::string model;
    std::string name;
    std::string fingerprint;
    std::string description;
    std::string version_release;
    int         version_sdk = 34;
};

/**
 * Returns the built-in spoofing fingerprint for the given profile name.
 * Throws std::invalid_argument if profile is unknown.
 */
DeviceFingerprint get_builtin_fingerprint(const std::string& profile);

/**
 * Loads a fingerprint from a JSON file.
 * JSON schema:
 * {
 *   "brand": "google",
 *   "device": "husky",
 *   "manufacturer": "Google",
 *   "model": "Pixel 8 Pro",
 *   "name": "husky",
 *   "fingerprint": "google/husky/husky:14/AD1A.240905.004/12117344:user/release-keys",
 *   "description": "husky-user 14 AD1A.240905.004 12117344 release-keys",
 *   "version_release": "14",
 *   "version_sdk": 34
 * }
 */
DeviceFingerprint load_fingerprint_file(const std::filesystem::path& path);

/**
 * Builds the list of QEMU -prop arguments needed to spoof the device identity
 * at VM boot time. These are passed as:
 *   -prop ro.product.model="Pixel 8 Pro" -prop ro.build.fingerprint="..." ...
 *
 * @param fp     The fingerprint to apply.
 * @param out    Output vector — new -prop pairs are appended.
 */
void build_gapps_qemu_props(
    const DeviceFingerprint& fp,
    std::vector<std::pair<std::string, std::string>>& out
);

/**
 * Verifies that the system image at path appears to contain GApps
 * (i.e., has Phonesky.apk / PrebuiltGmsCore.apk in priv-app).
 * Returns true if GApps are detected, false otherwise.
 *
 * Note: this is a lightweight heuristic check (file size + presence),
 * not a cryptographic verification.
 */
bool verify_gapps_image(const std::filesystem::path& image_path);

} // namespace avm
