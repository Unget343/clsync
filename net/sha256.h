#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace clsync::crypto {

std::optional<std::string> sha256_file(const std::filesystem::path& path);

} // namespace clsync::crypto
