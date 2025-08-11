#pragma once

#include <string>
#include <format>
#include <utility>
#include <filesystem>
#include <boost/serialization/access.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/string.hpp>

#ifdef __APPLE__
  #include <mach-o/dyld.h>
#endif

inline std::string GetExecutableDirectory()
{
#ifdef _WIN32
    return "./";
#elif defined(__APPLE__)
    char buffer[PATH_MAX];

    uint32_t size = sizeof(buffer);

    if (_NSGetExecutablePath(buffer, &size) == 0)
    {
        const std::string fullPath(buffer);

        return fullPath.substr(0, fullPath.find_last_of('/')) + "/";
    }

    throw std::runtime_error("Failed to get exe path");
#elif defined(__unix__)
    return "./";
#endif
}

namespace Blaster::Independent::Utility
{
    class AssetPath
    {

    public:

        AssetPath(std::string domain, std::string localPath) : domain(std::move(domain)), localPath(std::move(localPath)) { }

        AssetPath() = default;

        [[nodiscard]]
        std::string GetDomain() const
        {
            return domain;
        }

        [[nodiscard]]
        std::string GetLocalPath() const
        {
            return localPath;
        }

        [[nodiscard]]
        std::string GetFullPath() const
        {
            return std::format("{}Assets/{}/{}", GetExecutableDirectory(), domain, localPath);
        }

    private:

        friend class boost::serialization::access;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::make_nvp("domain", domain);
            archive & boost::serialization::make_nvp("localPath", localPath);
        }
        
        std::string domain;
        std::string localPath;

    };

    [[nodiscard]]
    inline bool operator==(const AssetPath& lhs, const AssetPath& rhs) noexcept
    {
        return lhs.GetDomain() == rhs.GetDomain() && lhs.GetLocalPath() == rhs.GetLocalPath();
    }

    [[nodiscard]]
    inline bool operator!=(const AssetPath& lhs, const AssetPath& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    [[nodiscard]]
    inline bool operator<(const AssetPath& a, const AssetPath& b) noexcept
    {
        if (a.GetDomain() != b.GetDomain())
            return a.GetDomain() < b.GetDomain();

        return a.GetLocalPath() < b.GetLocalPath();
    }
}

namespace std
{
    template<>
    struct hash<Blaster::Independent::Utility::AssetPath>
    {
        size_t operator()(const Blaster::Independent::Utility::AssetPath& p) const noexcept
        {
            const size_t h1 = std::hash<std::string>{}(p.GetDomain());
            const size_t h2 = std::hash<std::string>{}(p.GetLocalPath());

            return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
        }
    };
}

BOOST_CLASS_EXPORT(Blaster::Independent::Utility::AssetPath)