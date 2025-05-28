#pragma once

#include <cstdint>
#include <boost/serialization/shared_ptr.hpp>
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

        template<class Pointer>
        static std::vector<std::uint8_t> ObjectToBytes(const Pointer& pointer)
        {
            std::ostringstream stream;

            boost::archive::text_oarchive archive(stream);

            archive << BOOST_SERIALIZATION_NVP(pointer);

            const auto& buffer = stream.str();

            return {buffer.begin(), buffer.end()};
        }

        template <class Pointer>
        static void ObjectFromBytes(const std::vector<std::uint8_t>& buffer, Pointer& pointer)
        {
            const std::string bufferString(buffer.begin(), buffer.end());

            std::istringstream stream(bufferString);

            boost::archive::text_iarchive archive(stream);

            archive >> BOOST_SERIALIZATION_NVP(pointer);
        }

    private:

        NetworkSerialize() = default;

    };
}
