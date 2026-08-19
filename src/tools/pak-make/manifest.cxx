//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "manifest.hxx"
#include "error.hxx"

#include <algorithm>
#include <charconv>
#include <format>
#include <string_view>
#include <pak-io/version.hxx>

namespace pm
{
    namespace
    {
        auto parseComponent(std::string_view const text, std::uint32_t& value) -> bool
        {
            auto const [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            return error == std::errc{} && end == text.data() + text.size();
        }
    } // namespace

    auto Manifest::load(std::filesystem::path const& path) -> std::expected<Manifest, std::error_code>
    {
        auto result = toml::parse_file(path.c_str());

        if (!result)
        {
            return std::unexpected(makeErrorCode(ErrorCode::ManifestParseFailed));
        }

        auto const table = std::move(result).table();

        Manifest manifest;

        if (auto const version = table["version"].value<std::string>())
        {
            if (!manifest.parseVersion(*version))
            {
                return std::unexpected(makeErrorCode(ErrorCode::InvalidVersionIdentifier));
            }

            if (manifest._versionMajor < pk::version::major() || manifest._versionMinor < pk::version::minor())
            {
                return std::unexpected(makeErrorCode(ErrorCode::ToolchainVersionTooOld));
            }
        }

        auto const* const files = table["files"].as_array();
        if (!files)
        {
            return std::unexpected(makeErrorCode(ErrorCode::NoFilesArray));
        }

        auto const manifestDirectory = path.parent_path();

        for (auto const& fileEntry : *files)
        {
            auto const pathValue = fileEntry.value<std::string>();
            if (!pathValue || pathValue->empty())
            {
                return std::unexpected(makeErrorCode(ErrorCode::FilePathIsEmpty));
            }

            std::filesystem::path filePath(*pathValue);

            // Relative entries are resolved against the manifest's own directory.
            if (filePath.is_relative())
            {
                filePath = manifestDirectory / filePath;
            }

            manifest._files.emplace_back(std::move(filePath));
        }

        return manifest;
    }

    auto Manifest::parseVersion(std::string const& version) -> bool
    {
        auto const separator = version.find('.');
        if (separator == std::string::npos || version.find('.', separator + 1) != std::string::npos)
        {
            return false;
        }

        std::string_view const major{version.data(), separator};
        std::string_view const minor{version.data() + separator + 1, version.size() - separator - 1};

        if (major.empty() || minor.empty())
        {
            return false;
        }

        auto const isDigits = [](std::string_view const text)
        {
            return std::ranges::all_of(text, [](char const c) { return c >= '0' && c <= '9'; });
        };

        if (!isDigits(major) || !isDigits(minor))
        {
            return false;
        }

        return parseComponent(major, _versionMajor) && parseComponent(minor, _versionMinor);
    }
} // namespace pm
