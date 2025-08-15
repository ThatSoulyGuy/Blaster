#pragma once

#include <string>
#include <memory>
#include <glad/glad.h>
#include <FreeImage.h>
#include "boost/serialization/split_member.hpp"
#include "Independent/ComponentRegistry.hpp"
#include "Independent/ECS/Component.hpp"
#include "Independent/Utility/AssetPath.hpp"

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Utility;

namespace Blaster::Client::Render
{
    class TextureManager;

    class Texture final : public Component
    {

    public:

        Texture(const Texture&) = delete;
        Texture(Texture&&) = delete;
        Texture& operator=(const Texture&) = delete;
        Texture& operator=(Texture&&) = delete;

        [[nodiscard]]
        std::string GetRegistryName() const
        {
            return name;
        }

        [[nodiscard]]
        AssetPath GetPath() const
        {
            return path;
        }

        [[nodiscard]]
        Vector<int, 2> GetDimensions() const
        {
            return dimensions;
        }

        void Bind(const unsigned int slot) const
        {
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(GL_TEXTURE_2D, id);
        }

        void Unbind()
        {
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        void Uninitialize() const
        {
            glDeleteTextures(1, &id);
        }

        static std::shared_ptr<Texture> Create(const std::string& name, const AssetPath& path)
        {
            std::shared_ptr<Texture> result(new Texture());

            result->name = name;
            result->path = path;

            result->Generate();

            return result;
        }

        static std::shared_ptr<Texture> CreateFromMemory(const std::string& name, const Vector<std::uint32_t, 2>& dimensions, const std::vector<std::uint8_t>& data, bool generateMipMaps = true, GLenum internalFormat = GL_R8, GLenum sourceFormat = GL_RED)
        {
            std::shared_ptr<Texture> result(new Texture());

            result->name = name;
            result->path = { "!", "!" };

            result->GenerateData(dimensions, data.data(), generateMipMaps, internalFormat, sourceFormat);

            glBindTexture(GL_TEXTURE_2D, 0);

            return result;
        }

    private:

        Texture() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;
        friend class Blaster::Client::Render::TextureManager;

        template <typename Archive>
        void save(Archive& archive, const unsigned) const
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(name);
            archive & BOOST_SERIALIZATION_NVP(path);
            archive & BOOST_SERIALIZATION_NVP(isRegistered);

            if (isRegistered)
                archive & BOOST_SERIALIZATION_NVP(id);
        }

        template <typename Archive>
        void load(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(name);
            archive & BOOST_SERIALIZATION_NVP(path);
            archive & BOOST_SERIALIZATION_NVP(isRegistered);

            if (isRegistered)
                archive & BOOST_SERIALIZATION_NVP(id);
        }

        BOOST_SERIALIZATION_SPLIT_MEMBER()

        static void FreeImageErrorHandler(const FREE_IMAGE_FORMAT format, const char* message)
        {
            fprintf(stderr, "FreeImage-error (%s): %s\n",
                    format != FIF_UNKNOWN ? FreeImage_GetFormatFromFIF(format) : "Unknown",
                    message);
        }

        void Generate()
        {
            const std::string fullPath = path.GetFullPath();

            FreeImage_Initialise();
            FreeImage_SetOutputMessage(FreeImageErrorHandler);

            FIBITMAP* bitmap = FreeImage_Load(FIF_PNG, fullPath.c_str(), PNG_DEFAULT);

            if (!bitmap)
                throw std::runtime_error(std::format("Failed to load texture '{}'!", name));

            FIBITMAP* image = FreeImage_ConvertTo32Bits(bitmap);

            FreeImage_FlipVertical(image);

            FreeImage_Unload(bitmap);

            const std::uint32_t width = FreeImage_GetWidth(image);
            const std::uint32_t height = FreeImage_GetHeight(image);
            const void* pixels = FreeImage_GetBits(image);

            GenerateData({ width, height }, pixels, true, GL_RGBA8, GL_BGRA);

            FreeImage_Unload(image);
            
            glBindTexture(GL_TEXTURE_2D, 0);

            FreeImage_DeInitialise();
        }

        void GenerateData(const Vector<std::uint32_t, 2>& dimensions, const void* data, bool generateMipMaps = true, GLenum internalFormat = GL_RGBA8, GLenum sourceFormat = GL_RGBA)
        {
            this->dimensions = dimensions;

            if (sourceFormat == GL_RED || sourceFormat == GL_LUMINANCE)
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);

            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, dimensions.x(), dimensions.y(), 0, sourceFormat, GL_UNSIGNED_BYTE, data);

            if (generateMipMaps)
            {
                glGenerateMipmap(GL_TEXTURE_2D);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            }
            else
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }

        bool isRegistered = false;

        std::string name;
        AssetPath path;
        Vector<int, 2> dimensions;

        unsigned int id = 0;

        DESCRIBE_AND_REGISTER(Texture, (Component), (), (), (isRegistered, name, path, id))
    };
}

REGISTER_COMPONENT(Blaster::Client::Render::Texture, 39842)