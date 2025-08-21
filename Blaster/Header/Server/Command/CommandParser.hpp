#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <unordered_map>
#include <optional>
#include <stdexcept>
#include <charconv>
#include <limits>

namespace Blaster::Server::Command
{
    struct CommandArgument
    {
        struct Object;

        using Array = std::vector<CommandArgument>;

        using Map = std::unordered_map<std::string, CommandArgument>;

        struct Object { Map fieldList; };

        using Value = std::variant<std::string, int64_t, double, std::uint8_t, bool, Object, Array>;

        Value value;

        const std::string& AsString() const
        {
            return std::get<std::string>(value);
        }

        int64_t AsInt() const
        {
            return std::get<int64_t>(value);
        }

        double AsFloat() const
        {
            return std::get<double>(value);
        }

        std::uint8_t AsByte() const
        {
            return std::get<std::uint8_t>(value);
        }

        bool AsBool() const
        {
            return std::get<bool>(value);
        }

        const Object& AsObject() const
        {
            return std::get<Object>(value);
        }

        const Array& AsArray() const
        {
            return std::get<Array>(value);
        }

        bool IsString() const
        {
            return std::holds_alternative<std::string>(value);
        }

        bool IsInt() const
        {
            return std::holds_alternative<int64_t>(value);
        }

        bool IsFloat() const
        {
            return std::holds_alternative<double>(value);
        }

        bool IsByte() const
        {
            return std::holds_alternative<std::uint8_t>(value);
        }

        bool IsBool() const
        {
            return std::holds_alternative<bool>(value);
        }

        bool IsObject() const
        {
            return std::holds_alternative<Object>(value);
        }

        bool IsArray() const
        {
            return std::holds_alternative<Array>(value);
        }

        template<typename T>
        const T* Try() const
        {
            return std::get_if<T>(&value);
        }

        const CommandArgument* Find(const std::string& key) const
        {
            if (!IsObject())
                return nullptr;

            const auto& m = AsObject().fieldList;

            auto it = m.find(key);

            return (it == m.end()) ? nullptr : &it->second;
        }
    };

    struct CommandDescriptor
    {
        std::string name;
        std::vector<CommandArgument> argumentList;
    };

    class CommandParser final
    {

    public:

        CommandParser(const CommandParser&) = delete;
        CommandParser(CommandParser&&) = delete;
        CommandParser& operator=(const CommandParser&) = delete;
        CommandParser& operator=(CommandParser&&) = delete;

        static CommandDescriptor ParseLine(const std::string& source)
        {
            CommandParser result;

            result.source = std::string_view{ source };

            return result.ParseCommand();
        }

    private:

        CommandParser() = default;

        [[noreturn]]
        void Fail(const char* message)
        {
            throw std::runtime_error(std::string("Command parse error at ") + std::to_string(currentPosition) + ": " + message);
        }

        bool Done() const
        {
            return currentPosition >= source.size();
        }

        char Peek() const
        {
            return Done() ? '\0' : source[currentPosition];
        }

        char Get()
        {
            return Done() ? '\0' : source[currentPosition++];
        }

        static bool IsBeginningOfIndentation(char character)
        {
            return std::isalpha(static_cast<unsigned char>(character)) || character == '_';
        }

        static bool IsCharacterIndented(char character)
        {
            return std::isalnum(static_cast<unsigned char>(character)) || character == '_';
        }

        void SkipWhitespace()
        {
            while (!Done() && std::isspace(static_cast<unsigned char>(Peek()))) ++currentPosition;
        }

        void ExpectCharacter(char c)
        {
            if (Peek() != c)
                Fail("unexpected character");

            ++currentPosition;
        }

        CommandDescriptor ParseCommand()
        {
            SkipWhitespace();

            if (Peek() == '/')
                ++currentPosition;

            if (!IsBeginningOfIndentation(Peek()))
                Fail("expected command name");

            std::string name;
            name.push_back(Get());

            while (IsCharacterIndented(Peek()))
                name.push_back(Get());

            SkipWhitespace();

            std::vector<CommandArgument> args;

            if (!Done())
            {
                if (Peek() == ',')
                    ++currentPosition;

                SkipWhitespace();

                if (!Done())
                {
                    args.push_back(ParseValue());
                    SkipWhitespace();

                    while (Peek() == ',')
                    {
                        ++currentPosition; SkipWhitespace();
                        args.push_back(ParseValue());
                        SkipWhitespace();
                    }
                }
            }

            return CommandDescriptor{ std::move(name), std::move(args) };
        }

        CommandArgument ParseValue()
        {
            SkipWhitespace();

            const char character = Peek();

            if (character == '"')
                return CommandArgument{ ParseString() };

            if (character == '{')
                return CommandArgument{ ParseObject() };

            if (character == '[')
                return CommandArgument{ ParseArray() };

            if (IsBeginningOfIndentation(character))
            {
                const std::string ident = ParseIdentation();

                if (ident == "true")
                    return CommandArgument{ true };

                if (ident == "false")
                    return CommandArgument{ false };

                return CommandArgument{ ident };
            }

            if (character == '-' || character == '+' || std::isdigit(static_cast<unsigned char>(character)))
                return ParseNumber();

            Fail("expected value");
        }

