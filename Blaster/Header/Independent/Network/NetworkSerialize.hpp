#pragma once

#include <cstdint>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

namespace Blaster::Independent::Network
{
    class NetworkSerialize final
    {

    public:

        NetworkSerialize(const NetworkSerialize&) = delete;
        NetworkSerialize(NetworkSerialize&&) = delete;
        NetworkSerialize& operator=(const NetworkSerialize&) = delete;
        NetworkSerialize& operator=(NetworkSerialize&&) = delete;

        template <class T>
        static std::vector<std::uint8_t> ObjectToBytes(const T& object)
        {
            std::ostringstream stream;

            boost::archive::text_oarchive archive(stream);

            archive << object;

            const auto& buffer = stream.str();

            return { buffer.begin(), buffer.end() };
        }

        template <class T>
        static void ObjectFromBytes(const std::vector<std::uint8_t>& byteList, T& object)
        {
            const std::string buffer(reinterpret_cast<const char*>(byteList.data()), byteList.size());

            std::istringstream stream(buffer);

            boost::archive::text_iarchive archive(stream);

            archive >> object;
        }

    private:

        NetworkSerialize() = default;

    };
}
