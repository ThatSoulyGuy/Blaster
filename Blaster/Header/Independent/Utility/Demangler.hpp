#pragma once

#include <string>

#if defined(__GNUG__)
#include <cxxabi.h>
#elif defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")

#undef min
#undef max
#undef ERROR
#endif

namespace Blaster::Independent::Utility
{
	class Demangler final
	{

    public:

        Demangler(const Demangler&) = delete;
        Demangler(Demangler&&) = delete;
        Demangler& operator=(const Demangler&) = delete;
        Demangler& operator=(Demangler&&) = delete;

        static std::string Demangle(const char* raw)
        {
#if defined(__GNUG__)
            int status = 0;

            std::unique_ptr<char, void(*)(void*)> p{ abi::__cxa_demangle(raw, nullptr, nullptr, &status), std::free };

            return status == 0 ? std::string{ p.get() } : raw;
#elif defined(_MSC_VER)
            char buf[1024];

            if (UnDecorateSymbolName(raw, buf, sizeof(buf), UNDNAME_NAME_ONLY))
                return buf;

            return raw;
#else
            return raw;
#endif
        }

    private:

        Demangler() = default;

	};
}