        std::string ParseIdentation()
        {
            if (!IsBeginningOfIndentation(Peek()))
                Fail("expected identifier");

            std::string result;

            result.push_back(Get());

            while (IsCharacterIndented(Peek()))
                result.push_back(Get());

            return result;
        }

        std::string ParseString()
        {
            ExpectCharacter('"');
            std::string out;

            while (!Done())
            {
                char character = Get();

                if (character == '"')
                    break;

                if (character == '\\')
                {
                    if (Done())
                        Fail("unterminated escape");

                    char e = Get();

                    switch (e)
                    {
                        case '"': out.push_back('"');
                            break;

                        case '\\': out.push_back('\\');
                            break;

                        case 'n': out.push_back('\n');
                            break;

                        case 't': out.push_back('\t');
                            break;

                        case 'r': out.push_back('\r');
                            break;

                        default:
                            Fail("unknown escape sequence");
                    }
                }
                else
                    out.push_back(character);
            }

            return out;
        }

        CommandArgument::Object ParseObject()
        {
            ExpectCharacter('{');
            SkipWhitespace();

            CommandArgument::Object result{};

            if (Peek() == '}')
            {
                ++currentPosition;
                
                return result;
            }

            while (true)
            {
                std::string key;

                if (Peek() == '"')
                    key = ParseString();
                else
                    key = ParseIdentation();

                SkipWhitespace();
                ExpectCharacter('=');
                SkipWhitespace();

                CommandArgument val = ParseValue();

                result.fieldList.emplace(std::move(key), std::move(val));

                SkipWhitespace();

                if (Peek() == ',')
                {
                    ++currentPosition;
                    SkipWhitespace();
                    
                    continue;
                }

                if (Peek() == '}')
                {
                    ++currentPosition;
                    
                    break;
                }

                Fail("expected ',' or '}' in object");
            }

            return result;
        }

        CommandArgument::Array ParseArray()
        {
            ExpectCharacter('[');
            SkipWhitespace();

            CommandArgument::Array result;

            if (Peek() == ']')
            {
                ++currentPosition;
                return result;
            }

            while (true)
            {
                result.emplace_back(ParseValue());
                SkipWhitespace();

                if (Peek() == ',')
                {
                    ++currentPosition;
                    SkipWhitespace();
                    
                    continue;
                }

                if (Peek() == ']')
                {
                    ++currentPosition;
                    break;
                }

                Fail("expected ',' or ']' in array");
            }

            return result;
        }

        CommandArgument ParseNumber()
        {
            const std::size_t start = currentPosition;

            if (Peek() == '+' || Peek() == '-')
                ++currentPosition;

            bool seenDot = false, seenExp = false;

            auto isExp = [](char c)
                {
                    return c == 'e' || c == 'E';
                };

            while (!Done())
            {
                char character = Peek();

                if (std::isdigit(static_cast<unsigned char>(character)))
                {
                    ++currentPosition;
                    continue;
                }

                if (character == '.' && !seenDot && !seenExp)
                {
                    seenDot = true;
                    ++currentPosition;
                    
                    continue;
                }

                if (isExp(character) && !seenExp)
                {
                    seenExp = true;
                    ++currentPosition;

                    if (Peek() == '+' || Peek() == '-')
                        ++currentPosition;

                    continue;
                }

                break;
            }

            char suffix = '\0';

            if (!Done())
            {
                char c = Peek();

                if (c == 'i' || c == 'f' || c == 'b')
                {
                    suffix = c;
                    ++currentPosition;
                }
            }

            std::string_view token = source.substr(start, currentPosition - start - (suffix ? 1 : 0));

            if (token.empty())
                Fail("malformed number");

            if (suffix == 'i')
            {
                int64_t out = 0;

                auto [p, ec] = std::from_chars(token.data(), token.data() + token.size(), out, 10);

                if (ec != std::errc{} || p != token.data() + token.size())
                    Fail("invalid integer literal");

                return CommandArgument{ out };
            }
            if (suffix == 'b')
            {
                unsigned long long tmp = 0;
                auto [p, ec] = std::from_chars(token.data(), token.data() + token.size(), tmp, 10);

                if (ec != std::errc{} || p != token.data() + token.size())
                    Fail("invalid byte literal");

                if (tmp > std::numeric_limits<std::uint8_t>::max())
                    Fail("byte out of range (0..255)");

                return CommandArgument{ static_cast<std::uint8_t>(tmp) };
            }

            if (suffix == 'f')
            {
                double out = 0.0;
                char* end = nullptr;
                out = std::strtod(token.data(), &end);

                if (end != token.data() + token.size())
                    Fail("invalid float literal");

                return CommandArgument{ out };
            }

            if (seenDot || seenExp)
            {
                double out = 0.0;
                char* end = nullptr;

                out = std::strtod(token.data(), &end);

                if (end != token.data() + token.size())
                    Fail("invalid float literal");

                return CommandArgument{ out };
            }
            else
            {
                int64_t out = 0;

                auto [p, ec] = std::from_chars(token.data(), token.data() + token.size(), out, 10);

                if (ec != std::errc{} || p != token.data() + token.size())
                    Fail("invalid integer literal");

                return CommandArgument{ out };
            }
        }

        std::string_view source;
        std::size_t currentPosition = 0;
    };
}